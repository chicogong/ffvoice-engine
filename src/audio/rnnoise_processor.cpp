/**
 * @file rnnoise_processor.cpp
 * @brief RNNoise deep learning noise suppression implementation
 */

#include "audio/rnnoise_processor.h"

#include "utils/logger.h"

#include <algorithm>
#include <cmath>

namespace ffvoice {

RNNoiseProcessor::RNNoiseProcessor(const RNNoiseConfig& config) : config_(config) {
    log_info("RNNoiseProcessor created");
}

RNNoiseProcessor::~RNNoiseProcessor() {
#ifdef ENABLE_RNNOISE
    // Clean up RNNoise states
    for (auto* state : states_) {
        if (state) {
            rnnoise_destroy(state);
        }
    }
    states_.clear();
#endif
    log_info("RNNoiseProcessor destroyed");
}

bool RNNoiseProcessor::Initialize(int sample_rate, int channels) {
    sample_rate_ = sample_rate;
    channels_ = channels;

#ifdef ENABLE_RNNOISE
    // RNNoise supports 48kHz, 44.1kHz, 24kHz
    if (sample_rate != 48000 && sample_rate != 44100 && sample_rate != 24000) {
        log_error("RNNoise: Unsupported sample rate " + std::to_string(sample_rate) +
                  " Hz. Supported: 48000, 44100, 24000 Hz");
        return false;
    }

    // RNNoise frame size: 480 samples (10ms @48kHz)
    frame_size_ = 480;

    // Initialize rebuffer for frame accumulation (256 -> 480)
    rebuffer_.resize(frame_size_ * channels_, 0.0f);
    rebuffer_pos_ = 0;

    // Create RNNoise state for each channel
    states_.resize(channels_);
    for (int ch = 0; ch < channels_; ++ch) {
        states_[ch] = rnnoise_create(nullptr);
        if (!states_[ch]) {
            log_error("RNNoise: Failed to create DenoiseState for channel " + std::to_string(ch));
            return false;
        }
    }

    log_info("RNNoiseProcessor initialized:");
    log_info("  Sample rate: " + std::to_string(sample_rate) + " Hz");
    log_info("  Channels: " + std::to_string(channels));
    log_info("  Frame size: " + std::to_string(frame_size_) + " samples");
    if (config_.enable_vad) {
        log_info("  VAD: enabled (experimental)");
    }
#else
    // Passthrough mode when RNNoise is not enabled
    log_info("RNNoiseProcessor initialized in PASSTHROUGH mode");
    log_info("  (Rebuild with -DENABLE_RNNOISE=ON for actual noise suppression)");
    log_info("  Sample rate: " + std::to_string(sample_rate) + " Hz");
    log_info("  Channels: " + std::to_string(channels));
#endif

    return true;
}

void RNNoiseProcessor::Process(int16_t* samples, size_t num_samples) {
    if (num_samples == 0)
        return;

#ifdef ENABLE_RNNOISE
    // Convert int16_t -> float (resize only if needed to avoid reallocations)
    if (float_buffer_.size() < num_samples) {
        float_buffer_.resize(num_samples);
    }
    for (size_t i = 0; i < num_samples; ++i) {
        float_buffer_[i] = samples[i] / 32768.0f;  // Normalize to [-1, 1]
    }

    // Frame rebuffering (256 -> 480)
    size_t input_pos = 0;
    while (input_pos < num_samples) {
        // Copy samples to rebuffer
        size_t to_copy = std::min(frame_size_ * channels_ - rebuffer_pos_, num_samples - input_pos);
        std::copy(float_buffer_.begin() + input_pos, float_buffer_.begin() + input_pos + to_copy,
                  rebuffer_.begin() + rebuffer_pos_);
        rebuffer_pos_ += to_copy;
        input_pos += to_copy;

        // Process complete frame
        if (rebuffer_pos_ >= frame_size_ * channels_) {
            ProcessFrame(rebuffer_.data(), frame_size_);

            // Copy processed data back to float_buffer
            size_t output_start = input_pos - to_copy;
            for (size_t i = 0; i < frame_size_ * channels_; ++i) {
                if (output_start + i < num_samples) {
                    float_buffer_[output_start + i] = rebuffer_[i];
                }
            }

            rebuffer_pos_ = 0;
        }
    }

    // Convert float -> int16_t
    for (size_t i = 0; i < num_samples; ++i) {
        float clamped = std::clamp(float_buffer_[i], -1.0f, 1.0f);
        samples[i] = static_cast<int16_t>(clamped * 32767.0f);
    }
#else
    // Passthrough mode: do nothing
    (void)samples;
    (void)num_samples;
#endif
}

void RNNoiseProcessor::ProcessFrame(float* frame, size_t frame_size) {
#ifdef ENABLE_RNNOISE
    // Process each channel independently
    float total_vad_prob = 0.0f;
    for (int ch = 0; ch < channels_; ++ch) {
        // Extract channel data (deinterleave)
        std::vector<float> channel_data(frame_size);
        for (size_t i = 0; i < frame_size; ++i) {
            channel_data[i] = frame[i * channels_ + ch];
        }

        // Apply RNNoise denoising (in-place)
        // rnnoise_process_frame returns VAD probability (0.0-1.0)
        float vad_prob = rnnoise_process_frame(states_[ch], channel_data.data(), channel_data.data());
        total_vad_prob += vad_prob;

        // Write back to interleaved buffer
        for (size_t i = 0; i < frame_size; ++i) {
            frame[i * channels_ + ch] = channel_data[i];
        }
    }

    // Average VAD probability across channels (for stereo)
    if (config_.enable_vad) {
        last_vad_prob_ = total_vad_prob / channels_;
    }
#else
    (void)frame;
    (void)frame_size;
#endif
}

void RNNoiseProcessor::Reset() {
    rebuffer_pos_ = 0;
    last_vad_prob_ = 0.0f;
    std::fill(rebuffer_.begin(), rebuffer_.end(), 0.0f);

#ifdef ENABLE_RNNOISE
    // Destroy and recreate RNNoise states
    for (auto* state : states_) {
        if (state) {
            rnnoise_destroy(state);
        }
    }
    states_.clear();

    // Recreate states
    states_.resize(channels_);
    for (int ch = 0; ch < channels_; ++ch) {
        states_[ch] = rnnoise_create(nullptr);
    }

    log_info("RNNoiseProcessor: State reset");
#endif
}

}  // namespace ffvoice
