/**
 * @file diarizer.cpp
 * @brief Speaker diarization implementation (sherpa-onnx offline C API)
 */

#include "audio/diarizer.h"

#include "utils/logger.h"

#include <algorithm>
#include <cstring>

namespace ffvoice {

// =============================================================================
// MergeIntoSegments — unconditionally compiled (no sherpa-onnx dependency)
// =============================================================================

void MergeIntoSegments(std::vector<TranscriptionSegment>& segments,
                       const std::vector<SpeakerSegment>& speakers) {
    if (speakers.empty()) {
        return;
    }

    for (auto& seg : segments) {
        int64_t best_overlap = 0;
        int32_t best_speaker = -1;

        for (const auto& sp : speakers) {
            // Millisecond overlap between the transcription and speaker spans.
            const int64_t overlap =
                std::min(seg.end_ms, sp.end_ms) - std::max(seg.start_ms, sp.start_ms);

            // Strict '>' so that, on a tie, the earlier (lower-index) speaker
            // segment is kept.
            if (overlap > best_overlap) {
                best_overlap = overlap;
                best_speaker = sp.speaker_id;
            }
        }

        if (best_overlap > 0) {
            seg.speaker_id = best_speaker;
        }
        // Otherwise leave seg.speaker_id unchanged (default -1).
    }
}

#ifdef ENABLE_DIARIZATION

// =============================================================================
// Diarizer — gated by ENABLE_DIARIZATION
// =============================================================================

Diarizer::Diarizer(const DiarizerConfig& config) : config_(config) {
    LOG_INFO("Diarizer created");
}

Diarizer::~Diarizer() {
    if (handle_) {
        SherpaOnnxDestroyOfflineSpeakerDiarization(handle_);
        handle_ = nullptr;
    }
    LOG_INFO("Diarizer destroyed");
}

bool Diarizer::Init() {
    // Idempotent: a second successful Init() is a no-op.
    if (initialized_) {
        return true;
    }

    // Test-seam mode: no model files, no sherpa-onnx handle needed.
    if (config_.diarize_fn) {
        initialized_ = true;
        LOG_INFO("Diarizer initialized in test-seam mode (diarize_fn set)");
        return true;
    }

    if (config_.segmentation_model_path.empty()) {
        last_error_ = "Diarizer: segmentation_model_path is empty";
        LOG_ERROR("%s", last_error_.c_str());
        return false;
    }
    if (config_.embedding_model_path.empty()) {
        last_error_ = "Diarizer: embedding_model_path is empty";
        LOG_ERROR("%s", last_error_.c_str());
        return false;
    }

    SherpaOnnxOfflineSpeakerDiarizationConfig sd_config;
    memset(&sd_config, 0, sizeof(sd_config));

    sd_config.segmentation.pyannote.model = config_.segmentation_model_path.c_str();
    sd_config.segmentation.num_threads = config_.num_threads;
    sd_config.segmentation.provider = "cpu";

    sd_config.embedding.model = config_.embedding_model_path.c_str();
    sd_config.embedding.num_threads = config_.num_threads;
    sd_config.embedding.provider = "cpu";

    // num_clusters > 0 uses a fixed speaker count; otherwise fall back to the
    // distance threshold (sherpa-onnx ignores threshold when num_clusters > 0).
    if (config_.num_speakers > 0) {
        sd_config.clustering.num_clusters = config_.num_speakers;
        sd_config.clustering.threshold = 0.0f;
    } else {
        sd_config.clustering.num_clusters = 0;
        sd_config.clustering.threshold = config_.cluster_threshold;
    }

    handle_ = SherpaOnnxCreateOfflineSpeakerDiarization(&sd_config);
    if (!handle_) {
        last_error_ =
            "Diarizer: SherpaOnnxCreateOfflineSpeakerDiarization failed "
            "(check segmentation/embedding model paths)";
        LOG_ERROR("%s", last_error_.c_str());
        return false;
    }

    initialized_ = true;
    LOG_INFO("Diarizer initialized: segmentation='%s', embedding='%s', sample_rate=%d Hz",
             config_.segmentation_model_path.c_str(), config_.embedding_model_path.c_str(),
             SherpaOnnxOfflineSpeakerDiarizationGetSampleRate(handle_));
    return true;
}

std::vector<SpeakerSegment> Diarizer::Diarize(const std::vector<float>& samples, int sample_rate) {
    std::vector<SpeakerSegment> result;

    if (!initialized_) {
        LOG_ERROR("Diarizer::Diarize called before successful Init()");
        return result;
    }
    if (samples.empty()) {
        return result;
    }

    // Test-seam mode: delegate to the injected function.
    if (config_.diarize_fn) {
        return config_.diarize_fn(samples, sample_rate);
    }

    const int expected_rate = SherpaOnnxOfflineSpeakerDiarizationGetSampleRate(handle_);
    if (sample_rate != expected_rate) {
        last_error_ = "Diarizer: sample rate mismatch";
        LOG_ERROR("Diarizer::Diarize: sample rate %d Hz does not match expected %d Hz", sample_rate,
                  expected_rate);
        return result;
    }

    const SherpaOnnxOfflineSpeakerDiarizationResult* sd_result =
        SherpaOnnxOfflineSpeakerDiarizationProcess(handle_, samples.data(),
                                                   static_cast<int32_t>(samples.size()));
    if (!sd_result) {
        last_error_ = "Diarizer: SherpaOnnxOfflineSpeakerDiarizationProcess returned null";
        LOG_ERROR("%s", last_error_.c_str());
        return result;
    }

    const int32_t num_segments = SherpaOnnxOfflineSpeakerDiarizationResultGetNumSegments(sd_result);
    const SherpaOnnxOfflineSpeakerDiarizationSegment* segments =
        SherpaOnnxOfflineSpeakerDiarizationResultSortByStartTime(sd_result);

    if (segments && num_segments > 0) {
        result.reserve(static_cast<size_t>(num_segments));
        for (int32_t i = 0; i < num_segments; ++i) {
            // sherpa-onnx reports start/end in seconds; convert to milliseconds.
            const int64_t start_ms = static_cast<int64_t>(segments[i].start * 1000.0f);
            const int64_t end_ms = static_cast<int64_t>(segments[i].end * 1000.0f);
            result.emplace_back(start_ms, end_ms, segments[i].speaker);
        }
    }

    // Free the C objects: the segment array first, then the result.
    if (segments) {
        SherpaOnnxOfflineSpeakerDiarizationDestroySegment(segments);
    }
    SherpaOnnxOfflineSpeakerDiarizationDestroyResult(sd_result);

    LOG_INFO("Diarizer::Diarize produced %zu speaker segment(s)", result.size());
    return result;
}

bool Diarizer::IsInitialized() const {
    return initialized_;
}

std::string Diarizer::GetLastError() const {
    return last_error_;
}

int Diarizer::GetExpectedSampleRate() const {
    // In test-seam mode (or before Init) there is no handle; report the
    // sherpa-onnx diarization model rate, which is 16 kHz.
    if (handle_) {
        return SherpaOnnxOfflineSpeakerDiarizationGetSampleRate(handle_);
    }
    return 16000;
}

#endif  // ENABLE_DIARIZATION

}  // namespace ffvoice
