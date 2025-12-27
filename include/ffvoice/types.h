/**
 * @file types.h
 * @brief Common type definitions for ffvoice-engine
 */

#pragma once

#include <cstdint>
#include <string>
#include <vector>

namespace ffvoice {

/**
 * Audio format types
 */
enum class AudioFormat {
    FLOAT32,  ///< 32-bit floating point
    INT16,    ///< 16-bit signed integer
    INT24,    ///< 24-bit signed integer
    INT32     ///< 32-bit signed integer
};

/**
 * Audio file formats
 */
enum class AudioFileFormat {
    WAV,  ///< WAV (PCM)
    FLAC  ///< FLAC (lossless compression)
};

/**
 * Log levels
 */
enum class LogLevel { ERROR = 0, WARNING = 1, INFO = 2, DEBUG = 3 };

/**
 * Audio stream configuration
 */
struct AudioStreamConfig {
    int device_id = 0;        ///< Audio device ID
    int sample_rate = 48000;  ///< Sample rate in Hz
    int channels = 1;         ///< Number of channels (1=mono, 2=stereo)
    int buffer_frames = 256;  ///< Buffer size in frames
    AudioFormat format = AudioFormat::FLOAT32;
};

/**
 * Audio file configuration
 */
struct AudioFileConfig {
    AudioFileFormat format = AudioFileFormat::WAV;
    int sample_rate = 48000;
    int channels = 1;
    int bit_depth = 16;              ///< Bit depth for PCM (16 or 24)
    int flac_compression_level = 5;  ///< FLAC compression (0-8)
    std::string output_path;
};

/**
 * Device information
 */
struct AudioDeviceInfo {
    int id;
    std::string name;
    int max_input_channels;
    int max_output_channels;
    std::vector<int> supported_sample_rates;
    bool is_default;
};

}  // namespace ffvoice
