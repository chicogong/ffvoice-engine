/**
 * @file mock_audio_device.h
 * @brief Mock classes for audio device interfaces
 *
 * Provides Google Mock implementations of audio device interfaces
 * for isolated unit testing without real hardware dependencies.
 */

#ifndef FFVOICE_TESTS_MOCKS_MOCK_AUDIO_DEVICE_H
#define FFVOICE_TESTS_MOCKS_MOCK_AUDIO_DEVICE_H

#include <gmock/gmock.h>

#include <cstdint>
#include <functional>
#include <memory>
#include <string>
#include <vector>

namespace ffvoice {
namespace test {

/**
 * @brief Audio device information structure
 */
struct AudioDeviceInfo {
    std::string id;
    std::string name;
    uint32_t max_channels;
    uint32_t default_sample_rate;
    bool is_default;
    bool is_input;
};

/**
 * @brief Audio stream parameters
 */
struct AudioStreamParams {
    uint32_t sample_rate;
    uint16_t channels;
    uint16_t bits_per_sample;
    size_t buffer_frames;
};

/**
 * @brief Audio callback function type
 */
using AudioCallback = std::function<void(const void* input, void* output, size_t frames)>;

/**
 * @class IAudioDevice
 * @brief Interface for audio device operations
 *
 * This interface defines the contract for audio device operations
 * that will be mocked in tests.
 */
class IAudioDevice {
public:
    virtual ~IAudioDevice() = default;

    /**
     * @brief Initialize audio device
     *
     * @return True if initialization successful
     */
    virtual bool Initialize() = 0;

    /**
     * @brief Shutdown audio device
     */
    virtual void Shutdown() = 0;

    /**
     * @brief Start audio stream
     *
     * @return True if stream started successfully
     */
    virtual bool Start() = 0;

    /**
     * @brief Stop audio stream
     *
     * @return True if stream stopped successfully
     */
    virtual bool Stop() = 0;

    /**
     * @brief Check if device is running
     *
     * @return True if device is currently running
     */
    virtual bool IsRunning() const = 0;

    /**
     * @brief Get device information
     *
     * @return Device information structure
     */
    virtual AudioDeviceInfo GetDeviceInfo() const = 0;

    /**
     * @brief Set audio callback
     *
     * @param callback Callback function to handle audio data
     */
    virtual void SetCallback(AudioCallback callback) = 0;

    /**
     * @brief Get current stream parameters
     *
     * @return Stream parameters
     */
    virtual AudioStreamParams GetStreamParams() const = 0;

    /**
     * @brief Get current latency in milliseconds
     *
     * @return Latency in milliseconds
     */
    virtual double GetLatency() const = 0;
};

/**
 * @class MockAudioDevice
 * @brief Mock implementation of IAudioDevice interface
 *
 * Uses Google Mock to create a controllable audio device for testing.
 */
class MockAudioDevice : public IAudioDevice {
public:
    MOCK_METHOD(bool, Initialize, (), (override));
    MOCK_METHOD(void, Shutdown, (), (override));
    MOCK_METHOD(bool, Start, (), (override));
    MOCK_METHOD(bool, Stop, (), (override));
    MOCK_METHOD(bool, IsRunning, (), (const, override));
    MOCK_METHOD(AudioDeviceInfo, GetDeviceInfo, (), (const, override));
    MOCK_METHOD(void, SetCallback, (AudioCallback callback), (override));
    MOCK_METHOD(AudioStreamParams, GetStreamParams, (), (const, override));
    MOCK_METHOD(double, GetLatency, (), (const, override));

    /**
     * @brief Helper method to set default behavior for successful initialization
     */
    void SetDefaultSuccessBehavior() {
        using ::testing::_;
        using ::testing::Return;

        ON_CALL(*this, Initialize()).WillByDefault(Return(true));
        ON_CALL(*this, Start()).WillByDefault(Return(true));
        ON_CALL(*this, Stop()).WillByDefault(Return(true));
        ON_CALL(*this, IsRunning()).WillByDefault(Return(false));
        ON_CALL(*this, GetLatency()).WillByDefault(Return(10.0));
    }

