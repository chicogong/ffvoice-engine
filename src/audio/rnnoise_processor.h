/**
 * @file rnnoise_processor.h
 * @brief RNNoise deep learning noise suppression
 */

#pragma once

#include "audio/audio_processor.h"

#include <memory>
#include <vector>

#ifdef ENABLE_RNNOISE
extern "C" {
    #include <rnnoise.h>
}
#endif

namespace ffvoice {

/**
 * @brief RNNoise configuration
 */
struct RNNoiseConfig {
    bool enable_vad = false;  ///< Voice activity detection (experimental)
};

/**
 * @brief RNNoise deep learning noise suppression processor
 *
 * Integrates Xiph RNNoise for real-time speech denoising:
 * - Deep learning based (RNN)
 * - Low CPU overhead (~5-10%)
 * - Excellent noise reduction for speech
 * - Supports stereo + mono
 *
 * Note: Requires float conversion and frame rebuffering (256 -> 480 samples)
 */
class RNNoiseProcessor : public AudioProcessor {
public:
    /**
     * @brief Create RNNoise processor with configuration
     * @param config RNNoise configuration
     */
    explicit RNNoiseProcessor(const RNNoiseConfig& config = RNNoiseConfig{});

    /**
     * @brief Destructor
     */
    ~RNNoiseProcessor() override;

    /**
     * @brief Initialize the processor
     * @param sample_rate Sample rate in Hz (48000, 44100, or 24000)
     * @param channels Number of channels (1 or 2)
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
    std::string GetName() const override { return "RNNoiseProcessor"; }

    /**
     * @brief Get the last VAD probability (0.0 = silence, 1.0 = speech)
     *
     * This returns the VAD probability from the most recent processed frame.
     * Only valid when enable_vad is true in config.
     *
     * @return VAD probability (0.0-1.0), or 0.0 if VAD is disabled
     */
    float GetVADProbability() const { return last_vad_prob_; }

private:
    /**
     * @brief Process a single frame (480 samples)
     * @param frame Frame buffer (float, interleaved)
     * @param frame_size Frame size in samples per channel
     */
    void ProcessFrame(float* frame, size_t frame_size);

    RNNoiseConfig config_;  ///< Configuration

#ifdef ENABLE_RNNOISE
    // RNNoise states (one per channel)
    std::vector<DenoiseState*> states_;
#endif

    // Format conversion buffers
    std::vector<float> float_buffer_;  ///< int16 -> float conversion
    std::vector<int16_t> int16_buffer_;

    // Frame rebuffering (256 -> 480)
    std::vector<float> rebuffer_;  ///< Accumulation buffer
    size_t rebuffer_pos_ = 0;      ///< Current position in rebuffer
    size_t frame_size_ = 0;        ///< 480 samples @48kHz (10ms)

    // VAD state
    float last_vad_prob_ = 0.0f;  ///< Last VAD probability (0.0-1.0)
};

} // namespace ffvoice
