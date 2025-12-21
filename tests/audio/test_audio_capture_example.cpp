/**
 * @file test_audio_capture_example.cpp
 * @brief Example audio capture tests demonstrating test architecture
 *
 * This file demonstrates how to write tests using the test fixtures,
 * mocks, and utilities provided by the test architecture.
 */

#include <gtest/gtest.h>
#include <gmock/gmock.h>

#include "fixtures/audio_test_fixture.h"
#include "mocks/mock_audio_device.h"
#include "utils/test_signal_generator.h"
#include "utils/test_helpers.h"

using namespace ffvoice::test;
using ::testing::Return;
using ::testing::_;
using ::testing::AtLeast;

/**
 * @class AudioCaptureTest
 * @brief Test suite for audio capture functionality
 *
 * Demonstrates testing audio capture with mocked devices and
 * generated test signals.
 */
class AudioCaptureTest : public AudioCaptureTestFixture {
protected:
    void SetUp() override {
        AudioCaptureTestFixture::SetUp();

        // Create mock capture device
        mock_device_ = std::make_shared<MockAudioCaptureDevice>();

        // Create signal generator
        signal_gen_ = std::make_unique<TestSignalGenerator>(config_.sample_rate);

        // Set default mock behavior
        mock_device_->SetDefaultSuccessBehavior();
    }

    void TearDown() override {
        mock_device_.reset();
        signal_gen_.reset();
        AudioCaptureTestFixture::TearDown();
    }

    std::shared_ptr<MockAudioCaptureDevice> mock_device_;
    std::unique_ptr<TestSignalGenerator> signal_gen_;
};

// ============================================================================
// Basic Functionality Tests
// ============================================================================

/**
 * @test Verify device initializes successfully
 */
TEST_F(AudioCaptureTest, InitializesSuccessfully) {
    // Arrange
    EXPECT_CALL(*mock_device_, Initialize())
        .Times(1)
        .WillOnce(Return(true));

    // Act
    bool result = mock_device_->Initialize();

    // Assert
    EXPECT_TRUE(result);
}

/**
 * @test Verify device initialization failure is handled
 */
TEST_F(AudioCaptureTest, HandlesInitializationFailure) {
    // Arrange
    EXPECT_CALL(*mock_device_, Initialize())
        .Times(1)
        .WillOnce(Return(false));

    // Act
    bool result = mock_device_->Initialize();

    // Assert
    EXPECT_FALSE(result);
}

/**
 * @test Verify device starts capturing
 */
TEST_F(AudioCaptureTest, StartsCapturing) {
    // Arrange
    EXPECT_CALL(*mock_device_, Initialize()).WillOnce(Return(true));
    EXPECT_CALL(*mock_device_, Start()).Times(1).WillOnce(Return(true));
    EXPECT_CALL(*mock_device_, IsRunning()).WillRepeatedly(Return(true));

    // Act
    mock_device_->Initialize();
    bool started = mock_device_->Start();

    // Assert
    EXPECT_TRUE(started);
    EXPECT_TRUE(mock_device_->IsRunning());
}

/**
 * @test Verify device stops capturing
 */
TEST_F(AudioCaptureTest, StopsCapturing) {
    // Arrange
    EXPECT_CALL(*mock_device_, Start()).WillOnce(Return(true));
    EXPECT_CALL(*mock_device_, Stop()).Times(1).WillOnce(Return(true));
    EXPECT_CALL(*mock_device_, IsRunning())
        .WillOnce(Return(true))
        .WillRepeatedly(Return(false));

    // Act
    mock_device_->Start();
    bool running_before = mock_device_->IsRunning();
    mock_device_->Stop();
    bool running_after = mock_device_->IsRunning();

    // Assert
    EXPECT_TRUE(running_before);
    EXPECT_FALSE(running_after);
}

// ============================================================================
// Audio Data Capture Tests
// ============================================================================

/**
 * @test Verify capturing sine wave audio data
 */
