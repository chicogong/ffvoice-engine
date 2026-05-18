/**
 * @file audio_capture_device.cpp
 * @brief Audio capture implementation using PortAudio
 */

#include "audio/audio_capture_device.h"

#include "utils/logger.h"

#include <iostream>

namespace ffvoice {

bool AudioCaptureDevice::is_initialized_ = false;

AudioCaptureDevice::AudioCaptureDevice() {
    if (!is_initialized_) {
        Initialize();
    }
}

AudioCaptureDevice::~AudioCaptureDevice() {
    Close();
}

bool AudioCaptureDevice::Initialize() {
    if (is_initialized_) {
        return true;
    }

    PaError err = Pa_Initialize();
    if (err != paNoError) {
        LOG_ERROR("PortAudio initialization failed: %s", Pa_GetErrorText(err));
        return false;
    }

    is_initialized_ = true;
    LOG_INFO("PortAudio initialized successfully");
    return true;
}

void AudioCaptureDevice::Terminate() {
    if (is_initialized_) {
        Pa_Terminate();
        is_initialized_ = false;
        LOG_INFO("PortAudio terminated");
    }
}

std::vector<AudioDeviceInfo> AudioCaptureDevice::GetDevices() {
    std::vector<AudioDeviceInfo> devices;

    if (!is_initialized_ && !Initialize()) {
        return devices;
    }

    int num_devices = Pa_GetDeviceCount();
    if (num_devices < 0) {
        LOG_ERROR("Pa_GetDeviceCount failed");
        return devices;
    }

    int default_input = Pa_GetDefaultInputDevice();

    for (int i = 0; i < num_devices; ++i) {
        const PaDeviceInfo* device_info = Pa_GetDeviceInfo(i);
        if (!device_info || device_info->maxInputChannels <= 0) {
            continue;  // Skip output-only devices
        }

        AudioDeviceInfo info;
        info.id = i;
        info.name = device_info->name;
        info.max_input_channels = device_info->maxInputChannels;
        info.max_output_channels = device_info->maxOutputChannels;
        info.is_default = (i == default_input);

        // Add common sample rates
        info.supported_sample_rates = {8000, 16000, 22050, 44100, 48000, 96000};

        devices.push_back(info);
    }

    return devices;
}

int AudioCaptureDevice::GetDefaultInputDevice() {
    if (!is_initialized_ && !Initialize()) {
        return -1;
    }

    return Pa_GetDefaultInputDevice();
}

bool AudioCaptureDevice::Open(int device_id, int sample_rate, int channels, int frames_per_buffer) {
    if (stream_) {
        LOG_ERROR("Device already open");
        return false;
    }

    sample_rate_ = sample_rate;
    channels_ = channels;

    // Use default device if -1
    if (device_id < 0) {
        device_id = Pa_GetDefaultInputDevice();
    }

    if (device_id == paNoDevice) {
        LOG_ERROR("No default input device found");
        return false;
    }

    // Store the device ID for later use
    device_id_ = device_id;

    // Configure input parameters
    PaStreamParameters input_params;
    input_params.device = device_id;
    input_params.channelCount = channels;
    input_params.sampleFormat = paInt16;  // 16-bit PCM
    input_params.suggestedLatency = Pa_GetDeviceInfo(device_id)->defaultLowInputLatency;
    input_params.hostApiSpecificStreamInfo = nullptr;

    // Open stream
    PaError err = Pa_OpenStream(&stream_, &input_params,
                                nullptr,  // No output
                                sample_rate, frames_per_buffer,
                                paClipOff,  // Don't clip samples
                                nullptr,    // No callback yet (will set on Start)
                                nullptr);

    if (err != paNoError) {
        LOG_ERROR("Failed to open stream: %s", Pa_GetErrorText(err));
        stream_ = nullptr;
        return false;
    }

    LOG_INFO("Audio device opened: %s", Pa_GetDeviceInfo(device_id)->name);
    return true;
}

int AudioCaptureDevice::PortAudioCallback(const void* input_buffer, void* output_buffer,
                                          unsigned long frames_per_buffer,
                                          const PaStreamCallbackTimeInfo* time_info,
                                          PaStreamCallbackFlags status_flags, void* user_data) {
    (void)output_buffer;  // Unused
    (void)time_info;      // Unused

    auto* self = static_cast<AudioCaptureDevice*>(user_data);

    if (!self || !self->user_callback_) {
        return paContinue;
    }

    // Thread-safe check: only execute callback if actively capturing
    if (!self->callback_active_.load()) {
        return paContinue;
    }

    // Check for input overflow
    if (status_flags & paInputOverflow) {
        LOG_ERROR("Input overflow detected");
    }

    // Call user callback with audio data
    const int16_t* samples = static_cast<const int16_t*>(input_buffer);
    size_t num_samples = frames_per_buffer * self->channels_;

    self->user_callback_(samples, num_samples);

    return paContinue;
}

bool AudioCaptureDevice::Start(AudioCallback callback) {
    if (!stream_) {
        LOG_ERROR("Device not open");
        return false;
    }

    if (is_capturing_) {
        LOG_ERROR("Already capturing");
        return false;
    }

    user_callback_ = callback;

    // Close the callback-less stream opened by Open(). The handle is invalid
    // after this point regardless of the return code, so clear it immediately
    // to avoid a dangling pointer / double-close if reopening fails below.
    PaError err = Pa_CloseStream(stream_);
    stream_ = nullptr;
    if (err != paNoError) {
        LOG_ERROR("Failed to close stream before reopen: %s", Pa_GetErrorText(err));
        return false;
    }

    // Use the stored device ID
    PaStreamParameters input_params;
    input_params.device = device_id_;
    input_params.channelCount = channels_;
    input_params.sampleFormat = paInt16;
    input_params.suggestedLatency = Pa_GetDeviceInfo(device_id_)->defaultLowInputLatency;
    input_params.hostApiSpecificStreamInfo = nullptr;

    err = Pa_OpenStream(&stream_, &input_params, nullptr, sample_rate_,
                        256,  // frames per buffer
                        paClipOff, PortAudioCallback,
                        this  // User data
    );

    if (err != paNoError) {
        LOG_ERROR("Failed to reopen stream with callback: %s", Pa_GetErrorText(err));
        stream_ = nullptr;  // No valid stream; keep device in a clean closed state
        return false;
    }

    err = Pa_StartStream(stream_);
    if (err != paNoError) {
        LOG_ERROR("Failed to start stream: %s", Pa_GetErrorText(err));
        // Stream was opened but could not start; close it so we do not leak
        // the handle and leave the device half-open.
        Pa_CloseStream(stream_);
        stream_ = nullptr;
        return false;
    }

    callback_active_.store(true);  // Enable callback execution
    is_capturing_ = true;
    LOG_INFO("Audio capture started");
    return true;
}

void AudioCaptureDevice::Stop() {
    if (!stream_ || !is_capturing_) {
        return;
    }

    callback_active_.store(false);  // Disable callback execution

    PaError err = Pa_StopStream(stream_);
    if (err != paNoError) {
        LOG_ERROR("Failed to stop stream: %s", Pa_GetErrorText(err));
    }

    is_capturing_ = false;
    LOG_INFO("Audio capture stopped");
}

void AudioCaptureDevice::Close() {
    if (is_capturing_) {
        Stop();
    }

    if (stream_) {
        Pa_CloseStream(stream_);
        stream_ = nullptr;
        LOG_INFO("Audio device closed");
    }
}

}  // namespace ffvoice