    /**
     * @brief Helper method to simulate device initialization failure
     */
    void SetInitializationFailure() {
        using ::testing::Return;
        ON_CALL(*this, Initialize()).WillByDefault(Return(false));
    }
};

/**
 * @class IAudioCaptureDevice
 * @brief Interface for audio capture (input) device
 */
class IAudioCaptureDevice : public IAudioDevice {
public:
    /**
     * @brief Read audio samples from capture device
     *
     * @param buffer Buffer to store captured audio
     * @param frames Number of frames to read
     * @return Number of frames actually read
     */
    virtual size_t Read(void* buffer, size_t frames) = 0;

    /**
     * @brief Get available frames to read
     *
     * @return Number of available frames
     */
    virtual size_t GetAvailableFrames() const = 0;

    /**
     * @brief Get input level (VU meter)
     *
     * @return Input level (0.0 to 1.0)
     */
    virtual double GetInputLevel() const = 0;
};

/**
 * @class MockAudioCaptureDevice
 * @brief Mock implementation of audio capture device
 */
class MockAudioCaptureDevice : public IAudioCaptureDevice {
public:
    MOCK_METHOD(bool, Initialize, (), (override));
    MOCK_METHOD(void, Shutdown, (), (override));
    MOCK_METHOD(bool, Start, (), (override));
    MOCK_METHOD(bool, Stop, (), (override));
    MOCK_METHOD(bool, IsRunning, (), (const, override));
    MOCK_METHOD(AudioDeviceInfo, GetDeviceInfo, (), (const, override));
    MOCK_METHOD(void, SetCallback, (AudioCallback callback), (override));
    MOCK_METHOD(AudioStreamParams, GetStreamParams, (), (const, override));
    MOCK_METHOD(double, GetLatency, (), (const, override));
    MOCK_METHOD(size_t, Read, (void* buffer, size_t frames), (override));
    MOCK_METHOD(size_t, GetAvailableFrames, (), (const, override));
    MOCK_METHOD(double, GetInputLevel, (), (const, override));

    /**
     * @brief Simulate captured audio data
     *
     * @param data Pre-generated audio data to return on Read()
     */
    void SimulateCapturedData(const std::vector<int16_t>& data) {
        captured_data_ = data;
        data_index_ = 0;

        using ::testing::_;
        using ::testing::Invoke;

        ON_CALL(*this, Read(_, _))
            .WillByDefault(Invoke([this](void* buffer, size_t frames) -> size_t {
                size_t frames_to_copy =
                    std::min(frames, (captured_data_.size() - data_index_) / sizeof(int16_t));

                if (frames_to_copy > 0) {
                    std::memcpy(buffer, &captured_data_[data_index_],
                                frames_to_copy * sizeof(int16_t));
                    data_index_ += frames_to_copy;
                }

                return frames_to_copy;
            }));
    }

private:
    std::vector<int16_t> captured_data_;
    size_t data_index_ = 0;
};

/**
 * @class IAudioPlaybackDevice
 * @brief Interface for audio playback (output) device
 */
class IAudioPlaybackDevice : public IAudioDevice {
public:
    /**
     * @brief Write audio samples to playback device
     *
     * @param buffer Buffer containing audio to play
     * @param frames Number of frames to write
     * @return Number of frames actually written
     */
    virtual size_t Write(const void* buffer, size_t frames) = 0;

    /**
     * @brief Get available space in playback buffer
     *
     * @return Number of frames that can be written
     */
    virtual size_t GetAvailableSpace() const = 0;

    /**
     * @brief Get output level (VU meter)
     *
     * @return Output level (0.0 to 1.0)
     */
    virtual double GetOutputLevel() const = 0;

    /**
     * @brief Flush playback buffer
     */
    virtual void Flush() = 0;
};

/**
 * @class MockAudioPlaybackDevice
 * @brief Mock implementation of audio playback device
 */
class MockAudioPlaybackDevice : public IAudioPlaybackDevice {
public:
    MOCK_METHOD(bool, Initialize, (), (override));
    MOCK_METHOD(void, Shutdown, (), (override));
    MOCK_METHOD(bool, Start, (), (override));
    MOCK_METHOD(bool, Stop, (), (override));
    MOCK_METHOD(bool, IsRunning, (), (const, override));
    MOCK_METHOD(AudioDeviceInfo, GetDeviceInfo, (), (const, override));
    MOCK_METHOD(void, SetCallback, (AudioCallback callback), (override));
    MOCK_METHOD(AudioStreamParams, GetStreamParams, (), (const, override));
    MOCK_METHOD(double, GetLatency, (), (const, override));
    MOCK_METHOD(size_t, Write, (const void* buffer, size_t frames), (override));
    MOCK_METHOD(size_t, GetAvailableSpace, (), (const, override));
    MOCK_METHOD(double, GetOutputLevel, (), (const, override));
    MOCK_METHOD(void, Flush, (), (override));

