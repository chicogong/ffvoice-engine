/**
 * @file test_audio_pipeline.cpp
 * @brief Integration tests for complete audio processing pipelines
 *
 * These tests verify that multiple components work together correctly
 * in realistic scenarios, simulating end-to-end workflows.
 */

#include "audio/audio_processor.h"
#include "audio/rnnoise_processor.h"
#include "audio/vad_segmenter.h"
#include "media/wav_writer.h"
#include "media/flac_writer.h"
#include "utils/signal_generator.h"

#ifdef ENABLE_WHISPER
#include "audio/whisper_processor.h"
#include "utils/audio_converter.h"
#endif

#include <gtest/gtest.h>

#include <cstdio>
#include <memory>
#include <vector>

using namespace ffvoice;

class AudioPipelineTest : public ::testing::Test {
protected:
    void SetUp() override {
        // Clean up any leftover test files
        for (const auto& file : temp_files_) {
            std::remove(file.c_str());
        }
    }

    void TearDown() override {
        // Clean up test files
        for (const auto& file : temp_files_) {
            std::remove(file.c_str());
        }
    }

    void RegisterTempFile(const std::string& filename) {
        temp_files_.push_back(filename);
    }

    std::vector<std::string> temp_files_;
};

// =============================================================================
// Audio Processing Chain Integration Tests
// =============================================================================

TEST_F(AudioPipelineTest, ProcessorChain_VolumeAndFilter) {
    const int sample_rate = 48000;
    const int channels = 1;

    // Create processing chain: VolumeNormalizer → HighPassFilter
    AudioProcessorChain chain;
    chain.AddProcessor(std::make_unique<VolumeNormalizer>(0.5f));
    chain.AddProcessor(std::make_unique<HighPassFilter>(80.0f));

    ASSERT_TRUE(chain.Initialize(sample_rate, channels));

    // Generate test audio (sine wave at 440Hz)
    SignalGenerator generator;
    std::vector<int16_t> samples = generator.GenerateSineWave(440.0, 1.0, sample_rate, 0.3);

    // Process samples through chain
    chain.Process(samples.data(), samples.size());

    // Verify samples were processed (should be modified)
    bool all_zero = std::all_of(samples.begin(), samples.end(),
                                [](int16_t s) { return s == 0; });
    EXPECT_FALSE(all_zero) << "Processed samples should not all be zero";
}

#ifdef ENABLE_RNNOISE
TEST_F(AudioPipelineTest, ProcessorChain_WithRNNoise) {
    const int sample_rate = 48000;
    const int channels = 1;

    // Create chain with RNNoise
    AudioProcessorChain chain;
    chain.AddProcessor(std::make_unique<HighPassFilter>(80.0f));
    chain.AddProcessor(std::make_unique<RNNoiseProcessor>());
    chain.AddProcessor(std::make_unique<VolumeNormalizer>(0.5f));

    ASSERT_TRUE(chain.Initialize(sample_rate, channels));

    // Generate minimal test audio (20ms to process 2 RNNoise frames)
    SignalGenerator generator;
    auto speech = generator.GenerateSineWave(440.0, 0.02, sample_rate, 0.3);
    auto noise = generator.GenerateWhiteNoise(speech.size(), sample_rate, 0.1);

    // Mix speech + noise
    std::vector<int16_t> noisy_speech(speech.size());
    for (size_t i = 0; i < speech.size(); ++i) {
        noisy_speech[i] = static_cast<int16_t>(
            std::clamp(static_cast<int32_t>(speech[i]) + noise[i],
                       static_cast<int32_t>(INT16_MIN),
                       static_cast<int32_t>(INT16_MAX)));
    }

    // Process through RNNoise chain
    chain.Process(noisy_speech.data(), noisy_speech.size());

    // Verify samples were processed
    bool all_zero = std::all_of(noisy_speech.begin(), noisy_speech.end(),
                                [](int16_t s) { return s == 0; });
    EXPECT_FALSE(all_zero) << "Processed samples should not all be zero";
}
#endif

// =============================================================================
// Recording Pipeline Integration Tests
// =============================================================================

