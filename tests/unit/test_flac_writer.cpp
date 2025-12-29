/**
 * @file test_flac_writer.cpp
 * @brief Unit tests for FlacWriter
 */

#include "media/flac_writer.h"
#include "utils/signal_generator.h"

#include <gtest/gtest.h>

#include <cstdio>
#include <fstream>
#include <vector>

using namespace ffvoice;

class FlacWriterTest : public ::testing::Test {
protected:
    void SetUp() override {
        test_file_ = "/tmp/test_flac_writer.flac";
    }

    void TearDown() override {
        // Clean up test file
        std::remove(test_file_.c_str());
    }

    // Helper: Get file size
    size_t GetFileSize(const std::string& filename) {
        std::ifstream file(filename, std::ios::binary | std::ios::ate);
        if (!file) return 0;
        return static_cast<size_t>(file.tellg());
    }

    // Helper: Check if file starts with fLaC magic
    bool HasFlacMagic(const std::string& filename) {
        std::ifstream file(filename, std::ios::binary);
        if (!file) return false;
        char magic[4];
        file.read(magic, 4);
        return file.good() && 
               magic[0] == 'f' && magic[1] == 'L' && 
               magic[2] == 'a' && magic[3] == 'C';
    }

    std::string test_file_;
};

// ============================================================================
// Basic Functionality Tests
// ============================================================================

TEST_F(FlacWriterTest, CreateAndOpen) {
    FlacWriter writer;
    EXPECT_FALSE(writer.IsOpen());
    
    EXPECT_TRUE(writer.Open(test_file_, 48000, 1));
    EXPECT_TRUE(writer.IsOpen());
    
    writer.Close();
    EXPECT_FALSE(writer.IsOpen());
}

TEST_F(FlacWriterTest, OpenInvalidPath) {
    FlacWriter writer;
    
    // Try to open in non-existent directory
    EXPECT_FALSE(writer.Open("/nonexistent/path/test.flac", 48000, 1));
    EXPECT_FALSE(writer.IsOpen());
}

TEST_F(FlacWriterTest, WriteSingleSample) {
    FlacWriter writer;
    ASSERT_TRUE(writer.Open(test_file_, 48000, 1));
    
    std::vector<int16_t> samples = {1000};
    size_t written = writer.WriteSamples(samples);
    EXPECT_EQ(written, 1u);
    EXPECT_EQ(writer.GetTotalSamples(), 1u);
    
    writer.Close();
    EXPECT_TRUE(HasFlacMagic(test_file_));
}

TEST_F(FlacWriterTest, WriteMultipleSamples) {
    FlacWriter writer;
    ASSERT_TRUE(writer.Open(test_file_, 48000, 1));
    
    // Generate 1 second of samples
    auto samples = SignalGenerator::GenerateSineWave(440.0, 1.0, 48000, 0.5);
    size_t written = writer.WriteSamples(samples);
    
    EXPECT_EQ(written, samples.size());
    EXPECT_EQ(writer.GetTotalSamples(), samples.size());
    
    writer.Close();
    EXPECT_TRUE(HasFlacMagic(test_file_));
}

TEST_F(FlacWriterTest, WriteMultipleTimes) {
    FlacWriter writer;
    ASSERT_TRUE(writer.Open(test_file_, 48000, 1));
    
    std::vector<int16_t> samples1(1000, 100);
    std::vector<int16_t> samples2(2000, 200);
    
    writer.WriteSamples(samples1);
    writer.WriteSamples(samples2);
    
    EXPECT_EQ(writer.GetTotalSamples(), 3000u);
    
    writer.Close();
}

// ============================================================================
// Compression Tests
// ============================================================================

TEST_F(FlacWriterTest, CompressionRatio) {
    FlacWriter writer;
    ASSERT_TRUE(writer.Open(test_file_, 48000, 1, 16, 5));  // Default compression
    
    // Write 1 second of sine wave
    auto samples = SignalGenerator::GenerateSineWave(440.0, 1.0, 48000, 0.5);
    writer.WriteSamples(samples);
    writer.Close();
    
    // FLAC should compress sine wave significantly
    size_t flac_size = GetFileSize(test_file_);
    size_t raw_size = samples.size() * sizeof(int16_t);
    
    // Expect at least 2x compression for sine wave
    EXPECT_LT(flac_size, raw_size);
    EXPECT_GT(flac_size, 0u);
    
    double ratio = static_cast<double>(raw_size) / flac_size;
    EXPECT_GT(ratio, 2.0);  // At least 2x compression
}

TEST_F(FlacWriterTest, CompressionLevels) {
    // Test different compression levels produce different sizes
    std::vector<size_t> sizes;
    auto samples = SignalGenerator::GenerateSineWave(440.0, 1.0, 48000, 0.5);
    
    for (int level : {0, 5, 8}) {
        std::string filename = "/tmp/test_flac_level_" + std::to_string(level) + ".flac";
        
        FlacWriter writer;
        ASSERT_TRUE(writer.Open(filename, 48000, 1, 16, level));
        writer.WriteSamples(samples);
        writer.Close();
        
        sizes.push_back(GetFileSize(filename));
        std::remove(filename.c_str());
    }
    
    // Higher compression levels should produce smaller or equal files
    // Level 0 (fastest) >= Level 5 (default) >= Level 8 (best)
    EXPECT_GE(sizes[0], sizes[1]);  // Level 0 >= Level 5
    EXPECT_GE(sizes[1], sizes[2]);  // Level 5 >= Level 8
}

