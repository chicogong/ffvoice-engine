/**
 * @file audio_capture_device.h
 * @brief Audio capture device using PortAudio
 */

#pragma once

#include <portaudio.h>

#include <functional>
#include <memory>
#include <string>
#include <vector>

#include "ffvoice/types.h"

namespace ffvoice {

/**
 * @brief Audio capture device
 *
 * Captures audio from microphone or system audio using PortAudio
 */
class AudioCaptureDevice {
public:
    /**
     * @brief Audio callback function type
     * @param samples Audio samples (int16_t for 16-bit)
     * @param num_frames Number of frames captured
     */
    using AudioCallback = std::function<void(const int16_t* samples, size_t num_frames)>;

    AudioCaptureDevice();
    ~AudioCaptureDevice();

    // Disable copy
    AudioCaptureDevice(const AudioCaptureDevice&) = delete;
    AudioCaptureDevice& operator=(const AudioCaptureDevice&) = delete;

    /**
     * @brief Initialize PortAudio
     * @return true if successful
     */
    static bool Initialize();

    /**
     * @brief Terminate PortAudio
     */
    static void Terminate();

    /**
     * @brief Get list of available audio devices
     */
    static std::vector<AudioDeviceInfo> GetDevices();

    /**
     * @brief Get default input device ID
     */
    static int GetDefaultInputDevice();

    /**
     * @brief Open audio capture device
     * @param device_id Device ID (-1 for default)
     * @param sample_rate Sample rate in Hz
     * @param channels Number of channels (1 or 2)
     * @param frames_per_buffer Buffer size in frames
     * @return true if successful
     */
    bool Open(int device_id = -1, int sample_rate = 48000, int channels = 1,
              int frames_per_buffer = 256);

    /**
     * @brief Start capturing audio
     * @param callback Function to call when audio data is available
     * @return true if successful
     */
    bool Start(AudioCallback callback);

    /**
     * @brief Stop capturing audio
     */
    void Stop();

    /**
     * @brief Close the device
     */
    void Close();

    /**
     * @brief Check if device is open
     */
    bool IsOpen() const {
        return stream_ != nullptr;
    }

    /**
     * @brief Check if capturing
     */
    bool IsCapturing() const {
        return is_capturing_;
    }

    /**
     * @brief Get current sample rate
     */
    int GetSampleRate() const {
        return sample_rate_;
    }

    /**
     * @brief Get current channel count
     */
    int GetChannels() const {
        return channels_;
    }

private:
    // PortAudio callback
    static int PortAudioCallback(const void* input_buffer, void* output_buffer,
                                 unsigned long frames_per_buffer,
                                 const PaStreamCallbackTimeInfo* time_info,
                                 PaStreamCallbackFlags status_flags, void* user_data);

    PaStream* stream_ = nullptr;
    AudioCallback user_callback_;
    int device_id_ = -1;
    int sample_rate_ = 0;
    int channels_ = 0;
    bool is_capturing_ = false;

    static bool is_initialized_;
};

}  // namespace ffvoice
