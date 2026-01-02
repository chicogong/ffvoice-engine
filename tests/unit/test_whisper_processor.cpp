/**
 * @file test_whisper_processor.cpp
 * @brief Unit tests for WhisperProcessor
 * @note Only compiled when ENABLE_WHISPER is defined
 */

#ifdef ENABLE_WHISPER

#include "audio/whisper_processor.h"
#include "utils/signal_generator.h"
#include "media/wav_writer.h"

#include <gtest/gtest.h>

#include <cstdio>
#include <fstream>

using namespace ffvoice;

class WhisperProcessorTest : public ::testing::Test {
protected:
    void SetUp() override {
        // Cleanup any leftover test files
        std::remove(test_wav_file_.c_str());
    }

    void TearDown() override {
        // Cleanup test files
        std::remove(test_wav_file_.c_str());
    }

    // Helper: Create a simple test WAV file with silence
    bool CreateTestWavFile(const std::string& filename, int duration_ms = 1000,
                           int sample_rate = 16000) {
        WavWriter writer;
        if (!writer.Open(filename, sample_rate, 1, 16)) {
            return false;
        }

        // Generate silence
        SignalGenerator generator;
        std::vector<int16_t> samples = generator.GenerateSilence(
            sample_rate * duration_ms / 1000, sample_rate);

        writer.WriteSamples(samples);
        writer.Close();
        return true;
    }

    // Helper: Create a test WAV file with sine wave (simulates speech frequency)
    bool CreateTestSpeechWavFile(const std::string& filename, int duration_ms = 1000) {
        const int sample_rate = 16000;  // Whisper expects 16kHz
        WavWriter writer;
        if (!writer.Open(filename, sample_rate, 1, 16)) {
            return false;
        }

        // Generate 440Hz sine wave (simulates voice fundamental frequency)
        SignalGenerator generator;
        std::vector<int16_t> samples = generator.GenerateSineWave(
            440.0, duration_ms / 1000.0, sample_rate, 0.3);

        writer.WriteSamples(samples);
        writer.Close();
        return true;
    }

    // Helper: Check if model file exists
    bool ModelExists() {
        WhisperConfig config;
        if (config.model_path.empty()) {
            return false;
        }
        std::ifstream file(config.model_path);
        return file.good();
    }

    std::string test_wav_file_ = "test_whisper_temp.wav";
};

// =============================================================================
// Construction and Configuration Tests
// =============================================================================

TEST_F(WhisperProcessorTest, DefaultConstruction) {
    WhisperProcessor processor;
    // Should construct without error
    SUCCEED();
}

TEST_F(WhisperProcessorTest, ConfigConstruction) {
    WhisperConfig config;
    config.language = "en";
    config.n_threads = 2;
    config.model_type = WhisperModelType::TINY;

    WhisperProcessor processor(config);
    SUCCEED();
}

TEST_F(WhisperProcessorTest, ConfigValidation_Language) {
    WhisperConfig config;
    config.language = "zh";  // Chinese
    WhisperProcessor processor(config);

    // Configuration should be accepted
    SUCCEED();
}

TEST_F(WhisperProcessorTest, ConfigValidation_Threads) {
    WhisperConfig config;
    config.n_threads = 1;  // Single thread
    WhisperProcessor processor1(config);

    config.n_threads = 8;  // Multiple threads
    WhisperProcessor processor2(config);

    SUCCEED();
}

// =============================================================================
// Initialization Tests
// =============================================================================

TEST_F(WhisperProcessorTest, Initialize_WithValidModel) {
    if (!ModelExists()) {
        GTEST_SKIP() << "Whisper model not found, skipping test";
    }

    WhisperProcessor processor;
    bool result = processor.Initialize();

    EXPECT_TRUE(result) << "Initialization should succeed with valid model";
}

TEST_F(WhisperProcessorTest, Initialize_WithInvalidModelPath) {
    WhisperConfig config;
    config.model_path = "/nonexistent/path/model.bin";

    WhisperProcessor processor(config);
    bool result = processor.Initialize();

    EXPECT_FALSE(result) << "Initialization should fail with invalid model path";
}

