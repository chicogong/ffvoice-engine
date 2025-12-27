/**
 * @file audio_processor.cpp
 * @brief Audio processing implementations
 */

#include "audio/audio_processor.h"

#include "utils/logger.h"

#include <algorithm>
#include <cmath>
#include <limits>

namespace ffvoice {

// ============================================================================
// VolumeNormalizer Implementation
// ============================================================================

VolumeNormalizer::VolumeNormalizer(float target_level, float attack_time, float release_time)
    : target_level_(target_level),
      attack_time_(attack_time),
      release_time_(release_time),
      current_gain_(1.0f),
      attack_coeff_(0.0f),
      release_coeff_(0.0f) {
}

bool VolumeNormalizer::Initialize(int sample_rate, int channels) {
    sample_rate_ = sample_rate;
    channels_ = channels;

    // Calculate smoothing coefficients for gain adjustment
    // Using exponential moving average: alpha = 1 - exp(-dt / tau)
    attack_coeff_ = 1.0f - std::exp(-1.0f / (attack_time_ * sample_rate));
    release_coeff_ = 1.0f - std::exp(-1.0f / (release_time_ * sample_rate));

    current_gain_ = 1.0f;

    log_info("VolumeNormalizer initialized: target=" + std::to_string(target_level_) +
             ", attack=" + std::to_string(attack_time_) + "s" +
             ", release=" + std::to_string(release_time_) + "s");

    return true;
}

void VolumeNormalizer::Process(int16_t* samples, size_t num_samples) {
    if (num_samples == 0)
        return;

    constexpr float max_sample = 32767.0f;
    constexpr float min_gain = 0.1f;
    constexpr float max_gain = 10.0f;

    // Process in frames
    size_t num_frames = num_samples / channels_;

    for (size_t i = 0; i < num_frames; ++i) {
        // Calculate RMS for this frame
        float sum_squares = 0.0f;
        for (int ch = 0; ch < channels_; ++ch) {
            size_t idx = i * channels_ + ch;
            float sample = samples[idx] / max_sample;
            sum_squares += sample * sample;
        }
        float rms = std::sqrt(sum_squares / channels_);

        // Calculate desired gain
        float desired_gain = (rms > 0.001f) ? (target_level_ / rms) : 1.0f;
        desired_gain = std::clamp(desired_gain, min_gain, max_gain);

        // Smooth gain adjustment (attack/release)
        float coeff = (desired_gain > current_gain_) ? attack_coeff_ : release_coeff_;
        current_gain_ = current_gain_ + coeff * (desired_gain - current_gain_);

        // Apply gain to all channels in this frame
        for (int ch = 0; ch < channels_; ++ch) {
            size_t idx = i * channels_ + ch;
            float processed = samples[idx] * current_gain_;
            // Clamp to prevent overflow
            processed = std::clamp(processed, -max_sample, max_sample);
            samples[idx] = static_cast<int16_t>(processed);
        }
    }
}

void VolumeNormalizer::Reset() {
    current_gain_ = 1.0f;
}

// ============================================================================
// HighPassFilter Implementation
// ============================================================================

HighPassFilter::HighPassFilter(float cutoff_freq) : cutoff_freq_(cutoff_freq), alpha_(0.0f) {
}

bool HighPassFilter::Initialize(int sample_rate, int channels) {
    sample_rate_ = sample_rate;
    channels_ = channels;

    // Calculate filter coefficient
    // For first-order HPF: alpha = 1 / (1 + 2*pi*fc/fs)
    float rc = 1.0f / (2.0f * M_PI * cutoff_freq_);
    float dt = 1.0f / sample_rate;
    alpha_ = rc / (rc + dt);

    // Initialize state per channel
    prev_input_.resize(channels, 0.0f);
    prev_output_.resize(channels, 0.0f);

    log_info("HighPassFilter initialized: cutoff=" + std::to_string(cutoff_freq_) + "Hz");

    return true;
}

void HighPassFilter::Process(int16_t* samples, size_t num_samples) {
    if (num_samples == 0)
        return;

    constexpr float max_sample = 32767.0f;

    // Process samples
    size_t num_frames = num_samples / channels_;

    for (size_t i = 0; i < num_frames; ++i) {
        for (int ch = 0; ch < channels_; ++ch) {
            size_t idx = i * channels_ + ch;

            // Convert to normalized float
            float input = samples[idx] / max_sample;

            // First-order high-pass filter:
            // y[n] = alpha * (y[n-1] + x[n] - x[n-1])
            float output = alpha_ * (prev_output_[ch] + input - prev_input_[ch]);

            // Update state
            prev_input_[ch] = input;
            prev_output_[ch] = output;

            // Convert back to int16_t
            output = std::clamp(output * max_sample, -max_sample, max_sample);
            samples[idx] = static_cast<int16_t>(output);
        }
    }
}

void HighPassFilter::Reset() {
    std::fill(prev_input_.begin(), prev_input_.end(), 0.0f);
    std::fill(prev_output_.begin(), prev_output_.end(), 0.0f);
}

// ============================================================================
// AudioProcessorChain Implementation
// ============================================================================

void AudioProcessorChain::AddProcessor(std::unique_ptr<AudioProcessor> processor) {
    if (processor) {
        log_info("Adding processor to chain: " + processor->GetName());
        processors_.push_back(std::move(processor));
    }
}

bool AudioProcessorChain::Initialize(int sample_rate, int channels) {
    sample_rate_ = sample_rate;
    channels_ = channels;

    // Initialize all processors in chain
    for (auto& processor : processors_) {
        if (!processor->Initialize(sample_rate, channels)) {
            log_error("Failed to initialize processor: " + processor->GetName());
            return false;
        }
    }

    log_info("AudioProcessorChain initialized with " + std::to_string(processors_.size()) +
             " processors");

    return true;
}

void AudioProcessorChain::Process(int16_t* samples, size_t num_samples) {
    // Process through each processor in sequence
    for (auto& processor : processors_) {
        processor->Process(samples, num_samples);
    }
}

void AudioProcessorChain::Reset() {
    for (auto& processor : processors_) {
        processor->Reset();
    }
}

}  // namespace ffvoice
