/**
 * @file test_wav_writer.cpp
 * @brief Unit tests for WavWriter
 */

#include <gtest/gtest.h>
#include <fstream>
#include <vector>
#include <cstdio>

#include "media/wav_writer.h"
#include "utils/signal_generator.h"

using namespace ffvoice;

class WavWriterTest : public ::testing::Test {
protected:
    void SetUp() override {
        test_file_ = "/tmp/test_wav_writer.wav";
    }

    void TearDown() override {
        // Clean up test file
        std::remove(test_file_.c_str());
    }

    // Helper: Read WAV file header
    struct WavHeader {
        char riff[4];
        uint32_t chunk_size;
        char wave[4];
        char fmt[4];
        uint32_t fmt_size;
        uint16_t audio_format;
        uint16_t num_channels;
        uint32_t sample_rate;
        uint32_t byte_rate;
        uint16_t block_align;
        uint16_t bits_per_sample;
        char data[4];
        uint32_t data_size;
    };

    bool ReadWavHeader(const std::string& filename, WavHeader& header) {
        std::ifstream file(filename, std::ios::binary);
        if (!file) return false;
        file.read(reinterpret_cast<char*>(&header), sizeof(WavHeader));
        return file.good();
    }

    std::string test_file_;
};

// ============================================================================
// Basic Functionality Tests
// ============================================================================

TEST_F(WavWriterTest, CreateAndOpen) {
    WavWriter writer;
    EXPECT_FALSE(writer.IsOpen());

    ASSERT_TRUE(writer.Open(test_file_, 48000, 1, 16));
    EXPECT_TRUE(writer.IsOpen());

    writer.Close();
    EXPECT_FALSE(writer.IsOpen());
}

TEST_F(WavWriterTest, OpenInvalidPath) {
    WavWriter writer;
    // Try to open in non-existent directory
    EXPECT_FALSE(writer.Open("/nonexistent/path/file.wav", 48000, 1, 16));
    EXPECT_FALSE(writer.IsOpen());
}

TEST_F(WavWriterTest, WriteSingleSample) {
    WavWriter writer;
    ASSERT_TRUE(writer.Open(test_file_, 48000, 1, 16));

    int16_t sample = 1000;
    size_t written = writer.WriteSamples(&sample, 1);

    EXPECT_EQ(written, 1);
    EXPECT_EQ(writer.GetTotalSamples(), 1);

    writer.Close();

    // Verify file exists
    std::ifstream file(test_file_);
    EXPECT_TRUE(file.good());
}

TEST_F(WavWriterTest, WriteMultipleSamples) {
    WavWriter writer;
    ASSERT_TRUE(writer.Open(test_file_, 48000, 1, 16));

    std::vector<int16_t> samples = {100, 200, 300, 400, 500};
    size_t written = writer.WriteSamples(samples);

    EXPECT_EQ(written, 5);
    EXPECT_EQ(writer.GetTotalSamples(), 5);

    writer.Close();
}

TEST_F(WavWriterTest, WriteMultipleTimes) {
    WavWriter writer;
    ASSERT_TRUE(writer.Open(test_file_, 48000, 1, 16));

    std::vector<int16_t> samples1 = {100, 200, 300};
    std::vector<int16_t> samples2 = {400, 500, 600};

    writer.WriteSamples(samples1);
    writer.WriteSamples(samples2);

    EXPECT_EQ(writer.GetTotalSamples(), 6);

    writer.Close();
}

// ============================================================================
// WAV Header Validation Tests
// ============================================================================

TEST_F(WavWriterTest, ValidateWavHeaderMono) {
    WavWriter writer;
    ASSERT_TRUE(writer.Open(test_file_, 48000, 1, 16));

    std::vector<int16_t> samples(1000, 0);
    writer.WriteSamples(samples);
    writer.Close();

    // Read and validate header
    WavHeader header;
    ASSERT_TRUE(ReadWavHeader(test_file_, header));

    // Check RIFF header
    EXPECT_EQ(std::string(header.riff, 4), "RIFF");
    EXPECT_EQ(std::string(header.wave, 4), "WAVE");

    // Check fmt chunk
    EXPECT_EQ(std::string(header.fmt, 4), "fmt ");
    EXPECT_EQ(header.fmt_size, 16);
    EXPECT_EQ(header.audio_format, 1);  // PCM
    EXPECT_EQ(header.num_channels, 1);  // Mono
    EXPECT_EQ(header.sample_rate, 48000);
    EXPECT_EQ(header.bits_per_sample, 16);

    // Check calculated fields
    uint32_t expected_byte_rate = 48000 * 1 * 16 / 8;
    EXPECT_EQ(header.byte_rate, expected_byte_rate);

    uint16_t expected_block_align = 1 * 16 / 8;
    EXPECT_EQ(header.block_align, expected_block_align);

    // Check data chunk
    EXPECT_EQ(std::string(header.data, 4), "data");
    EXPECT_EQ(header.data_size, 1000 * 2);  // 1000 samples * 2 bytes
}

