/**
 * @file test_architecture_example.cpp
 * @brief Examples demonstrating test architecture usage
 *
 * This file provides comprehensive examples of using the test
 * architecture components: fixtures, mocks, signal generators,
 * and helpers.
 */

#include <gtest/gtest.h>
#include <gmock/gmock.h>

#include "fixtures/audio_test_fixture.h"
#include "mocks/mock_audio_device.h"
#include "mocks/mock_file_system.h"
#include "utils/test_signal_generator.h"
#include "utils/test_helpers.h"

using namespace ffvoice::test;
using ::testing::Return;
using ::testing::_;

// ============================================================================
// Example 1: Using Test Signal Generator
// ============================================================================

class SignalGeneratorExample : public ::testing::Test {
protected:
    TestSignalGenerator generator_{16000};  // 16kHz sample rate
};

TEST_F(SignalGeneratorExample, GenerateBasicWaveforms) {
    // Generate various waveforms
    auto sine = generator_.GenerateSineWave(440.0, 1000, 0.5);
    auto square = generator_.GenerateSquareWave(440.0, 1000, 0.5);
    auto triangle = generator_.GenerateTriangleWave(440.0, 1000, 0.5);
    auto sawtooth = generator_.GenerateSawtoothWave(440.0, 1000, 0.5);

    // Verify they have the expected length (16000 samples/sec * 1 sec)
    EXPECT_EQ(sine.size(), 16000);
    EXPECT_EQ(square.size(), 16000);
    EXPECT_EQ(triangle.size(), 16000);
    EXPECT_EQ(sawtooth.size(), 16000);

    // Verify they're not all zeros
    EXPECT_GT(TestHelpers::CalculateRMS(sine), 0.0);
    EXPECT_GT(TestHelpers::CalculateRMS(square), 0.0);
}

TEST_F(SignalGeneratorExample, GenerateNoiseSignals) {
    // Generate different types of noise
    auto white_noise = generator_.GenerateWhiteNoise(1000, 0.1);
    auto pink_noise = generator_.GeneratePinkNoise(1000, 0.1);

    // White noise should have high zero crossing rate
    size_t white_crossings = TestHelpers::CountZeroCrossings(white_noise);
    EXPECT_GT(white_crossings, white_noise.size() / 4);

    // Both should have non-zero energy
    EXPECT_GT(TestHelpers::CalculateEnergy(white_noise), 0.0);
    EXPECT_GT(TestHelpers::CalculateEnergy(pink_noise), 0.0);
}

TEST_F(SignalGeneratorExample, GenerateComplexSignals) {
    // Generate DTMF tone (dual tone)
    auto dtmf = generator_.GenerateDTMF('5', 200);
    EXPECT_GT(dtmf.size(), 0);

    // Generate frequency sweep (chirp)
    auto chirp = generator_.GenerateChirp(100.0, 1000.0, 1000);
    EXPECT_EQ(chirp.size(), 16000);

    // Generate impulse
    auto impulse = generator_.GenerateImpulse(100, 0.5, 1.0);
    EXPECT_EQ(impulse.size(), 1600);  // 100ms at 16kHz
}

TEST_F(SignalGeneratorExample, MixMultipleSignals) {
    // Create multiple signals
    auto tone1 = generator_.GenerateSineWave(440.0, 500, 0.3);
    auto tone2 = generator_.GenerateSineWave(880.0, 500, 0.3);
    auto tone3 = generator_.GenerateSineWave(1320.0, 500, 0.3);

    // Mix them together
    std::vector<std::vector<int16_t>> signals = {tone1, tone2, tone3};
    auto mixed = generator_.MixSignals(signals, 1.0);

    // Verify mixed signal has appropriate length
    EXPECT_EQ(mixed.size(), tone1.size());

    // Mixed signal should have higher complexity
    EXPECT_GT(TestHelpers::CalculateEnergy(mixed),
              TestHelpers::CalculateEnergy(tone1));
}

// ============================================================================
// Example 2: Using Test Helpers
// ============================================================================

class TestHelpersExample : public ::testing::Test {
protected:
    TestSignalGenerator generator_{16000};
};

TEST_F(TestHelpersExample, CompareFloatingPointValues) {
    double a = 0.1 + 0.2;
    double b = 0.3;

    // Direct comparison would fail
    // EXPECT_EQ(a, b);  // This might fail due to floating point precision

    // Use approximate comparison
    EXPECT_TRUE(TestHelpers::ApproximatelyEqual(a, b, 1e-10));
}

TEST_F(TestHelpersExample, AnalyzeSignalQuality) {
    // Generate reference signal
    auto reference = generator_.GenerateSineWave(1000.0, 100, 0.5);

    // Simulate slight degradation
    auto degraded = reference;
    for (auto& sample : degraded) {
        sample = static_cast<int16_t>(sample * 0.95);  // 5% amplitude reduction
    }

    // Calculate metrics
    double mse = TestHelpers::CalculateMSE(reference, degraded);
    double correlation = TestHelpers::CalculateCorrelation(reference, degraded);
    double rms_ref = TestHelpers::CalculateRMS(reference);
    double rms_deg = TestHelpers::CalculateRMS(degraded);

    // Verify quality metrics
    EXPECT_LT(mse, 1000.0);  // Low error
    EXPECT_GT(correlation, 0.99);  // High correlation
    EXPECT_LT(rms_deg, rms_ref);  // Reduced amplitude
}