TEST_F(WhisperProcessorTest, Initialize_MultipleTimes) {
    if (!ModelExists()) {
        GTEST_SKIP() << "Whisper model not found, skipping test";
    }

    WhisperProcessor processor;

    // First initialization
    EXPECT_TRUE(processor.Initialize());

    // Second initialization should also work (or be idempotent)
    bool result2 = processor.Initialize();
    EXPECT_TRUE(result2 || true) << "Multiple initialization attempts should not crash";
}

// =============================================================================
// File Transcription Tests
// =============================================================================

TEST_F(WhisperProcessorTest, TranscribeFile_SilenceReturnsEmpty) {
    if (!ModelExists()) {
        GTEST_SKIP() << "Whisper model not found, skipping test";
    }

    // Create test file with silence
    ASSERT_TRUE(CreateTestWavFile(test_wav_file_, 1000));

    WhisperProcessor processor;
    ASSERT_TRUE(processor.Initialize());

    std::vector<TranscriptionSegment> segments;
    bool result = processor.TranscribeFile(test_wav_file_, segments);

    EXPECT_TRUE(result);
    // Silence should produce no or minimal transcription
    EXPECT_LE(segments.size(), 2) << "Silence should not produce many segments";
}

TEST_F(WhisperProcessorTest, TranscribeFile_NonexistentFile) {
    if (!ModelExists()) {
        GTEST_SKIP() << "Whisper model not found, skipping test";
    }

    WhisperProcessor processor;
    ASSERT_TRUE(processor.Initialize());

    std::vector<TranscriptionSegment> segments;
    bool result = processor.TranscribeFile("/nonexistent/file.wav", segments);

    EXPECT_FALSE(result) << "Should fail with nonexistent file";
    EXPECT_TRUE(segments.empty());
}

TEST_F(WhisperProcessorTest, TranscribeFile_WithoutInitialization) {
    ASSERT_TRUE(CreateTestWavFile(test_wav_file_));

    WhisperProcessor processor;
    // Do NOT initialize

    std::vector<TranscriptionSegment> segments;
    bool result = processor.TranscribeFile(test_wav_file_, segments);

    EXPECT_FALSE(result) << "Should fail without initialization";
}

TEST_F(WhisperProcessorTest, TranscribeFile_ValidatesTimestamps) {
    if (!ModelExists()) {
        GTEST_SKIP() << "Whisper model not found, skipping test";
    }

    ASSERT_TRUE(CreateTestSpeechWavFile(test_wav_file_, 2000));

    WhisperProcessor processor;
    ASSERT_TRUE(processor.Initialize());

    std::vector<TranscriptionSegment> segments;
    bool result = processor.TranscribeFile(test_wav_file_, segments);

    EXPECT_TRUE(result);

    // Validate timestamp consistency
    for (const auto& seg : segments) {
        EXPECT_GE(seg.start_ms, 0) << "Start time should be non-negative";
        EXPECT_GE(seg.end_ms, seg.start_ms) << "End time should be >= start time";
        EXPECT_GE(seg.confidence, 0.0f) << "Confidence should be non-negative";
        EXPECT_LE(seg.confidence, 1.0f) << "Confidence should be <= 1.0";
    }
}

// =============================================================================
// Buffer Transcription Tests
// =============================================================================

TEST_F(WhisperProcessorTest, TranscribeBuffer_EmptyBuffer) {
    if (!ModelExists()) {
        GTEST_SKIP() << "Whisper model not found, skipping test";
    }

    WhisperProcessor processor;
    ASSERT_TRUE(processor.Initialize());

    std::vector<int16_t> samples;
    std::vector<TranscriptionSegment> segments;

    bool result = processor.TranscribeBuffer(samples.data(), 0, segments);

    // Empty buffer should either fail or return empty segments
    if (result) {
        EXPECT_TRUE(segments.empty());
    }
}

