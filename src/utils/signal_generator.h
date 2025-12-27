/**
 * @file signal_generator.h
 * @brief Simple audio signal generator for testing
 */

#pragma once

#include <cmath>
#include <cstdint>
#include <vector>

namespace ffvoice {

/**
 * @brief Generate test audio signals
 */
class SignalGenerator {
public:
    /**
     * @brief Generate a sine wave
     * @param frequency Frequency in Hz (e.g., 440 for A4)
     * @param duration Duration in seconds
     * @param sample_rate Sample rate in Hz (e.g., 48000)
     * @param amplitude Amplitude (0.0 to 1.0)
     * @return Vector of PCM samples (int16_t)
     */
    static std::vector<int16_t> GenerateSineWave(double frequency, double duration,
                                                 int sample_rate = 48000, double amplitude = 0.5);

    /**
     * @brief Generate silence (all zeros)
     * @param duration Duration in seconds
     * @param sample_rate Sample rate in Hz
     * @return Vector of zero samples
     */
    static std::vector<int16_t> GenerateSilence(double duration, int sample_rate = 48000);

    /**
     * @brief Generate white noise
     * @param duration Duration in seconds
     * @param sample_rate Sample rate in Hz
     * @param amplitude Amplitude (0.0 to 1.0)
     * @return Vector of random samples
     */
    static std::vector<int16_t> GenerateWhiteNoise(double duration, int sample_rate = 48000,
                                                   double amplitude = 0.1);
};

}  // namespace ffvoice