TEST_F(WavWriterTest, ValidateWavHeaderStereo) {
    WavWriter writer;
    ASSERT_TRUE(writer.Open(test_file_, 44100, 2, 16));

    std::vector<int16_t> samples(2000, 0);  // 1000 frames * 2 channels
    writer.WriteSamples(samples);
    writer.Close();

    WavHeader header;
    ASSERT_TRUE(ReadWavHeader(test_file_, header));

    EXPECT_EQ(header.num_channels, 2);  // Stereo
    EXPECT_EQ(header.sample_rate, 44100);

    uint32_t expected_byte_rate = 44100 * 2 * 16 / 8;
    EXPECT_EQ(header.byte_rate, expected_byte_rate);

    uint16_t expected_block_align = 2 * 16 / 8;
    EXPECT_EQ(header.block_align, expected_block_align);
}

// ============================================================================
// Different Sample Rates Tests
// ============================================================================

TEST_F(WavWriterTest, SupportVariousSampleRates) {
    std::vector<int> sample_rates = {8000, 16000, 22050, 44100, 48000, 96000};

    for (int rate : sample_rates) {
        WavWriter writer;
        std::string filename = "/tmp/test_" + std::to_string(rate) + ".wav";

        ASSERT_TRUE(writer.Open(filename, rate, 1, 16))
            << "Failed to open with sample rate " << rate;

        std::vector<int16_t> samples(100, 0);
        writer.WriteSamples(samples);
        writer.Close();

        WavHeader header;
        ASSERT_TRUE(ReadWavHeader(filename, header));
        EXPECT_EQ(header.sample_rate, static_cast<uint32_t>(rate));

        std::remove(filename.c_str());
    }
}

// ============================================================================
// Integration with SignalGenerator Tests
// ============================================================================

TEST_F(WavWriterTest, WriteSineWave) {
    WavWriter writer;
    ASSERT_TRUE(writer.Open(test_file_, 48000, 1, 16));

    // Generate 1 second of 440Hz sine wave
    auto samples = SignalGenerator::GenerateSineWave(440.0, 1.0, 48000, 0.5);

    size_t written = writer.WriteSamples(samples);
    EXPECT_EQ(written, 48000);  // 1 second at 48kHz

    writer.Close();

    // Verify file size is reasonable
    std::ifstream file(test_file_, std::ios::binary | std::ios::ate);
    size_t file_size = file.tellg();
    size_t expected_size = 44 + (48000 * 2);  // Header + data
    EXPECT_EQ(file_size, expected_size);
}

TEST_F(WavWriterTest, WriteSilence) {
    WavWriter writer;
    ASSERT_TRUE(writer.Open(test_file_, 48000, 1, 16));

    auto samples = SignalGenerator::GenerateSilence(0.5, 48000);
    writer.WriteSamples(samples);
    writer.Close();

    EXPECT_EQ(samples.size(), 24000);  // 0.5 seconds

    // All samples should be zero
    for (const auto& sample : samples) {
        EXPECT_EQ(sample, 0);
    }
}

// ============================================================================
// Error Handling Tests
// ============================================================================

TEST_F(WavWriterTest, WriteToClosedFile) {
    WavWriter writer;
    std::vector<int16_t> samples = {100, 200, 300};

    // Try to write without opening
    size_t written = writer.WriteSamples(samples);
    EXPECT_EQ(written, 0);
}

TEST_F(WavWriterTest, WriteNullPointer) {
    WavWriter writer;
    ASSERT_TRUE(writer.Open(test_file_, 48000, 1, 16));

    size_t written = writer.WriteSamples(nullptr, 100);
    EXPECT_EQ(written, 0);

    writer.Close();
}

TEST_F(WavWriterTest, CloseWithoutOpen) {
    WavWriter writer;
    // Should not crash
    writer.Close();
    EXPECT_FALSE(writer.IsOpen());
}

TEST_F(WavWriterTest, DoubleClose) {
    WavWriter writer;
    ASSERT_TRUE(writer.Open(test_file_, 48000, 1, 16));

    writer.Close();
    EXPECT_FALSE(writer.IsOpen());

    // Second close should be safe
    writer.Close();
    EXPECT_FALSE(writer.IsOpen());
}

TEST_F(WavWriterTest, ReopenAfterClose) {
    WavWriter writer;

    // First open
    ASSERT_TRUE(writer.Open(test_file_, 48000, 1, 16));
    std::vector<int16_t> samples1 = {100, 200, 300};
    writer.WriteSamples(samples1);
    writer.Close();

    // Reopen with different settings
    ASSERT_TRUE(writer.Open(test_file_, 44100, 2, 16));
    std::vector<int16_t> samples2 = {400, 500, 600};
    writer.WriteSamples(samples2);
    writer.Close();

    // Verify second file has correct settings
    WavHeader header;
    ASSERT_TRUE(ReadWavHeader(test_file_, header));
    EXPECT_EQ(header.sample_rate, 44100);
    EXPECT_EQ(header.num_channels, 2);
}

// ============================================================================
// Destructor Test
// ============================================================================

TEST_F(WavWriterTest, DestructorClosesFile) {
    {
        WavWriter writer;
        ASSERT_TRUE(writer.Open(test_file_, 48000, 1, 16));
        std::vector<int16_t> samples = {100, 200, 300};
        writer.WriteSamples(samples);
        // writer goes out of scope, destructor should close file
    }

    // Verify file was properly closed and written
    WavHeader header;
    ASSERT_TRUE(ReadWavHeader(test_file_, header));
    EXPECT_EQ(header.data_size, 6);  // 3 samples * 2 bytes
}