TEST_F(AudioCaptureTest, CapturesSineWaveData) {
    // Arrange: Generate test signal (440 Hz sine wave, 1 second)
    auto test_signal = signal_gen_->GenerateSineWave(440.0, 1000, 0.5);
    mock_device_->SimulateCapturedData(test_signal);

    EXPECT_CALL(*mock_device_, Read(_, _))
        .Times(AtLeast(1));

    // Act: Capture audio
    std::vector<int16_t> captured(1024);
    size_t frames_read = mock_device_->Read(captured.data(), captured.size());

    // Assert: Verify captured data
    EXPECT_GT(frames_read, 0);
    EXPECT_LE(frames_read, captured.size());

    // Verify signal characteristics
    double rms = CalculateRMS(captured);
    EXPECT_GT(rms, 0.0);  // Signal should not be silence
    EXPECT_LT(rms, 32767.0);  // Signal should not be clipping
}

/**
 * @test Verify capturing white noise
 */
TEST_F(AudioCaptureTest, CapturesWhiteNoise) {
    // Arrange: Generate white noise
    auto noise_signal = signal_gen_->GenerateWhiteNoise(500, 0.3);
    mock_device_->SimulateCapturedData(noise_signal);

    // Act
    std::vector<int16_t> captured(noise_signal.size());
    size_t frames_read = mock_device_->Read(captured.data(), captured.size());

    // Assert
    EXPECT_EQ(frames_read, captured.size());

    // White noise should have relatively high zero crossing rate
    size_t zero_crossings = TestHelpers::CountZeroCrossings(captured);
    EXPECT_GT(zero_crossings, captured.size() / 4);
}

/**
 * @test Verify capturing silence
 */
TEST_F(AudioCaptureTest, CapturesSilence) {
    // Arrange: Generate silence
    auto silence = signal_gen_->GenerateSilence(1000);
    mock_device_->SimulateCapturedData(silence);

    // Act
    std::vector<int16_t> captured(silence.size());
    mock_device_->Read(captured.data(), captured.size());

    // Assert: All samples should be zero
    for (int16_t sample : captured) {
        EXPECT_EQ(sample, 0);
    }

    // RMS should be zero
    EXPECT_DOUBLE_EQ(CalculateRMS(captured), 0.0);
}

// ============================================================================
// Buffer Management Tests
// ============================================================================

/**
 * @test Verify handling of partial buffer reads
 */
TEST_F(AudioCaptureTest, HandlesPartialBufferRead) {
    // Arrange: Small signal
    auto small_signal = signal_gen_->GenerateSineWave(440.0, 100);
    mock_device_->SimulateCapturedData(small_signal);

    // Act: Request more data than available
    std::vector<int16_t> captured(small_signal.size() * 2);
    size_t frames_read = mock_device_->Read(captured.data(), captured.size());

    // Assert: Should only read available data
    EXPECT_LE(frames_read, small_signal.size());
}

/**
 * @test Verify handling of empty buffer
 */
TEST_F(AudioCaptureTest, HandlesEmptyBuffer) {
    // Arrange: Empty signal
    mock_device_->SimulateCapturedData({});

    // Act
    std::vector<int16_t> captured(1024);
    size_t frames_read = mock_device_->Read(captured.data(), captured.size());

    // Assert
    EXPECT_EQ(frames_read, 0);
}

// ============================================================================
// Audio Quality Tests
// ============================================================================

/**
 * @test Verify captured audio maintains signal integrity
 */
TEST_F(AudioCaptureTest, MaintainsSignalIntegrity) {
    // Arrange: Generate known signal
    auto original_signal = signal_gen_->GenerateSineWave(1000.0, 500, 0.7);
    mock_device_->SimulateCapturedData(original_signal);

    // Act: Capture
    std::vector<int16_t> captured(original_signal.size());
    mock_device_->Read(captured.data(), captured.size());

    // Assert: Compare with original
    double correlation = TestHelpers::CalculateCorrelation(original_signal, captured);
    EXPECT_GT(correlation, 0.99);  // Very high correlation expected

    double mse = TestHelpers::CalculateMSE(original_signal, captured);
    EXPECT_LT(mse, 1.0);  // Very low error expected
}

