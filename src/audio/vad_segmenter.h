/**
 * @file vad_segmenter.h
 * @brief Voice Activity Detection (VAD) based audio segmentation
 *
 * VADSegmenter uses voice activity detection to intelligently segment audio
 * streams into meaningful chunks for real-time speech recognition.
 */

#pragma once

#include <cstddef>
#include <cstdint>
#include <functional>
#include <vector>

namespace ffvoice {

/**
 * @brief VAD-based audio segmentation for real-time speech recognition
 *
 * The VADSegmenter accumulates audio samples based on voice activity detection
 * and triggers callbacks when a complete speech segment is detected (followed
 * by sufficient silence) or when the maximum segment length is reached.
 *
 * State Machine:
 *   [Silence] --[VAD detects speech]--> [Accumulating Speech]
 *   [Accumulating Speech] --[VAD detects silence]--> [Trigger Callback] --> [Silence]
 *   [Accumulating Speech] --[Max length reached]--> [Trigger Callback] --> [Silence]
 */
class VADSegmenter {
public:
    /**
     * @brief Configuration for VAD segmentation
     */
    struct Config {
        float speech_threshold = 0.5f;        ///< VAD probability threshold (0.0-1.0)
        int min_speech_frames = 30;           ///< Minimum speech frames (~0.3s @10ms)
        int min_silence_frames = 50;          ///< Minimum silence frames (~0.5s @10ms)
        size_t max_segment_samples = 480000; ///< Max segment length (10s @48kHz)
    };

    /**
     * @brief Callback type for segment completion
     * @param samples Pointer to audio samples (int16_t)
     * @param num_samples Number of samples in the segment
     */
    using SegmentCallback = std::function<void(const int16_t*, size_t)>;

    /**
     * @brief Construct a VADSegmenter with default configuration
     */
    VADSegmenter();

    /**
     * @brief Construct a VADSegmenter with the given configuration
     * @param config Configuration parameters
     */
    explicit VADSegmenter(const Config& config);

    /**
     * @brief Process an audio frame with VAD probability
     *
     * Accumulates audio samples and detects segment boundaries based on
     * voice activity. Triggers the callback when a complete segment is detected.
     *
     * @param samples Audio samples (int16_t array)
     * @param num_samples Number of samples in this frame
     * @param vad_prob Voice activity probability (0.0 = silence, 1.0 = speech)
     * @param on_segment Callback to invoke when a segment is complete
     */
    void ProcessFrame(const int16_t* samples, size_t num_samples, float vad_prob,
                      SegmentCallback on_segment);

    /**
     * @brief Flush any remaining buffered audio
     *
     * Call this at the end of recording to process any accumulated audio
     * that hasn't been flushed yet.
     *
     * @param on_segment Callback to invoke for the final segment
     */
    void Flush(SegmentCallback on_segment);

    /**
     * @brief Reset the segmenter state
     *
     * Clears all buffered audio and resets internal state.
     */
    void Reset();

    /**
     * @brief Get the current buffer size in samples
     * @return Number of samples currently buffered
     */
    size_t GetBufferSize() const { return buffer_.size(); }

    /**
     * @brief Check if currently in a speech segment
     * @return True if accumulating speech, false if in silence
     */
    bool IsInSpeech() const { return in_speech_; }

private:
    Config config_;                   ///< Configuration parameters
    std::vector<int16_t> buffer_;     ///< Audio sample buffer
    int speech_frames_ = 0;           ///< Consecutive speech frames
    int silence_frames_ = 0;          ///< Consecutive silence frames
    bool in_speech_ = false;          ///< Currently in speech segment
};

} // namespace ffvoice