// ============================================================================
// Sample Rate and Channel Tests
// ============================================================================

TEST_F(FlacWriterTest, SupportVariousSampleRates) {
    std::vector<int> sample_rates = {8000, 16000, 22050, 44100, 48000, 96000};
    
    for (int rate : sample_rates) {
        std::string filename = "/tmp/test_flac_rate_" + std::to_string(rate) + ".flac";
        
        FlacWriter writer;
        EXPECT_TRUE(writer.Open(filename, rate, 1)) 
            << "Failed to open FLAC with sample rate " << rate;
        
        // Write some samples
        std::vector<int16_t> samples(rate / 10, 1000);  // 0.1 second
        writer.WriteSamples(samples);
        writer.Close();
        
        EXPECT_TRUE(HasFlacMagic(filename));
        std::remove(filename.c_str());
    }
}

TEST_F(FlacWriterTest, SupportStereo) {
    FlacWriter writer;
    ASSERT_TRUE(writer.Open(test_file_, 48000, 2));  // Stereo
    
    // Generate stereo samples (interleaved)
    std::vector<int16_t> stereo_samples;
    for (int i = 0; i < 48000; ++i) {
        stereo_samples.push_back(static_cast<int16_t>(i % 1000));  // Left
        stereo_samples.push_back(static_cast<int16_t>(-i % 1000)); // Right
    }
    
    size_t written = writer.WriteSamples(stereo_samples);
    EXPECT_EQ(written, stereo_samples.size());
    
    writer.Close();
    EXPECT_TRUE(HasFlacMagic(test_file_));
}

// ============================================================================
// Edge Cases
// ============================================================================

TEST_F(FlacWriterTest, WriteToClosedFile) {
    FlacWriter writer;
    ASSERT_TRUE(writer.Open(test_file_, 48000, 1));
    writer.Close();
    
    std::vector<int16_t> samples = {1000, 2000, 3000};
    size_t written = writer.WriteSamples(samples);
    
    // Should fail gracefully
    EXPECT_EQ(written, 0u);
}

TEST_F(FlacWriterTest, CloseWithoutOpen) {
    FlacWriter writer;
    // Should not crash
    writer.Close();
    EXPECT_FALSE(writer.IsOpen());
}

TEST_F(FlacWriterTest, DoubleClose) {
    FlacWriter writer;
    ASSERT_TRUE(writer.Open(test_file_, 48000, 1));
    
    writer.Close();
    writer.Close();  // Should not crash
    
    EXPECT_FALSE(writer.IsOpen());
}

TEST_F(FlacWriterTest, ReopenAfterClose) {
    FlacWriter writer;
    
    // First file
    ASSERT_TRUE(writer.Open(test_file_, 48000, 1));
    std::vector<int16_t> samples1(1000, 100);
    writer.WriteSamples(samples1);
    writer.Close();
    
    // Second file
    std::string test_file2 = "/tmp/test_flac_writer2.flac";
    ASSERT_TRUE(writer.Open(test_file2, 44100, 2));
    std::vector<int16_t> samples2(2000, 200);
    writer.WriteSamples(samples2);
    writer.Close();
    
    // Both files should exist and be valid
    EXPECT_TRUE(HasFlacMagic(test_file_));
    EXPECT_TRUE(HasFlacMagic(test_file2));
    
    std::remove(test_file2.c_str());
}

TEST_F(FlacWriterTest, DestructorClosesFile) {
    {
        FlacWriter writer;
        ASSERT_TRUE(writer.Open(test_file_, 48000, 1));
        
        std::vector<int16_t> samples(1000, 500);
        writer.WriteSamples(samples);
        // Destructor should close file
    }
    
    // File should be valid after destructor
    EXPECT_TRUE(HasFlacMagic(test_file_));
    EXPECT_GT(GetFileSize(test_file_), 0u);
}

// ============================================================================
// Silence and Noise Tests  
// ============================================================================

TEST_F(FlacWriterTest, WriteSilence) {
    FlacWriter writer;
    ASSERT_TRUE(writer.Open(test_file_, 48000, 1));
    
    // Silence should compress very well
    auto silence = SignalGenerator::GenerateSilence(1.0, 48000);
    writer.WriteSamples(silence);
    writer.Close();
    
    size_t flac_size = GetFileSize(test_file_);
    size_t raw_size = silence.size() * sizeof(int16_t);
    
    // Silence should compress extremely well (>10x)
    double ratio = static_cast<double>(raw_size) / flac_size;
    EXPECT_GT(ratio, 5.0);  // At least 5x compression for silence
}

TEST_F(FlacWriterTest, WriteWhiteNoise) {
    FlacWriter writer;
    ASSERT_TRUE(writer.Open(test_file_, 48000, 1));
    
    // White noise doesn't compress as well
    auto noise = SignalGenerator::GenerateWhiteNoise(1.0, 48000, 0.5);
    writer.WriteSamples(noise);
    writer.Close();
    
    size_t flac_size = GetFileSize(test_file_);
    size_t raw_size = noise.size() * sizeof(int16_t);
    
    // Noise compresses less than structured signals
    // But FLAC should still provide some compression
    EXPECT_LT(flac_size, raw_size);
}