TEST_F(TestHelpersExample, ConvertDecibels) {
    // Convert amplitude to dB
    double amplitude = 16384.0;  // Half of max int16_t
    double db = TestHelpers::AmplitudeToDecibels(amplitude);

    // -6dB is approximately half amplitude
    EXPECT_TRUE(TestHelpers::ApproximatelyEqual(db, -6.0, 0.1));

    // Convert back
    double converted_back = TestHelpers::DecibelsToAmplitude(db);
    EXPECT_TRUE(TestHelpers::ApproximatelyEqual(amplitude, converted_back, 1.0));
}

TEST_F(TestHelpersExample, MeasurePerformance) {
    // Measure execution time of an operation
    std::vector<int16_t> large_signal(100000);

    double elapsed = TestHelpers::MeasureExecutionTime([&]() {
        // Simulate some processing
        for (size_t i = 0; i < large_signal.size(); ++i) {
            large_signal[i] = static_cast<int16_t>(i % 32768);
        }
    });

    // Should complete quickly
    EXPECT_LT(elapsed, 100.0);  // Less than 100ms
    std::cout << "Operation took " << elapsed << " ms" << std::endl;
}

// ============================================================================
// Example 3: Using Audio Test Fixture
// ============================================================================

class AudioFixtureExample : public AudioTestFixture {
protected:
    void SetUp() override {
        AudioTestFixture::SetUp();

        // Customize audio configuration
        config_.sample_rate = 48000;
        config_.channels = 2;  // Stereo
        config_.bits_per_sample = 16;
        config_.buffer_size = 2048;

        // Reallocate buffers with new configuration
        AllocateBuffers();
    }
};

TEST_F(AudioFixtureExample, UseFixtureSignalGeneration) {
    // Use fixture methods to generate signals
    auto silence = GenerateSilence(1000);
    EXPECT_EQ(CalculateRMS(silence), 0.0);

    auto sine = GenerateSineWave(440.0, 1000, 0.5);
    EXPECT_GT(CalculateRMS(sine), 0.0);

    auto noise = GenerateWhiteNoise(500, 0.1);
    EXPECT_GT(CalculateRMS(noise), 0.0);
}

TEST_F(AudioFixtureExample, CompareAudioBuffers) {
    // Generate two similar signals
    auto signal1 = GenerateSineWave(440.0, 500, 0.5);
    auto signal2 = GenerateSineWave(440.0, 500, 0.5);

    // Should match exactly
    EXPECT_TRUE(CompareAudioBuffers(signal1, signal2, 0));

    // Add slight variation
    signal2[100] += 5;

    // Should still match with tolerance
    EXPECT_TRUE(CompareAudioBuffers(signal1, signal2, 10));

    // Should not match with strict tolerance
    EXPECT_FALSE(CompareAudioBuffers(signal1, signal2, 1));
}

TEST_F(AudioFixtureExample, CalculateSignalMetrics) {
    // Generate test signals
    auto signal = GenerateSineWave(1000.0, 1000, 0.7);
    auto noise = GenerateWhiteNoise(1000, 0.1);

    // Calculate SNR
    double snr = CalculateSNR(signal, noise);
    EXPECT_GT(snr, 10.0);  // Should have good SNR

    // Calculate RMS
    double rms = CalculateRMS(signal);
    EXPECT_GT(rms, 0.0);
    EXPECT_LT(rms, 32767.0);
}

// ============================================================================
// Example 4: Using Mock Audio Devices
// ============================================================================

class MockDeviceExample : public ::testing::Test {
protected:
    void SetUp() override {
        capture_device_ = std::make_shared<MockAudioCaptureDevice>();
        playback_device_ = std::make_shared<MockAudioPlaybackDevice>();
        device_manager_ = std::make_shared<MockAudioDeviceManager>();
        generator_ = std::make_unique<TestSignalGenerator>(16000);
    }

    std::shared_ptr<MockAudioCaptureDevice> capture_device_;
    std::shared_ptr<MockAudioPlaybackDevice> playback_device_;
    std::shared_ptr<MockAudioDeviceManager> device_manager_;
    std::unique_ptr<TestSignalGenerator> generator_;
};

TEST_F(MockDeviceExample, SimulateCaptureDevice) {
    // Generate test audio
    auto test_signal = generator_->GenerateSineWave(440.0, 1000);

    // Configure mock to return this data
    capture_device_->SimulateCapturedData(test_signal);

    // Capture audio
    std::vector<int16_t> captured(test_signal.size());
    size_t read = capture_device_->Read(captured.data(), captured.size());

    // Verify
    EXPECT_EQ(read, test_signal.size());
    EXPECT_EQ(captured, test_signal);
}

