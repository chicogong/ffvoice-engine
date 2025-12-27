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
     * @brief Pre-defined VAD sensitivity presets
     */
    enum class Sensitivity {
        VERY_SENSITIVE,  ///< Detect very quiet speech (threshold=0.3, quick trigger)
        SENSITIVE,       ///< Detect quiet speech (threshold=0.4)
        BALANCED,        ///< Balanced detection (threshold=0.5, default)
        CONSERVATIVE,    ///< Avoid false positives (threshold=0.6)
        VERY_CONSERVATIVE  ///< Only very clear speech (threshold=0.7)
    };

    /**
     * @brief Configuration for VAD segmentation
     */
    struct Config {
        float speech_threshold = 0.5f;        ///< VAD probability threshold (0.0-1.0)
        int min_speech_frames = 30;           ///< Minimum speech frames (~0.3s @10ms)
        int min_silence_frames = 50;          ///< Minimum silence frames (~0.5s @10ms)
        size_t max_segment_samples = 480000;  ///< Max segment length (10s @48kHz)
        bool enable_adaptive_threshold = false;  ///< Enable adaptive threshold adjustment
        float adaptive_factor = 0.1f;         ///< Adaptation speed (0.0-1.0, lower=slower)

        /**
         * @brief Create configuration from sensitivity preset
         * @param sensitivity Preset sensitivity level
         * @return Config with preset parameters
         */
        static Config FromPreset(Sensitivity sensitivity);
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

    /**
     * @brief Get current speech threshold (may differ from config if adaptive)
     * @return Current threshold value
     */
    float GetCurrentThreshold() const { return current_threshold_; }

    /**
     * @brief Get VAD statistics
     * @param avg_vad_prob Average VAD probability
     * @param speech_ratio Ratio of speech frames to total frames
     */
    void GetStatistics(float& avg_vad_prob, float& speech_ratio) const;

private:
    Config config_;                   ///< Configuration parameters
    std::vector<int16_t> buffer_;     ///< Audio sample buffer
    int speech_frames_ = 0;           ///< Consecutive speech frames
    int silence_frames_ = 0;          ///< Consecutive silence frames
    bool in_speech_ = false;          ///< Currently in speech segment

    // Adaptive threshold and statistics
    float current_threshold_;         ///< Current threshold (adaptive if enabled)
    float vad_sum_ = 0.0f;           ///< Sum of VAD probabilities for averaging
    int total_frames_ = 0;            ///< Total frames processed
    int total_speech_frames_ = 0;     ///< Total speech frames detected

    /**
     * @brief Update adaptive threshold based on recent VAD probabilities
     * @param vad_prob Current VAD probability
     */
    void UpdateAdaptiveThreshold(float vad_prob);
};

} // namespace ffvoice
