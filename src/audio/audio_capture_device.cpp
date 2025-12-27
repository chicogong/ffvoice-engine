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
        log_error("PortAudio initialization failed: " + std::string(Pa_GetErrorText(err)));
        return false;
    }

    is_initialized_ = true;
    log_info("PortAudio initialized successfully");
    return true;
}

void AudioCaptureDevice::Terminate() {
    if (is_initialized_) {
        Pa_Terminate();
        is_initialized_ = false;
        log_info("PortAudio terminated");
    }
}

std::vector<AudioDeviceInfo> AudioCaptureDevice::GetDevices() {
    std::vector<AudioDeviceInfo> devices;

    if (!is_initialized_ && !Initialize()) {
        return devices;
    }

    int num_devices = Pa_GetDeviceCount();
    if (num_devices < 0) {
        log_error("Pa_GetDeviceCount failed");
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
        log_error("Device already open");
        return false;
    }

    sample_rate_ = sample_rate;
    channels_ = channels;

    // Use default device if -1
    if (device_id < 0) {
        device_id = Pa_GetDefaultInputDevice();
    }

    if (device_id == paNoDevice) {
        log_error("No default input device found");
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
        log_error("Failed to open stream: " + std::string(Pa_GetErrorText(err)));
        stream_ = nullptr;
        return false;
    }

    log_info("Audio device opened: " + std::string(Pa_GetDeviceInfo(device_id)->name));
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

    // Check for input overflow
    if (status_flags & paInputOverflow) {
        log_error("Input overflow detected");
    }

    // Call user callback with audio data
    const int16_t* samples = static_cast<const int16_t*>(input_buffer);
    size_t num_samples = frames_per_buffer * self->channels_;

    self->user_callback_(samples, num_samples);

    return paContinue;
}

bool AudioCaptureDevice::Start(AudioCallback callback) {
    if (!stream_) {
        log_error("Device not open");
        return false;
    }

    if (is_capturing_) {
        log_error("Already capturing");
        return false;
    }

    user_callback_ = callback;

    // Close and reopen with callback
    Pa_CloseStream(stream_);

    // Use the stored device ID
    PaStreamParameters input_params;
    input_params.device = device_id_;
    input_params.channelCount = channels_;
    input_params.sampleFormat = paInt16;
    input_params.suggestedLatency = Pa_GetDeviceInfo(device_id_)->defaultLowInputLatency;
    input_params.hostApiSpecificStreamInfo = nullptr;

    PaError err = Pa_OpenStream(&stream_, &input_params, nullptr, sample_rate_,
                                256,  // frames per buffer
                                paClipOff, PortAudioCallback,
                                this  // User data
    );

    if (err != paNoError) {
        log_error("Failed to reopen stream with callback: " + std::string(Pa_GetErrorText(err)));
        return false;
    }

    err = Pa_StartStream(stream_);
    if (err != paNoError) {
        log_error("Failed to start stream: " + std::string(Pa_GetErrorText(err)));
        return false;
    }

    is_capturing_ = true;
    log_info("Audio capture started");
    return true;
}

void AudioCaptureDevice::Stop() {
    if (!stream_ || !is_capturing_) {
        return;
    }

    PaError err = Pa_StopStream(stream_);
    if (err != paNoError) {
        log_error("Failed to stop stream: " + std::string(Pa_GetErrorText(err)));
    }

    is_capturing_ = false;
    log_info("Audio capture stopped");
}

void AudioCaptureDevice::Close() {
    if (is_capturing_) {
        Stop();
    }

    if (stream_) {
        Pa_CloseStream(stream_);
        stream_ = nullptr;
        log_info("Audio device closed");
    }
}

}  // namespace ffvoice
