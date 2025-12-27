/**
 * @file audio_processor.h
 * @brief Audio processing interface and implementations
 */

#pragma once

#include <cstdint>
#include <memory>
#include <string>
#include <vector>

namespace ffvoice {

/**
 * @brief Abstract audio processor interface
 *
 * Base class for all audio processing modules.
 * Processes audio in-place for efficiency.
 */
class AudioProcessor {
public:
    virtual ~AudioProcessor() = default;

    /**
     * @brief Initialize the processor
     * @param sample_rate Sample rate in Hz
     * @param channels Number of channels
     * @return true if successful
     */
    virtual bool Initialize(int sample_rate, int channels) = 0;

    /**
     * @brief Process audio samples in-place
     * @param samples Audio samples (int16_t interleaved)
     * @param num_samples Number of samples (not frames!)
     */
    virtual void Process(int16_t* samples, size_t num_samples) = 0;

    /**
     * @brief Reset processor state
     */
    virtual void Reset() = 0;

    /**
     * @brief Get processor name
     */
    virtual std::string GetName() const = 0;

protected:
    int sample_rate_ = 0;
    int channels_ = 0;
};

/**
 * @brief Volume normalizer with automatic gain control
 *
 * Normalizes audio volume to prevent clipping and maintain
 * consistent loudness. Uses peak detection and smooth gain adjustment.
 */
class VolumeNormalizer : public AudioProcessor {
public:
    /**
     * @brief Create volume normalizer
     * @param target_level Target RMS level (0.0-1.0, default 0.3)
     * @param attack_time Attack time in seconds (default 0.1)
     * @param release_time Release time in seconds (default 0.3)
     */
    VolumeNormalizer(float target_level = 0.3f, float attack_time = 0.1f,
                     float release_time = 0.3f);

    bool Initialize(int sample_rate, int channels) override;
    void Process(int16_t* samples, size_t num_samples) override;
    void Reset() override;
    std::string GetName() const override {
        return "VolumeNormalizer";
    }

private:
    float target_level_;
    float attack_time_;
    float release_time_;
    float current_gain_;
    float attack_coeff_;
    float release_coeff_;
};

/**
 * @brief High-pass filter to remove low-frequency noise
 *
 * Removes rumble, breathing sounds, and other low-frequency noise.
 * Uses a simple first-order IIR filter.
 */
class HighPassFilter : public AudioProcessor {
public:
    /**
     * @brief Create high-pass filter
     * @param cutoff_freq Cutoff frequency in Hz (default 80Hz)
     */
    explicit HighPassFilter(float cutoff_freq = 80.0f);

    bool Initialize(int sample_rate, int channels) override;
    void Process(int16_t* samples, size_t num_samples) override;
    void Reset() override;
    std::string GetName() const override {
        return "HighPassFilter";
    }

private:
    float cutoff_freq_;
    float alpha_;                     // Filter coefficient
    std::vector<float> prev_input_;   // Previous input per channel
    std::vector<float> prev_output_;  // Previous output per channel
};

/**
 * @brief Chain multiple audio processors
 *
 * Processes audio through multiple processors in sequence.
 */
class AudioProcessorChain : public AudioProcessor {
public:
    AudioProcessorChain() = default;

    /**
     * @brief Add a processor to the chain
     */
    void AddProcessor(std::unique_ptr<AudioProcessor> processor);

    bool Initialize(int sample_rate, int channels) override;
    void Process(int16_t* samples, size_t num_samples) override;
    void Reset() override;
    std::string GetName() const override {
        return "AudioProcessorChain";
    }

    /**
     * @brief Get number of processors in chain
     */
    size_t GetProcessorCount() const {
        return processors_.size();
    }

private:
    std::vector<std::unique_ptr<AudioProcessor>> processors_;
};

}  // namespace ffvoice
