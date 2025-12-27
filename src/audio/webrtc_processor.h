/**
 * @file webrtc_processor.h
 * @brief WebRTC Audio Processing Module integration
 */

#pragma once

#include "audio/audio_processor.h"

#include <memory>
#include <vector>

#ifdef ENABLE_WEBRTC_APM
// WebRTC APM headers will be included here once dependency is integrated
#endif

namespace ffvoice {

/**
 * @brief WebRTC APM configuration
 */
struct WebRTCConfig {
    bool enable_ns = true;    ///< Enable noise suppression
    bool enable_agc = true;   ///< Enable automatic gain control
    bool enable_vad = false;  ///< Enable voice activity detection

    enum class NSLevel {
        Low,       ///< Low noise suppression
        Moderate,  ///< Moderate noise suppression
        High,      ///< High noise suppression
        VeryHigh   ///< Very high noise suppression
    };

    NSLevel ns_level = NSLevel::Moderate;  ///< Noise suppression level
    int agc_target_level_dbfs = 3;         ///< AGC target level in dBFS (0-31)
};

/**
 * @brief WebRTC Audio Processing Module processor
 *
 * Integrates WebRTC APM for advanced audio processing:
 * - Noise suppression (NS)
 * - Automatic gain control (AGC)
 * - Voice activity detection (VAD)
 *
 * Note: Currently supports mono (1 channel) only at 48kHz or 16kHz.
 */
class WebRTCProcessor : public AudioProcessor {
public:
    /**
     * @brief Create WebRTC processor with configuration
     * @param config WebRTC configuration
     */
    explicit WebRTCProcessor(const WebRTCConfig& config = WebRTCConfig{});

    /**
     * @brief Destructor
     */
    ~WebRTCProcessor() override;

    /**
     * @brief Initialize the processor
     * @param sample_rate Sample rate in Hz (48000 or 16000)
     * @param channels Number of channels (must be 1 for mono)
     * @return true if successful
     */
    bool Initialize(int sample_rate, int channels) override;

    /**
     * @brief Process audio samples in-place
     * @param samples Audio samples (int16_t interleaved)
     * @param num_samples Number of samples (not frames!)
     */
    void Process(int16_t* samples, size_t num_samples) override;

    /**
     * @brief Reset processor state
     */
    void Reset() override;

    /**
     * @brief Get processor name
     */
    std::string GetName() const override {
        return "WebRTCProcessor";
    }

    /**
     * @brief Check if voice was detected in last frame (requires VAD enabled)
     * @return true if voice detected
     */
    bool HasVoice() const {
        return has_voice_;
    }

private:
    /**
     * @brief Process a single frame (10ms = 480 samples @48kHz)
     * @param frame Frame buffer
     * @param frame_size Frame size in samples
     */
    void ProcessFrame(int16_t* frame, size_t frame_size);

    WebRTCConfig config_;  ///< Configuration

#ifdef ENABLE_WEBRTC_APM
    // WebRTC APM instance (will be implemented in Phase 2-3)
    // std::unique_ptr<webrtc::AudioProcessing> apm_;
#endif

    std::vector<int16_t> buffer_;  ///< Internal buffer for frame rebuffering
    size_t buffer_pos_ = 0;        ///< Current position in buffer
    size_t frame_size_ = 0;        ///< Frame size (10ms worth of samples)
    bool has_voice_ = false;       ///< Voice activity detection result
};

}  // namespace ffvoice
