/**
 * @file live_captioner.h
 * @brief Real-time live captioning using VAD segmentation and Whisper ASR
 *
 * LiveCaptioner connects a lock-free ring buffer (for audio ingestion from any
 * thread) to a VAD-driven segmenter and Whisper ASR back-end running on a
 * dedicated worker thread.  Partial captions are emitted at a configurable
 * interval while speech is ongoing; a Final caption is emitted at the end of
 * each detected utterance.
 */

#pragma once

#ifdef ENABLE_WHISPER

    #include "audio/vad_segmenter.h"
    #include "audio/whisper_processor.h"
    #include "utils/ring_buffer.h"

    #include <atomic>
    #include <cstddef>
    #include <cstdint>
    #include <functional>
    #include <string>
    #include <thread>
    #include <vector>

namespace ffvoice {

// ============================================================================
// Caption event types
// ============================================================================

/**
 * @brief Distinguishes mid-utterance (Partial) from end-of-utterance (Final)
 *        caption events.
 */
enum class CaptionEventType {
    Partial,  ///< Intermediate caption emitted while speech is ongoing
    Final     ///< Final caption emitted after end-of-speech detection
};

/**
 * @brief A single caption event delivered to the caller via CaptionCallback.
 */
struct CaptionEvent {
    CaptionEventType type;       ///< Partial or Final
    std::string text;            ///< Transcribed text (may be empty for short utterances)
    int64_t utterance_start_ms;  ///< Estimated utterance start (ms, from Whisper segments)
    int64_t utterance_end_ms;    ///< Estimated utterance end  (ms, from Whisper segments)
    float confidence;            ///< Mean segment confidence (0.0 for Partial; mean for Final)
    uint32_t utterance_id;       ///< Monotonically incrementing utterance counter
};

// ============================================================================
// Configuration
// ============================================================================

/**
 * @brief Configuration bundle for LiveCaptioner.
 *
 * All fields have sensible defaults for 48 kHz mono input with an English
 * Whisper tiny model.  Override as needed before passing to the constructor.
 */
struct LiveCaptionerConfig {
    /// Whisper ASR configuration (model path, language, threads, …)
    WhisperConfig whisper;

    /// VAD segmentation configuration (thresholds, min frames, …)
    VADSegmenter::Config vad;

    /// Interval between Partial caption attempts while in speech (ms)
    int partial_interval_ms = 500;

    /// Minimum accumulated samples before a Partial is emitted
    size_t min_samples_for_partial = 16000;

    /// Ring buffer capacity in samples (default ≈ 3 s at 48 kHz)
    size_t ring_buffer_capacity = 144000;

    /// Input audio sample rate (Hz)
    int sample_rate = 48000;

    /// Input audio channel count (1 = mono, 2 = stereo)
    int channels = 1;

    /**
     * @brief Optional VAD probability source.
     *
     * When set, the worker thread calls this function once per processing
     * batch to obtain a voice-activity probability in [0, 1].  When nullptr,
     * a built-in RMS-based estimator is used instead.
     */
    std::function<float()> vad_prob_source = nullptr;

    /// When true, Whisper's built-in progress messages are silenced.
    bool suppress_whisper_progress = true;

    /**
     * @brief Test-seam for the transcription back-end.
     *
     * When set, the worker thread calls this function instead of
     * WhisperProcessor::TranscribeBuffer.  Signature mirrors TranscribeBuffer:
     *   bool fn(const int16_t* samples, size_t count, vector<TranscriptionSegment>& out)
     *
     * This allows full unit-testing of LiveCaptioner without a real Whisper
     * model loaded.
     */
    std::function<bool(const int16_t*, size_t, std::vector<TranscriptionSegment>&)> transcribe_fn =
        nullptr;
};

// ============================================================================
// Callback alias
// ============================================================================

/**
 * @brief Callback type invoked from the worker thread on each caption event.
 *
 * @note The callback is called from the internal worker thread; it must be
 *       thread-safe and should return quickly to avoid stalling transcription.
 */
using CaptionCallback = std::function<void(const CaptionEvent&)>;

// ============================================================================
// LiveCaptioner class
// ============================================================================

/**
 * @brief Real-time live captioning engine.
 *
 * Typical usage:
 * @code
 * LiveCaptionerConfig cfg;
 * cfg.whisper.model_path = "/path/to/ggml-tiny.bin";
 * cfg.whisper.language   = "en";
 *
 * LiveCaptioner captioner(cfg);
 * captioner.SetCallback([](const CaptionEvent& ev) {
 *     std::cout << (ev.type == CaptionEventType::Final ? "[F] " : "[P] ")
 *               << ev.text << "\n";
 * });
 *
 * if (!captioner.Initialize()) { ... }
 * captioner.Start();
 *
 * // Feed audio from any thread:
 * captioner.FeedAudio(pcm, sample_count);
 *
 * captioner.Stop();  // Flushes pending Final caption
 * @endcode
 *
 * Threading model:
 * - FeedAudio() is the producer; it writes to a lock-free ring buffer and
 *   returns immediately — safe to call from a real-time audio callback.
 * - A single worker thread drains the ring buffer, runs VAD, and serialises
 *   all Whisper calls (whisper_context is not re-entrant).
 * - CaptionCallback is invoked from the worker thread.
 */
class LiveCaptioner {
public:
    /**
     * @brief Construct a LiveCaptioner with the given configuration.
     * @param config Configuration parameters (copied internally).
     */
    explicit LiveCaptioner(const LiveCaptionerConfig& config);

