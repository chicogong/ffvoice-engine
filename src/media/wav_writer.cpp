/**
 * @file wav_writer.cpp
 * @brief WAV file writer implementation
 */

#include "wav_writer.h"

#include <cstring>

namespace ffvoice {

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
    }
}

}  // namespace ffvoice
