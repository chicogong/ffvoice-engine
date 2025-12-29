/**
 * @file vad_segmenter.cpp
 * @brief Implementation of VAD-based audio segmentation
 */

#include "audio/vad_segmenter.h"

#include <algorithm>
#include <cstring>

#include "utils/logger.h"

namespace ffvoice {

VADSegmenter::Config VADSegmenter::Config::FromPreset(Sensitivity sensitivity) {
    Config config;
    switch (sensitivity) {
        case Sensitivity::VERY_SENSITIVE:
            config.speech_threshold = 0.3f;
            config.min_speech_frames = 20;   // ~0.2s (faster trigger)
            config.min_silence_frames = 40;  // ~0.4s
            break;
        case Sensitivity::SENSITIVE:
            config.speech_threshold = 0.4f;
            config.min_speech_frames = 25;   // ~0.25s
            config.min_silence_frames = 45;  // ~0.45s
            break;
        case Sensitivity::BALANCED:
            config.speech_threshold = 0.5f;
            config.min_speech_frames = 30;   // ~0.3s (default)
            config.min_silence_frames = 50;  // ~0.5s
            break;
        case Sensitivity::CONSERVATIVE:
            config.speech_threshold = 0.6f;
            config.min_speech_frames = 35;   // ~0.35s
            config.min_silence_frames = 55;  // ~0.55s
            break;
        case Sensitivity::VERY_CONSERVATIVE:
            config.speech_threshold = 0.7f;
            config.min_speech_frames = 40;   // ~0.4s (slower trigger)
            config.min_silence_frames = 60;  // ~0.6s
            break;
    }
    return config;
}

VADSegmenter::VADSegmenter() : VADSegmenter(Config{}) {
}

VADSegmenter::VADSegmenter(const Config& config)
    : config_(config), current_threshold_(config.speech_threshold) {
    // Reserve space for maximum segment size to avoid reallocations
    buffer_.reserve(config_.max_segment_samples);
}

void VADSegmenter::ProcessFrame(const int16_t* samples, size_t num_samples, float vad_prob,
                                SegmentCallback on_segment) {
    // Update statistics
    vad_sum_ += vad_prob;
    total_frames_++;

    // Update adaptive threshold if enabled
    if (config_.enable_adaptive_threshold) {
        UpdateAdaptiveThreshold(vad_prob);
    }

    // Determine if this frame contains speech (using current threshold)
    bool is_speech = vad_prob >= current_threshold_;

    if (is_speech) {
        total_speech_frames_++;
    }

    if (is_speech) {
        speech_frames_++;
        silence_frames_ = 0;

        // Start accumulating if we have enough consecutive speech frames
        if (!in_speech_ && speech_frames_ >= config_.min_speech_frames) {
            in_speech_ = true;
            LOG_INFO("VADSegmenter: Speech started (VAD prob: %.2f)", vad_prob);
        }
    } else {
        silence_frames_++;
        speech_frames_ = 0;
    }

    // Accumulate samples if in speech segment
    if (in_speech_) {
        buffer_.insert(buffer_.end(), samples, samples + num_samples);

        // Check termination conditions
        bool max_length_reached = buffer_.size() >= config_.max_segment_samples;
        bool silence_detected = silence_frames_ >= config_.min_silence_frames;

        if (max_length_reached || silence_detected) {
            const char* reason = max_length_reached ? "max length" : "silence";
            LOG_INFO("VADSegmenter: Segment complete (%s), %zu samples (%.2fs)", reason,
                     buffer_.size(), buffer_.size() / 48000.0);

            // Trigger callback with accumulated segment
            if (on_segment && !buffer_.empty()) {
                on_segment(buffer_.data(), buffer_.size());
            }

            // Reset state for next segment
            buffer_.clear();
            in_speech_ = false;
            speech_frames_ = 0;
            silence_frames_ = 0;
        }
    }
}

void VADSegmenter::Flush(SegmentCallback on_segment) {
    if (!buffer_.empty()) {
        LOG_INFO("VADSegmenter: Flushing final segment, %zu samples (%.2fs)", buffer_.size(),
                 buffer_.size() / 48000.0);

        if (on_segment) {
            on_segment(buffer_.data(), buffer_.size());
        }

        buffer_.clear();
    }

    // Reset state
    in_speech_ = false;
    speech_frames_ = 0;
    silence_frames_ = 0;
}

void VADSegmenter::Reset() {
    buffer_.clear();
    in_speech_ = false;
    speech_frames_ = 0;
    silence_frames_ = 0;

    // Reset statistics
    vad_sum_ = 0.0f;
    total_frames_ = 0;
    total_speech_frames_ = 0;
    current_threshold_ = config_.speech_threshold;

    LOG_INFO("VADSegmenter: Reset");
}

void VADSegmenter::UpdateAdaptiveThreshold(float /* vad_prob */) {
    // Use exponential moving average to adapt threshold
    // The threshold moves towards the average VAD probability
    if (total_frames_ > 0) {
        float avg_vad = vad_sum_ / total_frames_;

        // Adapt threshold: move towards avg + offset
        // If environment is noisy (high avg VAD), increase threshold
        // If environment is quiet (low avg VAD), decrease threshold
        float target_threshold = avg_vad + 0.2f;  // Keep threshold above average
        target_threshold = std::max(0.2f, std::min(0.8f, target_threshold));  // Clamp [0.2, 0.8]

        // Smooth adaptation using exponential moving average
        current_threshold_ += config_.adaptive_factor * (target_threshold - current_threshold_);
    }
}

void VADSegmenter::GetStatistics(float& avg_vad_prob, float& speech_ratio) const {
    if (total_frames_ > 0) {
        avg_vad_prob = vad_sum_ / total_frames_;
        speech_ratio = static_cast<float>(total_speech_frames_) / total_frames_;
    } else {
        avg_vad_prob = 0.0f;
        speech_ratio = 0.0f;
    }
}

}  // namespace ffvoice