TEST_F(MockDeviceExample, CapturePlaybackData) {
    // Configure mock to capture what's played
    playback_device_->CapturePlaybackData();

    // Play some audio
    auto test_signal = generator_->GenerateSineWave(880.0, 500);
    playback_device_->Write(test_signal.data(), test_signal.size());

    // Verify what was played
    const auto& played = playback_device_->GetPlayedData();
    EXPECT_EQ(played.size(), test_signal.size());
    EXPECT_EQ(played, test_signal);
}

TEST_F(MockDeviceExample, EnumerateDevices) {
    // Set up mock device manager
    device_manager_->SetupDefaultDevices();

    // Get default devices
    auto input = device_manager_->GetDefaultInputDevice();
    auto output = device_manager_->GetDefaultOutputDevice();

    // Verify device info
    EXPECT_TRUE(input.is_input);
    EXPECT_FALSE(output.is_input);
    EXPECT_TRUE(input.is_default);
    EXPECT_TRUE(output.is_default);
}

// ============================================================================
// Example 5: Using Mock File System
// ============================================================================

class MockFileSystemExample : public ::testing::Test {
protected:
    void SetUp() override {
        mock_fs_ = std::make_shared<MockFileSystem>();
        mock_fs_->SetupVirtualFileSystem();
        generator_ = std::make_unique<TestSignalGenerator>(16000);
    }

    std::shared_ptr<MockFileSystem> mock_fs_;
    std::unique_ptr<TestSignalGenerator> generator_;
};

TEST_F(MockFileSystemExample, CreateAndReadVirtualFile) {
    // Create virtual file with audio data
    auto audio_data = generator_->GenerateSineWave(440.0, 100);
    std::vector<uint8_t> file_data(
        reinterpret_cast<uint8_t*>(audio_data.data()),
        reinterpret_cast<uint8_t*>(audio_data.data()) + audio_data.size() * sizeof(int16_t)
    );

    mock_fs_->AddVirtualFile("/test/audio.raw", file_data);

    // Verify file exists
    EXPECT_TRUE(mock_fs_->FileExists("/test/audio.raw"));

    // Read it back
    auto read_data = mock_fs_->GetVirtualFileContent("/test/audio.raw");
    EXPECT_EQ(read_data, file_data);
}

TEST_F(MockFileSystemExample, UseFileReaderWriter) {
    // Create mock file writer
    auto writer = std::make_unique<MockFileWriter>();
    writer->CaptureWrittenData();

    // Write audio data
    auto audio_data = generator_->GenerateSineWave(1000.0, 200);
    EXPECT_CALL(*writer, Open(_, _)).WillOnce(Return(true));
    EXPECT_CALL(*writer, IsOpen()).WillRepeatedly(Return(true));

    writer->Open("/test/output.raw", false);
    writer->Write(audio_data.data(), audio_data.size() * sizeof(int16_t));

    // Verify written data
    const auto& written = writer->GetWrittenData();
    EXPECT_EQ(written.size(), audio_data.size() * sizeof(int16_t));
}

// ============================================================================
// Example 6: Integration Test
// ============================================================================

class AudioPipelineIntegrationExample : public AudioTestFixture {
protected:
    void SetUp() override {
        AudioTestFixture::SetUp();
        capture_ = std::make_shared<MockAudioCaptureDevice>();
        playback_ = std::make_shared<MockAudioPlaybackDevice>();
    }

    std::shared_ptr<MockAudioCaptureDevice> capture_;
    std::shared_ptr<MockAudioPlaybackDevice> playback_;
};

TEST_F(AudioPipelineIntegrationExample, CaptureProcessAndPlayback) {
    // Arrange: Generate input signal
    auto input_signal = GenerateSineWave(440.0, 1000, 0.5);

    // Set up capture device
    capture_->SimulateCapturedData(input_signal);

    // Set up playback device
    playback_->CapturePlaybackData();

    // Act: Simulate pipeline
    // 1. Capture audio
    std::vector<int16_t> captured(input_signal.size());
    capture_->Read(captured.data(), captured.size());

    // 2. Process audio (example: apply gain)
    std::vector<int16_t> processed = captured;
    for (auto& sample : processed) {
        sample = static_cast<int16_t>(sample * 0.8);  // Reduce gain
    }

    // 3. Playback
    playback_->Write(processed.data(), processed.size());

    // Assert: Verify pipeline
    const auto& played = playback_->GetPlayedData();
    EXPECT_EQ(played.size(), processed.size());
    EXPECT_EQ(played, processed);

    // Verify processing effect
    double input_rms = CalculateRMS(captured);
    double output_rms = CalculateRMS(played);
    EXPECT_LT(output_rms, input_rms);  // Gain reduction applied
}

// Note: Individual test files should not define main()
// Main is defined in test_main.cpp
