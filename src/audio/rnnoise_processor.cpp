/**
 * @file rnnoise_processor.cpp
 * @brief RNNoise deep learning noise suppression implementation
 */

#include "audio/rnnoise_processor.h"

#include "utils/logger.h"

#include <algorithm>
#include <cmath>
#include <limits>

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
        LOG_ERROR("RNNoise: Unsupported sample rate %d Hz. Supported: 48000, 44100, 24000 Hz",
                  sample_rate);
        return false;
    }

    // RNNoise frame size: 480 samples (10ms @48kHz)
    frame_size_ = 480;

    // Initialize rebuffer for frame accumulation (256 -> 480)
    rebuffer_.resize(frame_size_ * channels_, 0.0f);
    rebuffer_pos_ = 0;

    // Pre-allocate channel buffer to avoid allocations in ProcessFrame
    channel_buffer_.resize(frame_size_);

    // Create RNNoise state for each channel
    states_.resize(channels_);
    for (int ch = 0; ch < channels_; ++ch) {
        states_[ch] = rnnoise_create(nullptr);
        if (!states_[ch]) {
            LOG_ERROR("RNNoise: Failed to create DenoiseState for channel %d", ch);
            return false;
        }
    }

    LOG_INFO("RNNoiseProcessor initialized:");
    LOG_INFO("  Sample rate: %d Hz", sample_rate);
    LOG_INFO("  Channels: %d", channels);
    LOG_INFO("  Frame size: %zu samples", frame_size_);
    if (config_.enable_vad) {
        LOG_INFO("  VAD: enabled (experimental)");
    }
#else
    // Passthrough mode when RNNoise is not enabled
    LOG_INFO("RNNoiseProcessor initialized in PASSTHROUGH mode");
    LOG_INFO("  (Rebuild with -DENABLE_RNNOISE=ON for actual noise suppression)");
    LOG_INFO("  Sample rate: %d Hz", sample_rate);
    LOG_INFO("  Channels: %d", channels);
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
        // Check for potential overflow in frame_size_ * channels_
        size_t frame_total_size = frame_size_ * channels_;
        if (frame_size_ > 0 && frame_total_size / frame_size_ != static_cast<size_t>(channels_)) {
            LOG_ERROR("RNNoiseProcessor: Buffer size overflow detected (frame_size=%zu, channels=%d)",
                     frame_size_, channels_);
            return;
        }

        size_t remaining_in_rebuffer = frame_total_size - rebuffer_pos_;
        size_t remaining_in_input = num_samples - input_pos;
        size_t to_copy = std::min(remaining_in_rebuffer, remaining_in_input);
        std::copy(float_buffer_.begin() + input_pos, float_buffer_.begin() + input_pos + to_copy,
                  rebuffer_.begin() + rebuffer_pos_);
        rebuffer_pos_ += to_copy;
        input_pos += to_copy;

        // Process complete frame
        if (rebuffer_pos_ >= frame_total_size) {
            ProcessFrame(rebuffer_.data(), frame_size_);

            // Copy processed data back to float_buffer
            size_t output_start = input_pos - to_copy;
            for (size_t i = 0; i < frame_total_size; ++i) {
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
        // Extract channel data (deinterleave) - reuse pre-allocated buffer
        for (size_t i = 0; i < frame_size; ++i) {
            channel_buffer_[i] = frame[i * channels_ + ch];
        }

        // Apply RNNoise denoising (in-place)
        // rnnoise_process_frame returns VAD probability (0.0-1.0)
        float vad_prob = rnnoise_process_frame(states_[ch], channel_buffer_.data(), channel_buffer_.data());
        total_vad_prob += vad_prob;

        // Write back to interleaved buffer
        for (size_t i = 0; i < frame_size; ++i) {
            frame[i * channels_ + ch] = channel_buffer_[i];
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
        if (!states_[ch]) {
            LOG_ERROR("RNNoiseProcessor: Failed to recreate RNNoise state for channel %d", ch);
            // Clean up any successfully created states
            for (int i = 0; i < ch; ++i) {
                if (states_[i]) {
                    rnnoise_destroy(states_[i]);
                }
            }
            states_.clear();
            return;
        }
    }

    log_info("RNNoiseProcessor: State reset successfully");
#endif
}

}  // namespace ffvoice
