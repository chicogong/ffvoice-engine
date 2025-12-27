/**
 * @file flac_writer.cpp
 * @brief FLAC file writer implementation using libFLAC
 */

#include "media/flac_writer.h"

#include "utils/logger.h"

#include <FLAC/stream_encoder.h>

#include <algorithm>
#include <cstring>
#include <fstream>

namespace ffvoice {

FlacWriter::~FlacWriter() {
    Close();
}

bool FlacWriter::Open(const std::string& filename, int sample_rate, int channels,
                      int bits_per_sample, int compression_level) {
    if (encoder_) {
        log_error("FLAC encoder already open");
        return false;
    }

    // Validate parameters
    if (channels < 1 || channels > 2) {
        log_error("FLAC: Invalid channel count: " + std::to_string(channels));
        return false;
    }

    if (bits_per_sample != 16 && bits_per_sample != 24) {
        log_error("FLAC: Unsupported bits per sample: " + std::to_string(bits_per_sample));
        return false;
    }

    if (compression_level < 0 || compression_level > 8) {
        log_error("FLAC: Invalid compression level: " + std::to_string(compression_level));
        return false;
    }

    // Create encoder
    encoder_ = FLAC__stream_encoder_new();
    if (!encoder_) {
        log_error("FLAC: Failed to create encoder");
        return false;
    }

    // Store parameters
    filename_ = filename;
    sample_rate_ = sample_rate;
    channels_ = channels;
    bits_per_sample_ = bits_per_sample;
    total_samples_ = 0;
    bytes_written_ = 0;

    // Configure encoder
    FLAC__stream_encoder_set_channels(encoder_, channels);
    FLAC__stream_encoder_set_bits_per_sample(encoder_, bits_per_sample);
    FLAC__stream_encoder_set_sample_rate(encoder_, sample_rate);
    FLAC__stream_encoder_set_compression_level(encoder_, compression_level);

    // Enable verify mode for debugging
    FLAC__stream_encoder_set_verify(encoder_, true);

    // Initialize encoder
    FLAC__StreamEncoderInitStatus init_status =
        FLAC__stream_encoder_init_file(encoder_, filename.c_str(), nullptr, nullptr);

    if (init_status != FLAC__STREAM_ENCODER_INIT_STATUS_OK) {
        log_error("FLAC: Encoder init failed: " +
                  std::string(FLAC__StreamEncoderInitStatusString[init_status]));
        FLAC__stream_encoder_delete(encoder_);
        encoder_ = nullptr;
        return false;
    }

    log_info("FLAC encoder opened: " + filename + " (" + std::to_string(sample_rate) + "Hz, " +
             std::to_string(channels) + "ch, " + std::to_string(bits_per_sample) +
             "-bit, level=" + std::to_string(compression_level) + ")");

    return true;
}

size_t FlacWriter::WriteSamples(const int16_t* samples, size_t num_samples) {
    if (!encoder_) {
        log_error("FLAC encoder not open");
        return 0;
    }

    if (!samples || num_samples == 0) {
        return 0;
    }

    // Convert int16_t samples to FLAC__int32 for encoding
    // FLAC always works with 32-bit integers internally
    std::vector<FLAC__int32> buffer(num_samples);
    for (size_t i = 0; i < num_samples; ++i) {
        buffer[i] = samples[i];
    }

    // Calculate number of frames
    size_t num_frames = num_samples / channels_;

    // Process samples
    bool success = FLAC__stream_encoder_process_interleaved(encoder_, buffer.data(), num_frames);

    if (!success) {
        FLAC__StreamEncoderState state = FLAC__stream_encoder_get_state(encoder_);
        log_error("FLAC: Write failed: " + std::string(FLAC__StreamEncoderStateString[state]));
        return 0;
    }

    total_samples_ += num_samples;
    return num_samples;
}

size_t FlacWriter::WriteSamples(const std::vector<int16_t>& samples) {
    return WriteSamples(samples.data(), samples.size());
}

void FlacWriter::Close() {
    if (!encoder_) {
        return;
    }

    // Finish encoding
    FLAC__stream_encoder_finish(encoder_);

    // Clean up
    FLAC__stream_encoder_delete(encoder_);
    encoder_ = nullptr;

    // Get file size for compression ratio
    std::ifstream file(filename_, std::ios::binary | std::ios::ate);
    if (file.is_open()) {
        bytes_written_ = file.tellg();
        file.close();
    }

    log_info("FLAC encoder closed: " + filename_ + " (" + std::to_string(total_samples_) +
             " samples, " + std::to_string(bytes_written_) + " bytes, " +
             "ratio=" + std::to_string(GetCompressionRatio()) + "x)");
}

double FlacWriter::GetCompressionRatio() const {
    if (bytes_written_ == 0) {
        return 0.0;
    }

    // Calculate original size: samples * bytes_per_sample
    size_t original_size = total_samples_ * (bits_per_sample_ / 8);

    return static_cast<double>(original_size) / static_cast<double>(bytes_written_);
}

}  // namespace ffvoice
