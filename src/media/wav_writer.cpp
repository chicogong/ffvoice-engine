/**
 * @file wav_writer.cpp
 * @brief WAV file writer implementation
 */

#include "wav_writer.h"

#include "utils/logger.h"

#include <cstdint>
#include <cstring>
#include <limits>

namespace ffvoice {

namespace {
// Maximum size, in bytes, that the data chunk may reach. The RIFF container
// stores the data chunk size and the overall RIFF size as uint32_t fields, and
// the RIFF size is (data_size + 36). To keep both fields representable we cap
// the data chunk at UINT32_MAX - 36.
constexpr uint64_t kMaxWavDataBytes =
    static_cast<uint64_t>(std::numeric_limits<uint32_t>::max()) - 36u;
}  // namespace

WavWriter::~WavWriter() {
    Close();
}

bool WavWriter::Open(const std::string& filename, int sample_rate, int channels,
                     int bits_per_sample) {
    if (file_.is_open()) {
        Close();
    }

    sample_rate_ = sample_rate;
    channels_ = channels;
    bits_per_sample_ = bits_per_sample;
    total_samples_ = 0;
    size_limit_reached_ = false;

    file_.open(filename, std::ios::binary | std::ios::out);
    if (!file_) {
        return false;
    }

    // Write initial header (will be updated on close)
    WriteHeader();

    return true;
}

void WavWriter::WriteHeader() {
    // WAV file header structure (44 bytes for PCM)

    // RIFF chunk descriptor
    file_.write("RIFF", 4);
    uint32_t chunk_size = 0;  // Will be updated on close
    file_.write(reinterpret_cast<const char*>(&chunk_size), 4);
    file_.write("WAVE", 4);

    // fmt sub-chunk
    file_.write("fmt ", 4);
    uint32_t fmt_chunk_size = 16;  // PCM
    file_.write(reinterpret_cast<const char*>(&fmt_chunk_size), 4);

    uint16_t audio_format = 1;  // PCM
    file_.write(reinterpret_cast<const char*>(&audio_format), 2);

    uint16_t num_channels = static_cast<uint16_t>(channels_);
    file_.write(reinterpret_cast<const char*>(&num_channels), 2);

    uint32_t sample_rate = static_cast<uint32_t>(sample_rate_);
    file_.write(reinterpret_cast<const char*>(&sample_rate), 4);

    uint32_t byte_rate = sample_rate * channels_ * bits_per_sample_ / 8;
    file_.write(reinterpret_cast<const char*>(&byte_rate), 4);

    uint16_t block_align = static_cast<uint16_t>(channels_ * bits_per_sample_ / 8);
    file_.write(reinterpret_cast<const char*>(&block_align), 2);

    uint16_t bits = static_cast<uint16_t>(bits_per_sample_);
    file_.write(reinterpret_cast<const char*>(&bits), 2);

    // data sub-chunk
    file_.write("data", 4);
    uint32_t data_size = 0;  // Will be updated on close
    data_pos_ = file_.tellp();
    file_.write(reinterpret_cast<const char*>(&data_size), 4);
}

void WavWriter::UpdateHeader() {
    if (!file_.is_open()) {
        return;
    }

    // Calculate sizes
    uint32_t data_size = static_cast<uint32_t>(total_samples_ * bits_per_sample_ / 8);
    uint32_t chunk_size = data_size + 36;  // 36 = header size - 8

    // Update RIFF chunk size
    file_.seekp(4);
    file_.write(reinterpret_cast<const char*>(&chunk_size), 4);

    // Update data chunk size
    file_.seekp(data_pos_);
    file_.write(reinterpret_cast<const char*>(&data_size), 4);
}

size_t WavWriter::WriteSamples(const int16_t* samples, size_t num_samples) {
    if (!file_.is_open() || !samples) {
        return 0;
    }

    if (size_limit_reached_) {
        return 0;
    }

    // Guard against uint32_t overflow of the RIFF/data chunk size fields.
    // The data chunk size is total_samples_ * bits_per_sample_ / 8 bytes.
    // Reject the write if accepting these samples would push it past the limit.
    const uint64_t bytes_per_sample = static_cast<uint64_t>(bits_per_sample_) / 8u;
    const uint64_t current_data_bytes =
        static_cast<uint64_t>(total_samples_) * bytes_per_sample;
    const uint64_t incoming_data_bytes =
        static_cast<uint64_t>(num_samples) * bytes_per_sample;

    if (incoming_data_bytes > kMaxWavDataBytes - current_data_bytes) {
        size_limit_reached_ = true;
        log_error(
            "WavWriter: data size would exceed the 4 GB WAV/RIFF limit; "
            "no further samples will be written (existing data is intact)");
        return 0;
    }

    size_t bytes = num_samples * sizeof(int16_t);
    file_.write(reinterpret_cast<const char*>(samples), bytes);

    if (file_) {
        total_samples_ += num_samples;
        return num_samples;
    }

    return 0;
}

size_t WavWriter::WriteSamples(const std::vector<int16_t>& samples) {
    return WriteSamples(samples.data(), samples.size());
}

void WavWriter::Close() {
    if (file_.is_open()) {
        UpdateHeader();
        file_.close();
        total_samples_ = 0;
        size_limit_reached_ = false;
    }
}

}  // namespace ffvoice
