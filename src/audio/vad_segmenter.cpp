/**
 * @file vad_segmenter.cpp
 * @brief Implementation of VAD-based audio segmentation
 */

#include "audio/vad_segmenter.h"

#include <algorithm>
#include <cstring>

#include "utils/logger.h"

namespace ffvoice {

VADSegmenter::VADSegmenter() : VADSegmenter(Config{}) {
}

VADSegmenter::VADSegmenter(const Config& config) : config_(config) {
    // Reserve space for maximum segment size to avoid reallocations
    buffer_.reserve(config_.max_segment_samples);
}

void VADSegmenter::ProcessFrame(const int16_t* samples, size_t num_samples, float vad_prob,
                                SegmentCallback on_segment) {
    // Determine if this frame contains speech
    bool is_speech = vad_prob >= config_.speech_threshold;

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
    LOG_INFO("VADSegmenter: Reset");
}

} // namespace ffvoice
