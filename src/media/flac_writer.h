/**
 * @file flac_writer.h
 * @brief FLAC file writer using libFLAC
 */

#pragma once

#include <FLAC/stream_encoder.h>

#include <cstdint>
#include <memory>
#include <string>
#include <vector>

namespace ffvoice {

/**
 * @brief FLAC file writer for lossless audio compression
 *
 * Uses libFLAC to write compressed audio files with configurable
 * compression level. Supports mono/stereo, various sample rates.
 */
class FlacWriter {
public:
    FlacWriter() = default;
    ~FlacWriter();

    // Disable copy
    FlacWriter(const FlacWriter&) = delete;
    FlacWriter& operator=(const FlacWriter&) = delete;

    /**
     * @brief Open FLAC file for writing
     * @param filename Output file path
     * @param sample_rate Sample rate in Hz (e.g., 48000)
     * @param channels Number of channels (1=mono, 2=stereo)
     * @param bits_per_sample Bits per sample (16 or 24)
     * @param compression_level Compression level 0-8 (5=default, 8=max)
     * @return true if successful
     */
    bool Open(const std::string& filename, int sample_rate, int channels, int bits_per_sample = 16,
              int compression_level = 5);

    /**
     * @brief Write PCM samples to FLAC file
     * @param samples Pointer to sample data (int16_t for 16-bit)
     * @param num_samples Number of samples (not frames!)
     * @return Number of samples written
     */
    size_t WriteSamples(const int16_t* samples, size_t num_samples);

    /**
     * @brief Write PCM samples from vector
     */
    size_t WriteSamples(const std::vector<int16_t>& samples);

    /**
     * @brief Close the FLAC file and finalize encoding
     */
    void Close();

    /**
     * @brief Check if file is open
     */
    bool IsOpen() const {
        return encoder_ != nullptr;
    }

    /**
     * @brief Get total samples written
     */
    size_t GetTotalSamples() const {
        return total_samples_;
    }

    /**
     * @brief Get compression ratio (original_size / compressed_size)
     */
    double GetCompressionRatio() const;

private:
    FLAC__StreamEncoder* encoder_ = nullptr;
    std::string filename_;
    int sample_rate_ = 0;
    int channels_ = 0;
    int bits_per_sample_ = 0;
    size_t total_samples_ = 0;
    size_t bytes_written_ = 0;
};

}  // namespace ffvoice