/**
 * @test Verify no signal clipping occurs
 */
TEST_F(AudioCaptureTest, PreventsSaturatio) {
    // Arrange: Generate signal at various amplitudes
    std::vector<double> amplitudes = {0.5, 0.7, 0.9, 1.0};

    for (double amplitude : amplitudes) {
        auto signal = signal_gen_->GenerateSineWave(440.0, 100, amplitude);
        mock_device_->SimulateCapturedData(signal);

        // Act
        std::vector<int16_t> captured(signal.size());
        mock_device_->Read(captured.data(), captured.size());

        // Assert: Check for clipping
        int16_t peak = TestHelpers::CalculatePeak(captured);
        EXPECT_LT(peak, 32767) << "Signal clipping detected at amplitude " << amplitude;
    }
}

// ============================================================================
// Device Information Tests
// ============================================================================

/**
 * @test Verify device information retrieval
 */
TEST_F(AudioCaptureTest, RetrievesDeviceInfo) {
    // Arrange
    AudioDeviceInfo expected_info;
    expected_info.id = "test_device";
    expected_info.name = "Test Capture Device";
    expected_info.max_channels = 2;
    expected_info.default_sample_rate = 48000;
    expected_info.is_default = true;
    expected_info.is_input = true;

    EXPECT_CALL(*mock_device_, GetDeviceInfo())
        .WillOnce(Return(expected_info));

    // Act
    AudioDeviceInfo info = mock_device_->GetDeviceInfo();

    // Assert
    EXPECT_EQ(info.id, "test_device");
    EXPECT_EQ(info.name, "Test Capture Device");
    EXPECT_EQ(info.max_channels, 2);
    EXPECT_EQ(info.default_sample_rate, 48000);
    EXPECT_TRUE(info.is_default);
    EXPECT_TRUE(info.is_input);
}

/**
 * @test Verify stream parameters are correct
 */
TEST_F(AudioCaptureTest, ReturnsCorrectStreamParameters) {
    // Arrange
    AudioStreamParams expected_params;
    expected_params.sample_rate = 16000;
    expected_params.channels = 1;
    expected_params.bits_per_sample = 16;
    expected_params.buffer_frames = 1024;

    EXPECT_CALL(*mock_device_, GetStreamParams())
        .WillOnce(Return(expected_params));

    // Act
    AudioStreamParams params = mock_device_->GetStreamParams();

    // Assert
    EXPECT_EQ(params.sample_rate, 16000);
    EXPECT_EQ(params.channels, 1);
    EXPECT_EQ(params.bits_per_sample, 16);
    EXPECT_EQ(params.buffer_frames, 1024);
}

// ============================================================================
// Performance Tests
// ============================================================================

/**
 * @test Benchmark: Measure capture performance
 */
TEST_F(AudioCaptureTest, BenchmarkCapturePerformance) {
    // Arrange: 10 seconds of audio
    auto large_signal = signal_gen_->GenerateSineWave(440.0, 10000);
    mock_device_->SimulateCapturedData(large_signal);

    // Act & Measure
    std::vector<int16_t> captured(large_signal.size());
    double elapsed_ms = TestHelpers::MeasureExecutionTime([&]() {
        mock_device_->Read(captured.data(), captured.size());
    });

    // Assert: Should complete quickly (< 100ms for mock)
    EXPECT_LT(elapsed_ms, 100.0);

    // Log performance
    std::cout << "Capture benchmark: " << elapsed_ms << " ms for "
              << large_signal.size() << " samples" << std::endl;
}

// ============================================================================
// Main
// ============================================================================

// Note: main() is in test_main.cpp
// Individual test files should not define main()
