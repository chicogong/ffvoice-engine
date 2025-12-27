/**
 * @file test_audio_capture.cpp
 * @brief Unit tests for AudioCaptureDevice class
 * @details RED Phase - These tests are designed to FAIL initially
 *          as the production code hasn't been implemented yet
 */

#include "audio/audio_capture_device.h"
#include "utils/test_signal_generator.h"

#include <gtest/gtest.h>

#include "fixtures/audio_test_fixture.h"
#include "mocks/mock_audio_device.h"

namespace ffvoice {
namespace test {

/**
 * Test Suite: AudioCaptureDeviceTest
 * Tests audio capture functionality
 */
class AudioCaptureDeviceTest : public AudioTestFixture {
protected:
    std::unique_ptr<AudioCaptureDevice> capture_device_;
    std::unique_ptr<MockAudioManager> mock_manager_;

    void SetUp() override {
        AudioTestFixture::SetUp();
        mock_manager_ = std::make_unique<MockAudioManager>();
        capture_device_ = std::make_unique<AudioCaptureDevice>();
    }
};

// ============================================================================
// Device Enumeration Tests
// ============================================================================

TEST_F(AudioCaptureDeviceTest, ListAvailableDevices) {
    // UT-AC-001: List available devices
    auto devices = capture_device_->listDevices();

    EXPECT_FALSE(devices.empty()) << "Should find at least one audio device";

    for (const auto& device : devices) {
        EXPECT_FALSE(device.name.empty()) << "Device name should not be empty";
        EXPECT_GE(device.id, 0) << "Device ID should be non-negative";
        EXPECT_GT(device.max_input_channels, 0) << "Should have input channels";
        EXPECT_GT(device.sample_rates.size(), 0) << "Should support sample rates";
    }
}

TEST_F(AudioCaptureDeviceTest, HandleNoDevicesAvailable) {
    // UT-AC-002: Handle no devices available
    mock_manager_->setAvailableDeviceCount(0);
    capture_device_->setAudioManager(mock_manager_.get());

    auto devices = capture_device_->listDevices();
    EXPECT_TRUE(devices.empty()) << "Should return empty list when no devices";
}

TEST_F(AudioCaptureDeviceTest, ValidateDeviceInfoStructure) {
    // UT-AC-003: Validate device info structure
    auto devices = capture_device_->listDevices();
    ASSERT_FALSE(devices.empty());

    const auto& device = devices[0];

    // Check required fields
    EXPECT_FALSE(device.name.empty());
    EXPECT_GE(device.id, 0);
    EXPECT_GT(device.max_input_channels, 0);
    EXPECT_LE(device.max_input_channels, 32);  // Reasonable upper limit

    // Check supported sample rates
    EXPECT_TRUE(device.supportsSampleRate(44100) || device.supportsSampleRate(48000))
        << "Should support standard sample rates";
}

TEST_F(AudioCaptureDeviceTest, HandleDeviceEnumerationFailure) {
    // UT-AC-004: Handle device enumeration failure
    mock_manager_->setEnumerationError(true);
    capture_device_->setAudioManager(mock_manager_.get());

    EXPECT_THROW(
        { capture_device_->listDevices(); }, AudioException)
        << "Should throw on enumeration failure";
}

// ============================================================================
// Stream Initialization Tests
// ============================================================================

TEST_F(AudioCaptureDeviceTest, InitializeWithValidParameters) {
    // UT-AC-005: Initialize with valid parameters
    AudioStreamConfig config;
    config.device_id = 0;
    config.sample_rate = 48000;
    config.channels = 2;
    config.buffer_frames = 256;
    config.format = AudioFormat::FLOAT32;

    EXPECT_TRUE(capture_device_->initialize(config)) << "Should initialize with valid parameters";

    EXPECT_EQ(capture_device_->getSampleRate(), 48000);
    EXPECT_EQ(capture_device_->getChannelCount(), 2);
    EXPECT_EQ(capture_device_->getBufferSize(), 256);
}

TEST_F(AudioCaptureDeviceTest, RejectUnsupportedSampleRate) {
    // UT-AC-006: Reject unsupported sample rate
    AudioStreamConfig config;
    config.device_id = 0;
    config.sample_rate = 192000;  // Likely unsupported
    config.channels = 2;

    EXPECT_FALSE(capture_device_->initialize(config)) << "Should reject unsupported sample rate";

    auto error = capture_device_->getLastError();
    EXPECT_TRUE(error.find("sample rate") != std::string::npos)
        << "Error message should mention sample rate";
}

TEST_F(AudioCaptureDeviceTest, RejectInvalidChannelCount) {
    // UT-AC-007: Reject invalid channel count
    AudioStreamConfig config;
    config.device_id = 0;
    config.sample_rate = 48000;
    config.channels = 0;  // Invalid

    EXPECT_FALSE(capture_device_->initialize(config)) << "Should reject zero channels";

    config.channels = 100;  // Too many
    EXPECT_FALSE(capture_device_->initialize(config)) << "Should reject excessive channel count";
}

TEST_F(AudioCaptureDeviceTest, HandleDeviceOpenFailure) {
    // UT-AC-008: Handle device open failure
    mock_manager_->setDeviceBusy(0, true);
    capture_device_->setAudioManager(mock_manager_.get());

    AudioStreamConfig config;
    config.device_id = 0;
    config.sample_rate = 48000;
    config.channels = 2;

    EXPECT_FALSE(capture_device_->initialize(config)) << "Should fail when device is busy";

    auto error = capture_device_->getLastError();
    EXPECT_TRUE(error.find("busy") != std::string::npos ||
                error.find("in use") != std::string::npos)
        << "Error message should indicate device is busy";
}

TEST_F(AudioCaptureDeviceTest, ValidateBufferSizeConfiguration) {
    // UT-AC-009: Validate buffer size configuration
    AudioStreamConfig config;
    config.device_id = 0;
    config.sample_rate = 48000;
    config.channels = 2;

    // Test various buffer sizes
    std::vector<int> buffer_sizes = {64, 128, 256, 512, 1024, 2048};

    for (int size : buffer_sizes) {
        config.buffer_frames = size;
        EXPECT_TRUE(capture_device_->initialize(config)) << "Should support buffer size " << size;

        EXPECT_EQ(capture_device_->getBufferSize(), size) << "Buffer size should match requested";

        capture_device_->deinitialize();
    }

    // Test invalid buffer sizes
    config.buffer_frames = 0;
    EXPECT_FALSE(capture_device_->initialize(config)) << "Should reject zero buffer size";

    config.buffer_frames = -1;
    EXPECT_FALSE(capture_device_->initialize(config)) << "Should reject negative buffer size";
}

// ============================================================================
// Audio Streaming Tests
// ============================================================================

TEST_F(AudioCaptureDeviceTest, StartStopStreamSuccessfully) {
    // UT-AC-010: Start/stop stream successfully
    AudioStreamConfig config;
    config.device_id = 0;
    config.sample_rate = 48000;
    config.channels = 2;
    config.buffer_frames = 256;

    ASSERT_TRUE(capture_device_->initialize(config));

    EXPECT_FALSE(capture_device_->isStreaming()) << "Should not be streaming initially";

    EXPECT_TRUE(capture_device_->startStream()) << "Should start stream successfully";

    EXPECT_TRUE(capture_device_->isStreaming()) << "Should be streaming after start";

    // Let it run briefly
    std::this_thread::sleep_for(std::chrono::milliseconds(100));

    EXPECT_TRUE(capture_device_->stopStream()) << "Should stop stream successfully";

    EXPECT_FALSE(capture_device_->isStreaming()) << "Should not be streaming after stop";
}

TEST_F(AudioCaptureDeviceTest, ReceiveAudioDataInCallback) {
    // UT-AC-011: Receive audio data in callback
    AudioStreamConfig config;
    config.device_id = 0;
    config.sample_rate = 48000;
    config.channels = 2;
    config.buffer_frames = 256;

    ASSERT_TRUE(capture_device_->initialize(config));

    std::atomic<int> callback_count{0};
    std::atomic<size_t> total_frames{0};

    capture_device_->setAudioCallback([&](const float* input, size_t frames) {
        callback_count++;
        total_frames += frames;

        // Verify buffer content
        EXPECT_NE(input, nullptr) << "Input buffer should not be null";
        EXPECT_EQ(frames, 256) << "Frames should match buffer size";

        // Check for non-zero audio (not complete silence)
        bool has_signal = false;
        for (size_t i = 0; i < frames * 2; ++i) {
            if (std::abs(input[i]) > 0.0001f) {
                has_signal = true;
                break;
            }
        }
        EXPECT_TRUE(has_signal) << "Should receive non-silent audio";

        return 0;  // Success
    });

    EXPECT_TRUE(capture_device_->startStream());

    // Record for 500ms
    std::this_thread::sleep_for(std::chrono::milliseconds(500));

    EXPECT_TRUE(capture_device_->stopStream());

    // Verify callbacks were received
    EXPECT_GT(callback_count.load(), 0) << "Should have received callbacks";

    // At 48kHz, 500ms = 24000 frames
    EXPECT_NEAR(total_frames.load(), 24000, 2400)  // 10% tolerance
        << "Should receive expected number of frames";
}

TEST_F(AudioCaptureDeviceTest, HandleCallbackErrorsGracefully) {
    // UT-AC-012: Handle callback errors gracefully
    AudioStreamConfig config;
    config.device_id = 0;
    config.sample_rate = 48000;
    config.channels = 2;
    config.buffer_frames = 256;

    ASSERT_TRUE(capture_device_->initialize(config));

    int error_count = 0;

    // Callback that returns error after 3 calls
    capture_device_->setAudioCallback([&](const float* input, size_t frames) {
        static int call_count = 0;
        call_count++;

        if (call_count > 3) {
            error_count++;
            return 1;  // Return error
        }
        return 0;
    });

    capture_device_->setErrorCallback([&](const std::string& error) {
        EXPECT_FALSE(error.empty()) << "Error message should not be empty";
        error_count++;
    });

    EXPECT_TRUE(capture_device_->startStream());
    std::this_thread::sleep_for(std::chrono::milliseconds(200));
    EXPECT_TRUE(capture_device_->stopStream());

    EXPECT_GT(error_count, 0) << "Should have handled errors";
}

TEST_F(AudioCaptureDeviceTest, VerifyBufferSizeConsistency) {
    // UT-AC-013: Verify buffer size consistency
    AudioStreamConfig config;
    config.device_id = 0;
    config.sample_rate = 48000;
    config.channels = 2;
    config.buffer_frames = 512;

    ASSERT_TRUE(capture_device_->initialize(config));

    std::vector<size_t> frame_counts;

    capture_device_->setAudioCallback([&](const float* input, size_t frames) {
        frame_counts.push_back(frames);
        return 0;
    });

    EXPECT_TRUE(capture_device_->startStream());
    std::this_thread::sleep_for(std::chrono::milliseconds(200));
    EXPECT_TRUE(capture_device_->stopStream());

    // All callbacks should have same frame count
    for (size_t count : frame_counts) {
        EXPECT_EQ(count, 512) << "Buffer size should be consistent";
    }
}

TEST_F(AudioCaptureDeviceTest, TestStreamRestartAfterStop) {
    // UT-AC-014: Test stream restart after stop
    AudioStreamConfig config;
    config.device_id = 0;
    config.sample_rate = 48000;
    config.channels = 2;
    config.buffer_frames = 256;

    ASSERT_TRUE(capture_device_->initialize(config));

    // First start/stop cycle
    EXPECT_TRUE(capture_device_->startStream());
    std::this_thread::sleep_for(std::chrono::milliseconds(100));
    EXPECT_TRUE(capture_device_->stopStream());

    // Second start/stop cycle
    EXPECT_TRUE(capture_device_->startStream()) << "Should be able to restart after stop";
    std::this_thread::sleep_for(std::chrono::milliseconds(100));
    EXPECT_TRUE(capture_device_->stopStream());
}

// ============================================================================
// Error Handling Tests
// ============================================================================

TEST_F(AudioCaptureDeviceTest, HandleStreamOverflow) {
    // UT-AC-015: Handle stream overflow
    AudioStreamConfig config;
    config.device_id = 0;
    config.sample_rate = 48000;
    config.channels = 2;
    config.buffer_frames = 256;

    ASSERT_TRUE(capture_device_->initialize(config));

    std::atomic<int> overflow_count{0};

    // Slow callback to trigger overflow
    capture_device_->setAudioCallback([&](const float* input, size_t frames) {
        std::this_thread::sleep_for(std::chrono::milliseconds(50));  // Slow processing
        return 0;
    });

    capture_device_->setErrorCallback([&](const std::string& error) {
        if (error.find("overflow") != std::string::npos ||
            error.find("overrun") != std::string::npos) {
            overflow_count++;
        }
    });

    EXPECT_TRUE(capture_device_->startStream());
    std::this_thread::sleep_for(std::chrono::milliseconds(500));
    EXPECT_TRUE(capture_device_->stopStream());

    EXPECT_GT(overflow_count.load(), 0) << "Should detect and report overflow conditions";
}

TEST_F(AudioCaptureDeviceTest, HandleDeviceDisconnection) {
    // UT-AC-016: Handle device disconnection
    mock_manager_->addMockDevice(
        MockAudioDevice{.device_id = 99, .name = "USB Microphone", .removable = true});

    capture_device_->setAudioManager(mock_manager_.get());

    AudioStreamConfig config;
    config.device_id = 99;
    config.sample_rate = 48000;
    config.channels = 2;

    ASSERT_TRUE(capture_device_->initialize(config));
    ASSERT_TRUE(capture_device_->startStream());

    std::atomic<bool> disconnection_detected{false};

    capture_device_->setErrorCallback([&](const std::string& error) {
        if (error.find("disconnect") != std::string::npos ||
            error.find("removed") != std::string::npos) {
            disconnection_detected = true;
        }
    });

    // Simulate device disconnection
    mock_manager_->simulateDeviceDisconnection(99);

    std::this_thread::sleep_for(std::chrono::milliseconds(100));

    EXPECT_TRUE(disconnection_detected.load()) << "Should detect device disconnection";

    EXPECT_FALSE(capture_device_->isStreaming()) << "Stream should stop on disconnection";
}

TEST_F(AudioCaptureDeviceTest, CleanupOnErrorConditions) {
    // UT-AC-017: Cleanup on error conditions
    AudioStreamConfig config;
    config.device_id = 0;
    config.sample_rate = 48000;
    config.channels = 2;
    config.buffer_frames = 256;

    ASSERT_TRUE(capture_device_->initialize(config));

    // Force an error during streaming
    capture_device_->setAudioCallback([](const float* input, size_t frames) {
        throw std::runtime_error("Simulated callback error");
    });

    std::atomic<bool> error_handled{false};

    capture_device_->setErrorCallback([&](const std::string& error) { error_handled = true; });

    EXPECT_TRUE(capture_device_->startStream());
    std::this_thread::sleep_for(std::chrono::milliseconds(100));

    EXPECT_TRUE(error_handled.load()) << "Should handle callback exception";

    // Should still be able to clean up properly
    EXPECT_NO_THROW({
        capture_device_->stopStream();
        capture_device_->deinitialize();
    }) << "Should cleanup without throwing";
}

TEST_F(AudioCaptureDeviceTest, ThreadSafetyOfCallbacks) {
    // UT-AC-018: Thread safety of callbacks
    AudioStreamConfig config;
    config.device_id = 0;
    config.sample_rate = 48000;
    config.channels = 2;
    config.buffer_frames = 256;

    ASSERT_TRUE(capture_device_->initialize(config));

    std::atomic<int> callback_count{0};
    std::mutex data_mutex;
    std::vector<float> accumulated_data;

    capture_device_->setAudioCallback([&](const float* input, size_t frames) {
        callback_count++;

        // Thread-safe accumulation
        std::lock_guard<std::mutex> lock(data_mutex);
        accumulated_data.insert(accumulated_data.end(), input, input + frames * 2);
        return 0;
    });

    EXPECT_TRUE(capture_device_->startStream());

    // Start multiple threads that interact with the device
    std::vector<std::thread> threads;

    // Thread 1: Query status
    threads.emplace_back([&]() {
        for (int i = 0; i < 100; ++i) {
            capture_device_->isStreaming();
            capture_device_->getSampleRate();
            std::this_thread::sleep_for(std::chrono::milliseconds(1));
        }
    });

    // Thread 2: Get statistics
    threads.emplace_back([&]() {
        for (int i = 0; i < 100; ++i) {
            capture_device_->getStatistics();
            std::this_thread::sleep_for(std::chrono::milliseconds(1));
        }
    });

    std::this_thread::sleep_for(std::chrono::milliseconds(200));

    EXPECT_TRUE(capture_device_->stopStream());

    for (auto& thread : threads) {
        thread.join();
    }

    EXPECT_GT(callback_count.load(), 0) << "Should have received callbacks";
    EXPECT_GT(accumulated_data.size(), 0) << "Should have accumulated data";
}

}  // namespace test
}  // namespace ffvoice