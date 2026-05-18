/**
 * @file live_captioner.cpp
 * @brief Implementation of real-time live captioning engine
 */

#ifdef ENABLE_WHISPER

    #include "audio/live_captioner.h"

    #include "utils/logger.h"

    #include <algorithm>
    #include <chrono>
    #include <cmath>
    #include <numeric>
    #include <thread>

namespace ffvoice {

// ============================================================================
// Construction / Destruction
// ============================================================================

LiveCaptioner::LiveCaptioner(const LiveCaptionerConfig& config)
    : config_(config),
      whisper_(config.whisper),
      vad_(config.vad),
      ring_buffer_(config.ring_buffer_capacity) {
    // Pre-allocate accumulation buffer to avoid repeated allocations
    accumulation_buffer_.reserve(config_.vad.max_segment_samples);

    // Apply the suppress_whisper_progress flag to the embedded WhisperConfig so
    // the real WhisperProcessor respects it even though config_.whisper is a
    // copy and WhisperProcessor was already constructed from it above.  The
    // field is used in GetDefaultParams() which is called at inference time, so
    // patching it here is sufficient.
    config_.whisper.print_progress = !config_.suppress_whisper_progress;
}

LiveCaptioner::~LiveCaptioner() {
    Stop();
}

// ============================================================================
// Public API
// ============================================================================

bool LiveCaptioner::Initialize() {
    if (initialized_) {
        LOG_WARNING("LiveCaptioner: already initialized");
        return true;
    }

    // If a test-seam transcription function is provided, skip loading the real
    // Whisper model — this allows unit testing without a model file.
    if (config_.transcribe_fn) {
        LOG_INFO("LiveCaptioner: using test-seam transcribe_fn, skipping model load");
        initialized_ = true;
        return true;
    }

    // Apply the suppress flag (constructor copy may have been made before the
    // user set it; apply it now just before initialisation).
    config_.whisper.print_progress = !config_.suppress_whisper_progress;

    if (!whisper_.Initialize()) {
        last_error_ =
            "LiveCaptioner: WhisperProcessor initialization failed: " + whisper_.GetLastError();
        LOG_ERROR("%s", last_error_.c_str());
        return false;
    }

    initialized_ = true;
    LOG_INFO("LiveCaptioner: initialized (ring_buffer=%zu, partial_interval=%dms)",
             config_.ring_buffer_capacity, config_.partial_interval_ms);
    return true;
}

void LiveCaptioner::SetCallback(CaptionCallback callback) {
    callback_ = std::move(callback);
}

bool LiveCaptioner::Start() {
    if (!initialized_) {
        last_error_ = "LiveCaptioner: Start() called before Initialize()";
        LOG_ERROR("%s", last_error_.c_str());
        return false;
    }

    if (running_.load(std::memory_order_acquire)) {
        LOG_WARNING("LiveCaptioner: already running");
        return true;
    }

    running_.store(true, std::memory_order_release);
    worker_thread_ = std::thread(&LiveCaptioner::WorkerLoop, this);

    LOG_INFO("LiveCaptioner: worker thread started");
    return true;
}

void LiveCaptioner::Stop() {
    if (!running_.load(std::memory_order_acquire)) {
        return;
    }

    running_.store(false, std::memory_order_release);

    if (worker_thread_.joinable()) {
        worker_thread_.join();
    }

    // Flush any audio that is still buffered in the VAD segmenter.  This is
    // done on the calling thread (the worker has already exited), so we are
    // the only writer to accumulation_buffer_ at this point.
    auto on_segment = [this](const int16_t* seg_samples, size_t seg_count) {
        std::vector<TranscriptionSegment> segments;
        bool ok = Transcribe(seg_samples, seg_count, segments);

        if (callback_) {
            CaptionEvent ev;
            ev.type = CaptionEventType::Final;
            ev.utterance_id = utterance_id_++;
            ev.confidence = ok ? MeanConfidence(segments) : 0.0f;
            ev.text = ok ? JoinText(segments) : "";
            ev.utterance_start_ms = (!ok || segments.empty()) ? 0 : segments.front().start_ms;
            ev.utterance_end_ms = (!ok || segments.empty()) ? 0 : segments.back().end_ms;
            callback_(ev);
        }

        accumulation_buffer_.clear();
    };

    vad_.Flush(on_segment);

    LOG_INFO("LiveCaptioner: stopped");
}

size_t LiveCaptioner::FeedAudio(const int16_t* samples, size_t count) {
    if (!samples || count == 0) {
        return 0;
    }
    return ring_buffer_.push_bulk(samples, count);
}

bool LiveCaptioner::IsRunning() const {
    return running_.load(std::memory_order_acquire);
}

std::string LiveCaptioner::GetLastError() const {
    return last_error_;
}

// ============================================================================
// Worker thread
// ============================================================================

void LiveCaptioner::WorkerLoop() {
    // Batch size: ~100 ms at 48 kHz (10 ms chunks typical for VAD)
    static constexpr size_t kBatchSize = 4800;
    std::vector<int16_t> batch(kBatchSize);

    auto last_partial_time = std::chrono::steady_clock::now();

    // Build the VAD on_segment callback.  This closure is called from within
    // ProcessFrame() (on the worker thread) whenever end-of-speech is detected.
    auto on_segment = [this](const int16_t* seg_samples, size_t seg_count) {
        std::vector<TranscriptionSegment> segments;
        bool ok = Transcribe(seg_samples, seg_count, segments);

        if (callback_) {
            CaptionEvent ev;
            ev.type = CaptionEventType::Final;
            ev.utterance_id = utterance_id_++;
            ev.confidence = ok ? MeanConfidence(segments) : 0.0f;
            ev.text = ok ? JoinText(segments) : "";
            ev.utterance_start_ms = (!ok || segments.empty()) ? 0 : segments.front().start_ms;
            ev.utterance_end_ms = (!ok || segments.empty()) ? 0 : segments.back().end_ms;
            callback_(ev);
        }

        // Clear the worker-local accumulation buffer for the next utterance
        accumulation_buffer_.clear();
        partial_in_flight_.store(false, std::memory_order_release);
    };

    while (running_.load(std::memory_order_acquire)) {
        size_t n = ring_buffer_.pop_bulk(batch.data(), kBatchSize);

        if (n == 0) {
            // Nothing in the ring; yield to avoid a tight spin
            std::this_thread::sleep_for(std::chrono::milliseconds(10));
            continue;
        }

        // ----------------------------------------------------------------
        // Compute VAD probability for this batch
        // ----------------------------------------------------------------
        float vad_prob = 0.0f;
        if (config_.vad_prob_source) {
            vad_prob = config_.vad_prob_source();
        } else {
            // Built-in RMS estimator: map RMS → [0, 1] with a 3000-count knee
            double sum_sq = 0.0;
            for (size_t i = 0; i < n; ++i) {
                double s = static_cast<double>(batch[i]);
                sum_sq += s * s;
            }
            float rms = static_cast<float>(std::sqrt(sum_sq / static_cast<double>(n)));
            vad_prob = std::clamp(rms / 3000.0f, 0.0f, 1.0f);
        }

        // ----------------------------------------------------------------
        // Feed samples into the VAD segmenter
        // ----------------------------------------------------------------
        vad_.ProcessFrame(batch.data(), n, vad_prob, on_segment);

        // ----------------------------------------------------------------
        // Accumulate samples while in speech (worker-local, no mutex)
        // ----------------------------------------------------------------
        if (vad_.IsInSpeech()) {
            accumulation_buffer_.insert(accumulation_buffer_.end(), batch.data(), batch.data() + n);
        }

        // ----------------------------------------------------------------
        // Partial caption emission
        // ----------------------------------------------------------------
        auto now = std::chrono::steady_clock::now();
        auto elapsed_ms =
            std::chrono::duration_cast<std::chrono::milliseconds>(now - last_partial_time).count();

        if (vad_.IsInSpeech() &&
            elapsed_ms >= static_cast<long long>(config_.partial_interval_ms) &&
            accumulation_buffer_.size() >= config_.min_samples_for_partial &&
            !partial_in_flight_.load(std::memory_order_acquire)) {
            partial_in_flight_.store(true, std::memory_order_release);
            last_partial_time = now;

            // Transcribe a copy of the accumulation buffer
            std::vector<int16_t> partial_copy = accumulation_buffer_;
            std::vector<TranscriptionSegment> segments;
            Transcribe(partial_copy.data(), partial_copy.size(), segments);

            if (callback_) {
                CaptionEvent ev;
                ev.type = CaptionEventType::Partial;
                ev.utterance_id = utterance_id_;  // Same ID as ongoing utterance
                ev.confidence = 0.0f;             // Confidence not meaningful for Partial
                ev.text = JoinText(segments);
                ev.utterance_start_ms = segments.empty() ? 0 : segments.front().start_ms;
                ev.utterance_end_ms = segments.empty() ? 0 : segments.back().end_ms;
                callback_(ev);
            }

            partial_in_flight_.store(false, std::memory_order_release);
        }
    }

    LOG_INFO("LiveCaptioner: worker thread exiting");
}

// ============================================================================
// Private helpers
// ============================================================================

bool LiveCaptioner::Transcribe(const int16_t* samples, size_t num_samples,
                               std::vector<TranscriptionSegment>& segments) {
    if (config_.transcribe_fn) {
        return config_.transcribe_fn(samples, num_samples, segments);
    }
    return whisper_.TranscribeBuffer(samples, num_samples, segments);
}

float LiveCaptioner::MeanConfidence(const std::vector<TranscriptionSegment>& segments) {
    if (segments.empty()) {
        return 0.0f;
    }
    float sum = 0.0f;
    for (const auto& seg : segments) {
        sum += seg.confidence;
    }
    return sum / static_cast<float>(segments.size());
}

std::string LiveCaptioner::JoinText(const std::vector<TranscriptionSegment>& segments) {
    std::string result;
    for (const auto& seg : segments) {
        if (!result.empty() && !seg.text.empty() && seg.text.front() != ' ') {
            result += ' ';
        }
        result += seg.text;
    }
    return result;
}

}  // namespace ffvoice

#endif  // ENABLE_WHISPER