TEST_F(AudioPipelineTest, RecordingPipeline_WAV_WithProcessing) {
    const std::string output_file = "test_integration_recording.wav";
    RegisterTempFile(output_file);

    const int sample_rate = 48000;
    const int channels = 1;

    // Create processing chain
    AudioProcessorChain chain;
    chain.AddProcessor(std::make_unique<VolumeNormalizer>(0.5f));
    ASSERT_TRUE(chain.Initialize(sample_rate, channels));

    // Create WAV writer
    WavWriter writer;
    ASSERT_TRUE(writer.Open(output_file, sample_rate, channels, 16));

    // Generate test audio
    SignalGenerator generator;
    std::vector<int16_t> samples = generator.GenerateSineWave(440.0, 1.0, sample_rate, 0.3);

    // Process and write
    chain.Process(samples.data(), samples.size());
    size_t written = writer.WriteSamples(samples);
    EXPECT_EQ(written, samples.size());

    writer.Close();

    // Verify file was created
    std::ifstream file(output_file, std::ios::binary);
    EXPECT_TRUE(file.good()) << "Output WAV file should exist";
}

TEST_F(AudioPipelineTest, RecordingPipeline_FLAC_WithProcessing) {
    const std::string output_file = "test_integration_recording.flac";
    RegisterTempFile(output_file);

    const int sample_rate = 48000;
    const int channels = 1;

    // Create processing chain
    AudioProcessorChain chain;
    chain.AddProcessor(std::make_unique<HighPassFilter>(80.0f));
    ASSERT_TRUE(chain.Initialize(sample_rate, channels));

    // Create FLAC writer
    FlacWriter writer;
    ASSERT_TRUE(writer.Open(output_file, sample_rate, channels, 16, 5));

    // Generate and process audio
    SignalGenerator generator;
    std::vector<int16_t> samples = generator.GenerateSineWave(440.0, 2.0, sample_rate, 0.5);

    chain.Process(samples.data(), samples.size());
    size_t written = writer.WriteSamples(samples);
    EXPECT_EQ(written, samples.size());

    writer.Close();

    // Verify compression ratio
    double ratio = writer.GetCompressionRatio();
    EXPECT_GT(ratio, 1.0) << "FLAC should compress audio";
    EXPECT_LT(ratio, 10.0) << "Compression ratio should be reasonable";
}

// =============================================================================
// VAD Segmentation Pipeline Tests
// =============================================================================

#ifdef ENABLE_RNNOISE
TEST_F(AudioPipelineTest, VADPipeline_BasicIntegration) {
    const int sample_rate = 48000;

    // Create RNNoise processor with VAD
    RNNoiseConfig config;
    config.enable_vad = true;
    RNNoiseProcessor rnnoise(config);
    ASSERT_TRUE(rnnoise.Initialize(sample_rate, 1));

    // Create VAD segmenter
    VADSegmenter::Config vad_config = VADSegmenter::Config::FromPreset(
        VADSegmenter::Sensitivity::BALANCED);
    VADSegmenter segmenter(vad_config);

    // Track segment callbacks
    bool callback_invoked = false;
    auto segment_callback = [&callback_invoked](const int16_t* samples, size_t num_samples) {
        (void)samples;
        (void)num_samples;
        callback_invoked = true;
    };

    // Generate minimal test audio (just one RNNoise frame = 10ms)
    SignalGenerator generator;
    std::vector<int16_t> audio = generator.GenerateSineWave(440.0, 0.01, sample_rate, 0.5);

    // Process single frame
    rnnoise.Process(audio.data(), audio.size());
    float vad_prob = rnnoise.GetVADProbability();

    // Verify VAD probability is valid
    EXPECT_GE(vad_prob, 0.0f) << "VAD probability should be >= 0.0";
    EXPECT_LE(vad_prob, 1.0f) << "VAD probability should be <= 1.0";

    // Process through segmenter (may or may not trigger callback depending on VAD threshold)
    segmenter.ProcessFrame(audio.data(), audio.size(), vad_prob, segment_callback);
    segmenter.Flush(segment_callback);

    // This test just verifies the pipeline doesn't crash
    SUCCEED() << "VAD pipeline completed without errors";
}
#endif

