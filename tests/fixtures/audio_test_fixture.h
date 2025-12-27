/**
 * @file audio_test_fixture.h
 * @brief Test fixtures for audio-related unit tests
 *
 * Provides reusable test fixtures with common setup and teardown
 * operations for audio capture, playback, and processing tests.
 */

#ifndef FFVOICE_TESTS_FIXTURES_AUDIO_TEST_FIXTURE_H
#define FFVOICE_TESTS_FIXTURES_AUDIO_TEST_FIXTURE_H

#include <gmock/gmock.h>
#include <gtest/gtest.h>

#include <cmath>
#include <cstdint>
#include <fstream>
#include <memory>
#include <string>
#include <vector>

namespace ffvoice {
namespace test {

/**
 * @brief Audio format configuration for tests
 */
struct AudioConfig {
    uint32_t sample_rate = 16000;   // Sample rate in Hz
    uint16_t channels = 1;          // Number of channels (1=mono, 2=stereo)
    uint16_t bits_per_sample = 16;  // Bits per sample (8, 16, 24, 32)
    size_t buffer_size = 1024;      // Buffer size in frames

    /**
     * @brief Calculate bytes per frame
     */
    size_t bytes_per_frame() const {
        return channels * (bits_per_sample / 8);
    }

    /**
     * @brief Calculate bytes per second
     */
    size_t bytes_per_second() const {
        return sample_rate * bytes_per_frame();
    }
};

/**
 * @class AudioTestFixture
 * @brief Base fixture for audio-related tests
 *
 * Provides common setup/teardown and utility methods for testing
 * audio capture, playback, and processing components.
 */
class AudioTestFixture : public ::testing::Test {
protected:
    /**
     * @brief Set up test fixture
     *
     * Called before each test case. Initializes default audio
     * configuration and allocates test buffers.
     */
    void SetUp() override {
        // Initialize default audio configuration
        config_.sample_rate = 16000;
        config_.channels = 1;
        config_.bits_per_sample = 16;
        config_.buffer_size = 1024;

        // Allocate test buffers
        AllocateBuffers();
    }

    /**
     * @brief Tear down test fixture
     *
     * Called after each test case. Releases allocated resources.
     */
    void TearDown() override {
        // Clear buffers
        input_buffer_.clear();
        output_buffer_.clear();
        temp_files_.clear();
    }

    /**
     * @brief Allocate audio buffers based on current config
     */
    void AllocateBuffers() {
        size_t buffer_size_bytes = config_.buffer_size * config_.bytes_per_frame();
        input_buffer_.resize(buffer_size_bytes, 0);
        output_buffer_.resize(buffer_size_bytes, 0);
    }

    /**
     * @brief Generate silence (zeros)
     *
     * @param duration_ms Duration in milliseconds
     * @return Vector of audio samples (silence)
     */
    std::vector<int16_t> GenerateSilence(uint32_t duration_ms) {
        size_t num_samples = (config_.sample_rate * duration_ms) / 1000;
        return std::vector<int16_t>(num_samples, 0);
    }

    /**
     * @brief Generate sine wave
     *
     * @param frequency Frequency in Hz
     * @param duration_ms Duration in milliseconds
     * @param amplitude Amplitude (0.0 to 1.0)
     * @return Vector of audio samples (sine wave)
     */
    std::vector<int16_t> GenerateSineWave(double frequency, uint32_t duration_ms,
                                          double amplitude = 0.5) {
        size_t num_samples = (config_.sample_rate * duration_ms) / 1000;
        std::vector<int16_t> samples(num_samples);

        const double two_pi = 2.0 * M_PI;
        const int16_t max_amplitude = static_cast<int16_t>(32767 * amplitude);

        for (size_t i = 0; i < num_samples; ++i) {
            double t = static_cast<double>(i) / config_.sample_rate;
            double value = std::sin(two_pi * frequency * t);
            samples[i] = static_cast<int16_t>(value * max_amplitude);
        }

        return samples;
    }

    /**
     * @brief Generate white noise
     *
     * @param duration_ms Duration in milliseconds
     * @param amplitude Amplitude (0.0 to 1.0)
     * @return Vector of audio samples (white noise)
     */
    std::vector<int16_t> GenerateWhiteNoise(uint32_t duration_ms, double amplitude = 0.1) {
        size_t num_samples = (config_.sample_rate * duration_ms) / 1000;
        std::vector<int16_t> samples(num_samples);

        const int16_t max_amplitude = static_cast<int16_t>(32767 * amplitude);

        for (size_t i = 0; i < num_samples; ++i) {
            double random_value = (std::rand() / static_cast<double>(RAND_MAX)) * 2.0 - 1.0;
            samples[i] = static_cast<int16_t>(random_value * max_amplitude);
        }

        return samples;
    }

