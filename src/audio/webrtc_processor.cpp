/**
 * @file webrtc_processor.cpp
 * @brief WebRTC Audio Processing Module implementation
 */

#include "audio/webrtc_processor.h"

#include "utils/logger.h"

#include <algorithm>
#include <cmath>

namespace ffvoice {

// ============================================================================
// WebRTCProcessor Implementation
// ============================================================================

WebRTCProcessor::WebRTCProcessor(const WebRTCConfig& config)
    : config_(config), buffer_pos_(0), frame_size_(0), has_voice_(false) {
}

WebRTCProcessor::~WebRTCProcessor() {
}

bool WebRTCProcessor::Initialize(int sample_rate, int channels) {
    sample_rate_ = sample_rate;
    channels_ = channels;

    // WebRTC APM only supports mono
    if (channels != 1) {
        log_error("WebRTCProcessor: Only mono (1 channel) is supported");
        return false;
    }

    // Calculate frame size (10ms worth of samples)
    frame_size_ = sample_rate / 100;  // 480 samples @48kHz, 160 @16kHz

    // Allocate buffer for frame rebuffering
    buffer_.resize(frame_size_, 0);
    buffer_pos_ = 0;

#ifdef ENABLE_WEBRTC_APM
    log_info("WebRTCProcessor initialized (WebRTC APM enabled):");
    log_info("  Sample rate: " + std::to_string(sample_rate) + " Hz");
    log_info("  Channels: " + std::to_string(channels));
    log_info("  Frame size: " + std::to_string(frame_size_) + " samples");
    log_info("  Noise Suppression: " + std::string(config_.enable_ns ? "ON" : "OFF"));
    log_info("  AGC: " + std::string(config_.enable_agc ? "ON" : "OFF"));
    log_info("  VAD: " + std::string(config_.enable_vad ? "ON" : "OFF"));

    // TODO: Initialize WebRTC APM instance (Phase 3)
    log_info("WebRTCProcessor: Full APM implementation pending (Phase 3)");
#else
    log_info("WebRTCProcessor initialized in PASSTHROUGH mode (WebRTC APM not enabled)");
    log_info("  Rebuild with -DENABLE_WEBRTC_APM=ON for full functionality");
#endif

    return true;
}

void WebRTCProcessor::ProcessFrame(int16_t* frame, size_t frame_size) {
#ifdef ENABLE_WEBRTC_APM
    // TODO: Implement WebRTC APM processing (Phase 3)
    // - Convert int16_t* to webrtc::AudioFrame
    // - Call apm_->ProcessStream()
    // - Extract VAD result if enabled
    // - Convert back to int16_t*

    // For now, pass through
    has_voice_ = false;
#else
    // Pass through mode
    (void)frame;
    (void)frame_size;
    has_voice_ = false;
#endif
}

void WebRTCProcessor::Process(int16_t* samples, size_t num_samples) {
    if (num_samples == 0)
        return;

#ifdef ENABLE_WEBRTC_APM
    // Frame rebuffering: 256 samples -> 480 samples
    // PortAudio gives us 256-sample chunks, but WebRTC APM needs 480-sample frames

    size_t input_pos = 0;

    while (input_pos < num_samples) {
        // Fill buffer up to frame_size_
        size_t to_copy = std::min(frame_size_ - buffer_pos_, num_samples - input_pos);
        std::copy(samples + input_pos, samples + input_pos + to_copy, buffer_.data() + buffer_pos_);
        buffer_pos_ += to_copy;
        input_pos += to_copy;

        // Process complete frame
        if (buffer_pos_ >= frame_size_) {
            ProcessFrame(buffer_.data(), frame_size_);

            // Copy processed frame back to output
            // Note: This introduces ~5ms latency but maintains correctness
            size_t output_start = input_pos - to_copy;
            std::copy(buffer_.data(), buffer_.data() + frame_size_, samples + output_start);

            buffer_pos_ = 0;
        }
    }
#else
    // Pass through mode (no processing)
    (void)samples;
    (void)num_samples;
#endif
}

void WebRTCProcessor::Reset() {
    buffer_pos_ = 0;
    std::fill(buffer_.begin(), buffer_.end(), 0);
    has_voice_ = false;

#ifdef ENABLE_WEBRTC_APM
    // TODO: Reset WebRTC APM state (Phase 3)
    log_info("WebRTCProcessor: State reset");
#endif
}

}  // namespace ffvoice