// =============================================================================
// End-to-End Transcription Pipeline Tests
// =============================================================================

#if defined(ENABLE_WHISPER) && defined(ENABLE_RNNOISE)
TEST_F(AudioPipelineTest, FullPipeline_RecordProcessTranscribe) {
    const std::string wav_file = "test_full_pipeline.wav";
    RegisterTempFile(wav_file);

    const int sample_rate = 16000;  // Whisper-compatible
    const int channels = 1;

    // Step 1: Generate "recorded" audio with processing
    {
        AudioProcessorChain chain;
        chain.AddProcessor(std::make_unique<VolumeNormalizer>(0.5f));
        ASSERT_TRUE(chain.Initialize(sample_rate, channels));

        WavWriter writer;
        ASSERT_TRUE(writer.Open(wav_file, sample_rate, channels, 16));

        // Generate 2 seconds of test audio
        SignalGenerator generator;
        auto samples = generator.GenerateSineWave(440.0, 2.0, sample_rate, 0.3);

        chain.Process(samples.data(), samples.size());
        writer.WriteSamples(samples);
        writer.Close();
    }

    // Step 2: Transcribe the recorded file
    {
        // Check if model is available
        WhisperConfig config;
        if (config.model_path.empty()) {
            GTEST_SKIP() << "Whisper model not available, skipping transcription test";
        }

        std::ifstream model_file(config.model_path);
        if (!model_file.good()) {
            GTEST_SKIP() << "Whisper model file not found: " << config.model_path;
        }

        WhisperProcessor whisper(config);
        if (!whisper.Initialize()) {
            GTEST_SKIP() << "Failed to initialize Whisper: " << whisper.GetLastError();
        }

        std::vector<TranscriptionSegment> segments;
        bool result = whisper.TranscribeFile(wav_file, segments);

        EXPECT_TRUE(result) << "Transcription should succeed";
        // Sine wave may produce no/minimal transcription (expected)
        EXPECT_LE(segments.size(), 3) << "Sine wave should not produce many segments";
    }
}
#endif

// =============================================================================
// Error Recovery Integration Tests
// =============================================================================

TEST_F(AudioPipelineTest, ErrorRecovery_InvalidFileFormat) {
    const std::string invalid_file = "test_invalid.txt";
    RegisterTempFile(invalid_file);

    // Create invalid file
    std::ofstream file(invalid_file);
    file << "This is not audio data";
    file.close();

#ifdef ENABLE_WHISPER
    WhisperProcessor whisper;
    if (whisper.Initialize()) {
        std::vector<TranscriptionSegment> segments;
        bool result = whisper.TranscribeFile(invalid_file, segments);

        // Should handle gracefully
        EXPECT_FALSE(result) << "Should fail with invalid file";
        EXPECT_TRUE(segments.empty());
        EXPECT_FALSE(whisper.GetLastError().empty()) << "Should provide error message";
    }
#else
    GTEST_SKIP() << "WHISPER not enabled";
#endif
}

TEST_F(AudioPipelineTest, ErrorRecovery_ProcessorInitializationFailure) {
    // Test chain initialization with incompatible parameters
    AudioProcessorChain chain;
    chain.AddProcessor(std::make_unique<VolumeNormalizer>());

#ifdef ENABLE_RNNOISE
    chain.AddProcessor(std::make_unique<RNNoiseProcessor>());
#endif

    // Try to initialize with unsupported sample rate
    bool result = chain.Initialize(8000, 1);  // 8kHz may not be supported

#ifdef ENABLE_RNNOISE
    // With RNNoise, initialization should fail (unsupported sample rate)
    EXPECT_FALSE(result) << "Should fail with unsupported sample rate";
#else
    // Without RNNoise, only VolumeNormalizer is in chain, which accepts any sample rate
    EXPECT_TRUE(result) << "VolumeNormalizer should accept any sample rate";
#endif
}