    /**
     * @brief Destructor — calls Stop() if still running.
     */
    ~LiveCaptioner();

    // Non-copyable, non-movable (owns a thread and atomic state)
    LiveCaptioner(const LiveCaptioner&) = delete;
    LiveCaptioner& operator=(const LiveCaptioner&) = delete;

    /**
     * @brief Load the Whisper model and prepare internal state.
     *
     * Must be called before Start().  Skipped when transcribe_fn is set
     * (test-seam mode) — Initialize() still returns true in that case.
     *
     * @return true on success; false on failure (see GetLastError()).
     */
    bool Initialize();

    /**
     * @brief Register the callback invoked on each CaptionEvent.
     *
     * May be called before or after Initialize(), but must be called before
     * Start() to receive any events.
     *
     * @param callback Function called from the worker thread on each event.
     */
    void SetCallback(CaptionCallback callback);

    /**
     * @brief Start the worker thread and begin processing audio.
     *
     * Initialize() must have succeeded before calling Start().
     *
     * @return true if the worker thread was successfully launched.
     */
    bool Start();

    /**
     * @brief Stop the worker thread and flush any buffered audio.
     *
     * Blocks until the worker thread has joined.  A Final event is emitted for
     * any audio that was buffered but not yet transcribed at the time Stop() is
     * called.  Safe to call multiple times.
     */
    void Stop();

    /**
     * @brief Feed raw audio samples into the ring buffer.
     *
     * Lock-free and non-blocking — safe for real-time audio callbacks.
     * Returns the number of samples actually written; if the ring buffer is
     * full, the remaining samples are dropped and the caller should retry later.
     *
     * @param samples Pointer to int16_t PCM samples.
     * @param count   Number of samples (total, not per channel).
     * @return        Number of samples written (0..count).
     */
    size_t FeedAudio(const int16_t* samples, size_t count);

    /**
     * @brief Query whether the worker thread is currently running.
     * @return true between a successful Start() and Stop().
     */
    bool IsRunning() const;

    /**
     * @brief Return the last error message (empty string if no error).
     * @return Human-readable error description.
     */
    std::string GetLastError() const;

private:
    // -------------------------------------------------------------------------
    // Internal helpers
    // -------------------------------------------------------------------------

    /**
     * @brief Worker thread entry point.
     */
    void WorkerLoop();

    /**
     * @brief Invoke the transcription back-end (test seam or real Whisper).
     * @param samples     PCM samples to transcribe.
     * @param num_samples Number of samples.
     * @param segments    Output transcription segments.
     * @return true on success.
     */
    bool Transcribe(const int16_t* samples, size_t num_samples,
                    std::vector<TranscriptionSegment>& segments);

    /**
     * @brief Compute the mean confidence across a segment list.
     * @param segments Non-empty segment vector.
     * @return Mean of segment confidences, or 0.0 if empty.
     */
    static float MeanConfidence(const std::vector<TranscriptionSegment>& segments);

    /**
     * @brief Concatenate segment texts into a single string.
     * @param segments Segment vector.
     * @return Joined text.
     */
    static std::string JoinText(const std::vector<TranscriptionSegment>& segments);

    // -------------------------------------------------------------------------
    // Data members
    // -------------------------------------------------------------------------

    LiveCaptionerConfig config_;  ///< Configuration (copy)
    std::string last_error_;      ///< Last error message
    bool initialized_ = false;    ///< True after Initialize() succeeds

    WhisperProcessor whisper_;         ///< Whisper ASR back-end
    VADSegmenter vad_;                 ///< VAD segmenter
    RingBuffer<int16_t> ring_buffer_;  ///< Lock-free SPSC ring buffer

    std::thread worker_thread_;         ///< Worker thread
    std::atomic<bool> running_{false};  ///< Signals the worker thread to run

    CaptionCallback callback_;  ///< User-supplied caption callback

    // Worker-thread-local state (no mutex needed — only touched by worker)
    std::vector<int16_t> accumulation_buffer_;  ///< Accumulates samples during speech
    uint32_t utterance_id_ = 0;                 ///< Incremented on each Final event

    std::atomic<bool> partial_in_flight_{false};  ///< Guard against overlapping partials
};

}  // namespace ffvoice

#endif  // ENABLE_WHISPER