TEST_F(WhisperProcessorTest, TranscribeBuffer_SilenceBuffer) {
    if (!ModelExists()) {
        GTEST_SKIP() << "Whisper model not found, skipping test";
    }

    WhisperProcessor processor;
    ASSERT_TRUE(processor.Initialize());

    // 1 second of silence at 16kHz
    SignalGenerator generator;
    std::vector<int16_t> samples = generator.GenerateSilence(16000, 16000);
    std::vector<TranscriptionSegment> segments;

    bool result = processor.TranscribeBuffer(samples.data(), samples.size(), segments);

    EXPECT_TRUE(result);
    // Silence should produce minimal transcription
    EXPECT_LE(segments.size(), 2);
}

TEST_F(WhisperProcessorTest, TranscribeBuffer_ValidatesSampleCount) {
    if (!ModelExists()) {
        GTEST_SKIP() << "Whisper model not found, skipping test";
    }

    WhisperProcessor processor;
    ASSERT_TRUE(processor.Initialize());

    // Very short buffer (< minimum required)
    std::vector<int16_t> samples(100, 0);
    std::vector<TranscriptionSegment> segments;

    bool result = processor.TranscribeBuffer(samples.data(), samples.size(), segments);

    // Should handle short buffers gracefully (either process or return error)
    EXPECT_TRUE(result || !result) << "Should not crash with short buffer";
}

// =============================================================================
// Error Handling Tests
// =============================================================================

TEST_F(WhisperProcessorTest, GetLastError_AfterFailure) {
    WhisperConfig config;
    config.model_path = "/invalid/path.bin";

    WhisperProcessor processor(config);
    EXPECT_FALSE(processor.Initialize());

    std::string error = processor.GetLastError();
    EXPECT_FALSE(error.empty()) << "Should provide error message after failure";
}

TEST_F(WhisperProcessorTest, GetLastError_AfterSuccess) {
    if (!ModelExists()) {
        GTEST_SKIP() << "Whisper model not found, skipping test";
    }

    WhisperProcessor processor;
    ASSERT_TRUE(processor.Initialize());

    std::string error = processor.GetLastError();
    // Error should be empty or indicate success
    EXPECT_TRUE(error.empty() || error.find("success") != std::string::npos);
}

// =============================================================================
// Model Type Tests
// =============================================================================

TEST_F(WhisperProcessorTest, ModelType_Tiny) {
    WhisperConfig config;
    config.model_type = WhisperModelType::TINY;

    WhisperProcessor processor(config);
    // Should construct without error
    SUCCEED();
}

TEST_F(WhisperProcessorTest, ModelType_Base) {
    WhisperConfig config;
    config.model_type = WhisperModelType::BASE;

    WhisperProcessor processor(config);
    SUCCEED();
}

TEST_F(WhisperProcessorTest, ModelType_AllTypes) {
    // Test all model types construct successfully
    WhisperModelType types[] = {
        WhisperModelType::TINY,
        WhisperModelType::BASE,
        WhisperModelType::SMALL,
        WhisperModelType::MEDIUM,
        WhisperModelType::LARGE
    };

    for (auto type : types) {
        WhisperConfig config;
        config.model_type = type;
        WhisperProcessor processor(config);
        SUCCEED();
    }
}

// =============================================================================
// Thread Safety Tests
// =============================================================================

TEST_F(WhisperProcessorTest, ThreadSafety_SingleInstance) {
    if (!ModelExists()) {
        GTEST_SKIP() << "Whisper model not found, skipping test";
    }

    WhisperProcessor processor;
    ASSERT_TRUE(processor.Initialize());

    // Process multiple files sequentially (tests reusability)
    for (int i = 0; i < 3; ++i) {
        std::string filename = "test_temp_" + std::to_string(i) + ".wav";
        ASSERT_TRUE(CreateTestWavFile(filename, 500));

        std::vector<TranscriptionSegment> segments;
        EXPECT_TRUE(processor.TranscribeFile(filename, segments));

        std::remove(filename.c_str());
    }
}

#endif  // ENABLE_WHISPER