    /**
     * @brief Capture played audio data for verification
     *
     * @return Vector of captured playback data
     */
    const std::vector<int16_t>& GetPlayedData() const {
        return played_data_;
    }

    /**
     * @brief Set up mock to capture played audio
     */
    void CapturePlaybackData() {
        using ::testing::_;
        using ::testing::Invoke;

        ON_CALL(*this, Write(_, _))
            .WillByDefault(Invoke([this](const void* buffer, size_t frames) -> size_t {
                const int16_t* samples = static_cast<const int16_t*>(buffer);
                played_data_.insert(played_data_.end(), samples, samples + frames);
                return frames;
            }));
    }

private:
    std::vector<int16_t> played_data_;
};

/**
 * @class IAudioDeviceManager
 * @brief Interface for audio device enumeration and management
 */
class IAudioDeviceManager {
public:
    virtual ~IAudioDeviceManager() = default;

    /**
     * @brief Enumerate available audio devices
     *
     * @param input_devices List of input devices
     * @param output_devices List of output devices
     * @return True if enumeration successful
     */
    virtual bool EnumerateDevices(std::vector<AudioDeviceInfo>& input_devices,
                                  std::vector<AudioDeviceInfo>& output_devices) = 0;

    /**
     * @brief Get default input device
     *
     * @return Default input device info
     */
    virtual AudioDeviceInfo GetDefaultInputDevice() const = 0;

    /**
     * @brief Get default output device
     *
     * @return Default output device info
     */
    virtual AudioDeviceInfo GetDefaultOutputDevice() const = 0;

    /**
     * @brief Create capture device by ID
     *
     * @param device_id Device ID
     * @return Shared pointer to capture device
     */
    virtual std::shared_ptr<IAudioCaptureDevice>
    CreateCaptureDevice(const std::string& device_id) = 0;

    /**
     * @brief Create playback device by ID
     *
     * @param device_id Device ID
     * @return Shared pointer to playback device
     */
    virtual std::shared_ptr<IAudioPlaybackDevice>
    CreatePlaybackDevice(const std::string& device_id) = 0;
};

/**
 * @class MockAudioDeviceManager
 * @brief Mock implementation of audio device manager
 */
class MockAudioDeviceManager : public IAudioDeviceManager {
public:
    MOCK_METHOD(bool, EnumerateDevices,
                (std::vector<AudioDeviceInfo> & input_devices,
                 std::vector<AudioDeviceInfo>& output_devices),
                (override));
    MOCK_METHOD(AudioDeviceInfo, GetDefaultInputDevice, (), (const, override));
    MOCK_METHOD(AudioDeviceInfo, GetDefaultOutputDevice, (), (const, override));
    MOCK_METHOD(std::shared_ptr<IAudioCaptureDevice>, CreateCaptureDevice,
                (const std::string& device_id), (override));
    MOCK_METHOD(std::shared_ptr<IAudioPlaybackDevice>, CreatePlaybackDevice,
                (const std::string& device_id), (override));

    /**
     * @brief Set up default mock devices for testing
     */
    void SetupDefaultDevices() {
        using ::testing::_;
        using ::testing::Return;

        AudioDeviceInfo default_input;
        default_input.id = "default_input";
        default_input.name = "Default Input Device";
        default_input.max_channels = 2;
        default_input.default_sample_rate = 48000;
        default_input.is_default = true;
        default_input.is_input = true;

        AudioDeviceInfo default_output;
        default_output.id = "default_output";
        default_output.name = "Default Output Device";
        default_output.max_channels = 2;
        default_output.default_sample_rate = 48000;
        default_output.is_default = true;
        default_output.is_input = false;

        ON_CALL(*this, GetDefaultInputDevice()).WillByDefault(Return(default_input));
        ON_CALL(*this, GetDefaultOutputDevice()).WillByDefault(Return(default_output));
    }
};

}  // namespace test
}  // namespace ffvoice

#endif  // FFVOICE_TESTS_MOCKS_MOCK_AUDIO_DEVICE_H
