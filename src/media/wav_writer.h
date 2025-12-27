/**
 * @file wav_writer.h
 * @brief Simple WAV file writer (RIFF PCM format)
 */

#pragma once

#include <cstdint>
#include <fstream>
#include <string>
#include <vector>

namespace ffvoice {

/**
 * @brief Simple WAV file writer for PCM audio
 *
 * This is a minimal implementation that writes standard WAV files
 * with PCM data. Supports mono/stereo, various sample rates.
 */
class WavWriter {
public:
    WavWriter() = default;
    ~WavWriter();

    // Disable copy
    WavWriter(const WavWriter&) = delete;
    WavWriter& operator=(const WavWriter&) = delete;

    /**
     * @brief Open WAV file for writing
     * @param filename Output file path
     * @param sample_rate Sample rate in Hz (e.g., 48000)
     * @param channels Number of channels (1=mono, 2=stereo)
     * @param bits_per_sample Bits per sample (16 or 24)
     * @return true if successful
     */
    bool Open(const std::string& filename, int sample_rate, int channels, int bits_per_sample = 16);

    /**
     * @brief Write PCM samples to file
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
     * @brief Close the WAV file and finalize headers
     */
    void Close();

    /**
     * @brief Check if file is open
     */
    bool IsOpen() const {
        return file_.is_open();
    }

    /**
     * @brief Get total samples written
     */
    size_t GetTotalSamples() const {
        return total_samples_;
    }

private:
    void WriteHeader();
    void UpdateHeader();

    std::ofstream file_;
    int sample_rate_ = 0;
    int channels_ = 0;
    int bits_per_sample_ = 0;
    size_t total_samples_ = 0;
    std::streampos data_pos_ = 0;
};

}  // namespace ffvoice
