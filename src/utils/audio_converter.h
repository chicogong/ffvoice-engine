/**
 * @file audio_converter.h
 * @brief Audio format conversion utilities for Whisper ASR
 *
 * Provides utilities to convert various audio formats to Whisper's required format:
 * - Sample rate: 16000 Hz
 * - Format: float32 (normalized to [-1.0, 1.0])
 * - Channels: mono
 */

#pragma once

#include <cstdint>
#include <string>
#include <vector>

namespace ffvoice {

/**
 * @brief Audio format conversion utilities
 *
 * This class provides static utility methods for converting audio between different
 * formats. Primarily used to convert recorded audio (48kHz, int16, stereo) to
 * Whisper's required format (16kHz, float, mono).
 *
 * Conversion pipeline:
 * @code
 * WAV/FLAC file (48kHz, int16, stereo)
 *   → Read file
 *   → Convert to float (48kHz, float, stereo)
 *   → Stereo to mono (48kHz, float, mono)
 *   → Resample (16kHz, float, mono)  ← Whisper input
 * @endcode
 */
class AudioConverter {
public:
    /**
     * @brief Load and convert audio file to Whisper format
     *
     * Loads a WAV or FLAC file and converts it to the format required by Whisper:
     * - 16kHz sample rate
     * - float32 format
     * - mono channel
     *
     * @param filename Path to audio file (.wav or .flac)
     * @param pcm_data Output PCM data in Whisper format
     * @param target_sample_rate Target sample rate (default: 16000 Hz)
     * @return true if successful, false otherwise
     */
    static bool LoadAndConvert(const std::string& filename, std::vector<float>& pcm_data,
                               int target_sample_rate = 16000);

    /**
     * @brief Convert int16 samples to normalized float
     *
     * Converts int16_t audio samples to float format normalized to [-1.0, 1.0].
     *
     * @param input Input samples (int16_t format)
     * @param num_samples Number of samples to convert
     * @param output Output buffer (must be pre-allocated, same size as input)
     */
    static void Int16ToFloat(const int16_t* input, size_t num_samples, float* output);

    /**
     * @brief Convert normalized float samples to int16
     *
     * Converts float audio samples (range [-1.0, 1.0]) to int16_t format.
     * Values outside [-1.0, 1.0] are clamped.
     *
     * @param input Input samples (float format, [-1.0, 1.0])
     * @param num_samples Number of samples to convert
     * @param output Output buffer (must be pre-allocated, same size as input)
     */
    static void FloatToInt16(const float* input, size_t num_samples, int16_t* output);

    /**
     * @brief Resample audio using linear interpolation
     *
     * Simple linear interpolation resampler. For better quality, consider using
     * a more sophisticated resampler (e.g., libsamplerate).
     *
     * @param input Input samples
     * @param input_size Number of input samples
     * @param input_rate Input sample rate (e.g., 48000)
     * @param output Output buffer (must be pre-allocated)
     * @param output_size Number of output samples
     * @param output_rate Output sample rate (e.g., 16000)
     */
    static void Resample(const float* input, size_t input_size, int input_rate, float* output,
                         size_t output_size, int output_rate);

    /**
     * @brief Convert stereo to mono by averaging channels
     *
     * Converts interleaved stereo audio to mono by averaging left and right channels.
     *
     * @param stereo Input stereo samples (interleaved: L R L R ...)
     * @param num_frames Number of frames (= num_samples / 2)
     * @param mono Output mono samples (must be pre-allocated, size = num_frames)
     */
    static void StereoToMono(const float* stereo, size_t num_frames, float* mono);

private:
    /**
     * @brief Load WAV file
     * @param filename Path to WAV file
     * @param pcm_data Output PCM data (float format)
     * @param sample_rate Output sample rate
     * @param channels Output number of channels
     * @return true if successful, false otherwise
     */
    static bool LoadWAV(const std::string& filename, std::vector<float>& pcm_data, int& sample_rate,
                        int& channels);

    /**
     * @brief Load FLAC file
     * @param filename Path to FLAC file
     * @param pcm_data Output PCM data (float format)
     * @param sample_rate Output sample rate
     * @param channels Output number of channels
     * @return true if successful, false otherwise
     */
    static bool LoadFLAC(const std::string& filename, std::vector<float>& pcm_data,
                         int& sample_rate, int& channels);
};

}  // namespace ffvoice