    /**
     * @brief Calculate RMS (Root Mean Square) of audio samples
     *
     * @param samples Audio samples
     * @return RMS value
     */
    double CalculateRMS(const std::vector<int16_t>& samples) {
        if (samples.empty())
            return 0.0;

        double sum = 0.0;
        for (int16_t sample : samples) {
            sum += static_cast<double>(sample) * sample;
        }

        return std::sqrt(sum / samples.size());
    }

    /**
     * @brief Calculate SNR (Signal-to-Noise Ratio)
     *
     * @param signal Signal samples
     * @param noise Noise samples
     * @return SNR in dB
     */
    double CalculateSNR(const std::vector<int16_t>& signal, const std::vector<int16_t>& noise) {
        double signal_rms = CalculateRMS(signal);
        double noise_rms = CalculateRMS(noise);

        if (noise_rms == 0.0)
            return INFINITY;

        return 20.0 * std::log10(signal_rms / noise_rms);
    }

    /**
     * @brief Compare two audio buffers with tolerance
     *
     * @param expected Expected audio samples
     * @param actual Actual audio samples
     * @param tolerance Maximum allowed difference per sample
     * @return True if buffers match within tolerance
     */
    bool CompareAudioBuffers(const std::vector<int16_t>& expected,
                             const std::vector<int16_t>& actual, int16_t tolerance = 1) {
        if (expected.size() != actual.size())
            return false;

        for (size_t i = 0; i < expected.size(); ++i) {
            int16_t diff = std::abs(expected[i] - actual[i]);
            if (diff > tolerance) {
                return false;
            }
        }

        return true;
    }

    /**
     * @brief Write audio samples to WAV file for debugging
     *
     * @param filename Output filename
     * @param samples Audio samples
     */
    void WriteWavFile(const std::string& filename, const std::vector<int16_t>& samples) {
        std::ofstream file(filename, std::ios::binary);
        if (!file)
            return;

        // WAV header (simplified, mono 16-bit PCM)
        uint32_t data_size = samples.size() * sizeof(int16_t);
        uint32_t file_size = 36 + data_size;

        // RIFF header
        file.write("RIFF", 4);
        file.write(reinterpret_cast<const char*>(&file_size), 4);
        file.write("WAVE", 4);

        // fmt chunk
        file.write("fmt ", 4);
        uint32_t fmt_size = 16;
        file.write(reinterpret_cast<const char*>(&fmt_size), 4);
        uint16_t audio_format = 1;  // PCM
        file.write(reinterpret_cast<const char*>(&audio_format), 2);
        file.write(reinterpret_cast<const char*>(&config_.channels), 2);
        file.write(reinterpret_cast<const char*>(&config_.sample_rate), 4);
        uint32_t byte_rate = config_.bytes_per_second();
        file.write(reinterpret_cast<const char*>(&byte_rate), 4);
        uint16_t block_align = config_.bytes_per_frame();
        file.write(reinterpret_cast<const char*>(&block_align), 2);
        file.write(reinterpret_cast<const char*>(&config_.bits_per_sample), 2);

        // data chunk
        file.write("data", 4);
        file.write(reinterpret_cast<const char*>(&data_size), 4);
        file.write(reinterpret_cast<const char*>(samples.data()), data_size);

        file.close();
        temp_files_.push_back(filename);
    }

    /**
     * @brief Get test data directory path
     */
    std::string GetTestDataPath() const {
        return "./data/";
    }

    // Protected member variables
    AudioConfig config_;                   // Current audio configuration
    std::vector<uint8_t> input_buffer_;    // Input buffer
    std::vector<uint8_t> output_buffer_;   // Output buffer
    std::vector<std::string> temp_files_;  // Temporary files to cleanup
};

/**
 * @class AudioCaptureTestFixture
 * @brief Specialized fixture for audio capture tests
 */
class AudioCaptureTestFixture : public AudioTestFixture {
protected:
    void SetUp() override {
        AudioTestFixture::SetUp();
        // Additional capture-specific setup
    }

    void TearDown() override {
        // Capture-specific cleanup
        AudioTestFixture::TearDown();
    }
};

/**
 * @class AudioPlaybackTestFixture
 * @brief Specialized fixture for audio playback tests
 */
class AudioPlaybackTestFixture : public AudioTestFixture {
protected:
    void SetUp() override {
        AudioTestFixture::SetUp();
        // Additional playback-specific setup
    }

    void TearDown() override {
        // Playback-specific cleanup
        AudioTestFixture::TearDown();
    }
};

/**
 * @class AudioProcessingTestFixture
 * @brief Specialized fixture for audio processing tests
 */
class AudioProcessingTestFixture : public AudioTestFixture {
protected:
    void SetUp() override {
        AudioTestFixture::SetUp();
        // Additional processing-specific setup
    }

    void TearDown() override {
        // Processing-specific cleanup
        AudioTestFixture::TearDown();
    }
};

}  // namespace test
}  // namespace ffvoice

#endif  // FFVOICE_TESTS_FIXTURES_AUDIO_TEST_FIXTURE_H
