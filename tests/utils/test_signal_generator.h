/**
 * @file test_signal_generator.h
 * @brief Utility for generating test audio signals
 *
 * Provides comprehensive signal generation capabilities for testing
 * audio processing algorithms, including various waveforms, noise,
 * and complex test patterns.
 */

#ifndef FFVOICE_TESTS_UTILS_TEST_SIGNAL_GENERATOR_H
#define FFVOICE_TESTS_UTILS_TEST_SIGNAL_GENERATOR_H

#include <vector>
#include <cmath>
#include <cstdint>
#include <random>
#include <complex>
#include <algorithm>

namespace ffvoice {
namespace test {

/**
 * @class TestSignalGenerator
 * @brief Generates various audio test signals
 *
 * Provides methods to generate standard test signals including
 * sine waves, square waves, noise, chirps, and more for testing
 * audio processing algorithms.
 */
class TestSignalGenerator {
public:
    /**
     * @brief Constructor
     *
     * @param sample_rate Sample rate in Hz
     */
    explicit TestSignalGenerator(uint32_t sample_rate = 16000)
        : sample_rate_(sample_rate),
          rng_(std::random_device{}()) {}

    /**
     * @brief Generate silence (zeros)
     *
     * @param duration_ms Duration in milliseconds
     * @return Vector of samples (all zeros)
     */
    std::vector<int16_t> GenerateSilence(uint32_t duration_ms) {
        size_t num_samples = SamplesToGenerate(duration_ms);
        return std::vector<int16_t>(num_samples, 0);
    }

    /**
     * @brief Generate DC offset
     *
     * @param duration_ms Duration in milliseconds
     * @param offset DC offset value
     * @return Vector of samples (constant value)
     */
    std::vector<int16_t> GenerateDCOffset(uint32_t duration_ms, int16_t offset) {
        size_t num_samples = SamplesToGenerate(duration_ms);
        return std::vector<int16_t>(num_samples, offset);
    }

    /**
     * @brief Generate sine wave
     *
     * @param frequency Frequency in Hz
     * @param duration_ms Duration in milliseconds
     * @param amplitude Amplitude (0.0 to 1.0)
     * @param phase_rad Initial phase in radians
     * @return Vector of sine wave samples
     */
    std::vector<int16_t> GenerateSineWave(
        double frequency,
        uint32_t duration_ms,
        double amplitude = 0.5,
        double phase_rad = 0.0
    ) {
        size_t num_samples = SamplesToGenerate(duration_ms);
        std::vector<int16_t> samples(num_samples);

        const double two_pi = 2.0 * M_PI;
        const int16_t max_amplitude = static_cast<int16_t>(32767 * amplitude);

        for (size_t i = 0; i < num_samples; ++i) {
            double t = static_cast<double>(i) / sample_rate_;
            double value = std::sin(two_pi * frequency * t + phase_rad);
            samples[i] = static_cast<int16_t>(value * max_amplitude);
        }

        return samples;
    }

    /**
     * @brief Generate cosine wave
     *
     * @param frequency Frequency in Hz
     * @param duration_ms Duration in milliseconds
     * @param amplitude Amplitude (0.0 to 1.0)
     * @param phase_rad Initial phase in radians
     * @return Vector of cosine wave samples
     */
    std::vector<int16_t> GenerateCosineWave(
        double frequency,
        uint32_t duration_ms,
        double amplitude = 0.5,
        double phase_rad = 0.0
    ) {
        size_t num_samples = SamplesToGenerate(duration_ms);
        std::vector<int16_t> samples(num_samples);

        const double two_pi = 2.0 * M_PI;
        const int16_t max_amplitude = static_cast<int16_t>(32767 * amplitude);

        for (size_t i = 0; i < num_samples; ++i) {
            double t = static_cast<double>(i) / sample_rate_;
            double value = std::cos(two_pi * frequency * t + phase_rad);
            samples[i] = static_cast<int16_t>(value * max_amplitude);
        }

        return samples;
    }

    /**
     * @brief Generate square wave
     *
     * @param frequency Frequency in Hz
     * @param duration_ms Duration in milliseconds
     * @param amplitude Amplitude (0.0 to 1.0)
     * @param duty_cycle Duty cycle (0.0 to 1.0, default 0.5)
     * @return Vector of square wave samples
     */
    std::vector<int16_t> GenerateSquareWave(
        double frequency,
        uint32_t duration_ms,
        double amplitude = 0.5,
        double duty_cycle = 0.5
    ) {
        size_t num_samples = SamplesToGenerate(duration_ms);
        std::vector<int16_t> samples(num_samples);

        const int16_t max_amplitude = static_cast<int16_t>(32767 * amplitude);
        const double period = sample_rate_ / frequency;

        for (size_t i = 0; i < num_samples; ++i) {
            double phase = std::fmod(i, period) / period;
            samples[i] = (phase < duty_cycle) ? max_amplitude : -max_amplitude;
        }

        return samples;
    }

    /**
     * @brief Generate sawtooth wave
     *
     * @param frequency Frequency in Hz
     * @param duration_ms Duration in milliseconds
     * @param amplitude Amplitude (0.0 to 1.0)
     * @return Vector of sawtooth wave samples
     */
    std::vector<int16_t> GenerateSawtoothWave(
        double frequency,
        uint32_t duration_ms,
        double amplitude = 0.5
    ) {
        size_t num_samples = SamplesToGenerate(duration_ms);
        std::vector<int16_t> samples(num_samples);

        const int16_t max_amplitude = static_cast<int16_t>(32767 * amplitude);
        const double period = sample_rate_ / frequency;

        for (size_t i = 0; i < num_samples; ++i) {
            double phase = std::fmod(i, period) / period;
            double value = 2.0 * phase - 1.0;
            samples[i] = static_cast<int16_t>(value * max_amplitude);
        }

        return samples;
    }

    /**
     * @brief Generate triangle wave
     *
     * @param frequency Frequency in Hz
     * @param duration_ms Duration in milliseconds
     * @param amplitude Amplitude (0.0 to 1.0)
     * @return Vector of triangle wave samples
     */
    std::vector<int16_t> GenerateTriangleWave(
        double frequency,
        uint32_t duration_ms,
        double amplitude = 0.5
    ) {
        size_t num_samples = SamplesToGenerate(duration_ms);
        std::vector<int16_t> samples(num_samples);

        const int16_t max_amplitude = static_cast<int16_t>(32767 * amplitude);
        const double period = sample_rate_ / frequency;

        for (size_t i = 0; i < num_samples; ++i) {
            double phase = std::fmod(i, period) / period;
            double value = (phase < 0.5) ? (4.0 * phase - 1.0) : (3.0 - 4.0 * phase);
            samples[i] = static_cast<int16_t>(value * max_amplitude);
        }

        return samples;
    }

    /**
     * @brief Generate white noise
     *
     * @param duration_ms Duration in milliseconds
     * @param amplitude Amplitude (0.0 to 1.0)
     * @return Vector of white noise samples
     */
    std::vector<int16_t> GenerateWhiteNoise(
        uint32_t duration_ms,
        double amplitude = 0.1
    ) {
        size_t num_samples = SamplesToGenerate(duration_ms);
        std::vector<int16_t> samples(num_samples);

        const int16_t max_amplitude = static_cast<int16_t>(32767 * amplitude);
        std::uniform_real_distribution<double> dist(-1.0, 1.0);

        for (size_t i = 0; i < num_samples; ++i) {
            samples[i] = static_cast<int16_t>(dist(rng_) * max_amplitude);
        }

        return samples;
    }

    /**
     * @brief Generate pink noise (1/f noise)
     *
     * @param duration_ms Duration in milliseconds
     * @param amplitude Amplitude (0.0 to 1.0)
     * @return Vector of pink noise samples
     */
    std::vector<int16_t> GeneratePinkNoise(
        uint32_t duration_ms,
        double amplitude = 0.1
    ) {
        size_t num_samples = SamplesToGenerate(duration_ms);
        std::vector<int16_t> samples(num_samples);

        const int16_t max_amplitude = static_cast<int16_t>(32767 * amplitude);
        std::uniform_real_distribution<double> dist(-1.0, 1.0);

        // Voss-McCartney algorithm for pink noise
        constexpr int num_generators = 16;
        double generators[num_generators] = {0};
        int counter = 0;

        for (size_t i = 0; i < num_samples; ++i) {
            int last_counter = counter;
            counter++;

            // Update generators based on counter bits
            for (int j = 0; j < num_generators; ++j) {
                if ((counter & (1 << j)) != (last_counter & (1 << j))) {
                    generators[j] = dist(rng_);
                }
            }

            // Sum all generators
            double sum = 0.0;
            for (int j = 0; j < num_generators; ++j) {
                sum += generators[j];
            }

            // Normalize and convert to int16_t
            double normalized = sum / num_generators;
            samples[i] = static_cast<int16_t>(normalized * max_amplitude);
        }

        return samples;
    }

    /**
     * @brief Generate linear chirp (sweep)
     *
     * @param f_start Start frequency in Hz
     * @param f_end End frequency in Hz
     * @param duration_ms Duration in milliseconds
     * @param amplitude Amplitude (0.0 to 1.0)
     * @return Vector of chirp samples
     */
    std::vector<int16_t> GenerateChirp(
        double f_start,
        double f_end,
        uint32_t duration_ms,
        double amplitude = 0.5
    ) {
        size_t num_samples = SamplesToGenerate(duration_ms);
        std::vector<int16_t> samples(num_samples);

        const double two_pi = 2.0 * M_PI;
        const int16_t max_amplitude = static_cast<int16_t>(32767 * amplitude);
        const double duration_sec = duration_ms / 1000.0;
        const double k = (f_end - f_start) / duration_sec;

        for (size_t i = 0; i < num_samples; ++i) {
            double t = static_cast<double>(i) / sample_rate_;
            double phase = two_pi * (f_start * t + 0.5 * k * t * t);
            double value = std::sin(phase);
            samples[i] = static_cast<int16_t>(value * max_amplitude);
        }

        return samples;
    }

    /**
     * @brief Generate impulse (delta function)
     *
     * @param duration_ms Duration in milliseconds
     * @param impulse_position Position of impulse (0.0 to 1.0)
     * @param amplitude Amplitude (0.0 to 1.0)
     * @return Vector of impulse samples
     */
    std::vector<int16_t> GenerateImpulse(
        uint32_t duration_ms,
        double impulse_position = 0.5,
        double amplitude = 1.0
    ) {
        size_t num_samples = SamplesToGenerate(duration_ms);
        std::vector<int16_t> samples(num_samples, 0);

        size_t impulse_index = static_cast<size_t>(num_samples * impulse_position);
        if (impulse_index < num_samples) {
            samples[impulse_index] = static_cast<int16_t>(32767 * amplitude);
        }

        return samples;
    }

    /**
     * @brief Generate DTMF (Dual-Tone Multi-Frequency) tone
     *
     * @param digit DTMF digit ('0'-'9', 'A'-'D', '*', '#')
     * @param duration_ms Duration in milliseconds
     * @param amplitude Amplitude (0.0 to 1.0)
     * @return Vector of DTMF tone samples
     */
    std::vector<int16_t> GenerateDTMF(
        char digit,
        uint32_t duration_ms,
        double amplitude = 0.5
    ) {
        // DTMF frequency pairs
        double freq1 = 0.0, freq2 = 0.0;

        switch (digit) {
            case '1': freq1 = 697; freq2 = 1209; break;
            case '2': freq1 = 697; freq2 = 1336; break;
            case '3': freq1 = 697; freq2 = 1477; break;
            case 'A': freq1 = 697; freq2 = 1633; break;
            case '4': freq1 = 770; freq2 = 1209; break;
            case '5': freq1 = 770; freq2 = 1336; break;
            case '6': freq1 = 770; freq2 = 1477; break;
            case 'B': freq1 = 770; freq2 = 1633; break;
            case '7': freq1 = 852; freq2 = 1209; break;
            case '8': freq1 = 852; freq2 = 1336; break;
            case '9': freq1 = 852; freq2 = 1477; break;
            case 'C': freq1 = 852; freq2 = 1633; break;
            case '*': freq1 = 941; freq2 = 1209; break;
            case '0': freq1 = 941; freq2 = 1336; break;
            case '#': freq1 = 941; freq2 = 1477; break;
            case 'D': freq1 = 941; freq2 = 1633; break;
            default: return GenerateSilence(duration_ms);
        }

        // Generate sum of two sine waves
        auto tone1 = GenerateSineWave(freq1, duration_ms, amplitude / 2.0);
        auto tone2 = GenerateSineWave(freq2, duration_ms, amplitude / 2.0);

        std::vector<int16_t> result(tone1.size());
        for (size_t i = 0; i < tone1.size(); ++i) {
            result[i] = tone1[i] + tone2[i];
        }

        return result;
    }

    /**
     * @brief Mix multiple signals together
     *
     * @param signals Vector of signal vectors to mix
     * @param mix_gain Gain to apply to each signal (0.0 to 1.0)
     * @return Mixed signal
     */
    std::vector<int16_t> MixSignals(
        const std::vector<std::vector<int16_t>>& signals,
        double mix_gain = 1.0
    ) {
        if (signals.empty()) return {};

        size_t max_length = 0;
        for (const auto& signal : signals) {
            max_length = std::max(max_length, signal.size());
        }

        std::vector<int16_t> result(max_length, 0);
        double scale = mix_gain / signals.size();

        for (const auto& signal : signals) {
            for (size_t i = 0; i < signal.size(); ++i) {
                int32_t mixed = result[i] + static_cast<int32_t>(signal[i] * scale);
                result[i] = Clamp(mixed);
            }
        }

        return result;
    }

    /**
     * @brief Apply envelope (ADSR) to signal
     *
     * @param signal Input signal
     * @param attack_ms Attack time in milliseconds
     * @param decay_ms Decay time in milliseconds
     * @param sustain_level Sustain level (0.0 to 1.0)
     * @param release_ms Release time in milliseconds
     * @return Signal with envelope applied
     */
    std::vector<int16_t> ApplyEnvelope(
        const std::vector<int16_t>& signal,
        uint32_t attack_ms,
        uint32_t decay_ms,
        double sustain_level,
        uint32_t release_ms
    ) {
        std::vector<int16_t> result = signal;
        size_t attack_samples = (sample_rate_ * attack_ms) / 1000;
        size_t decay_samples = (sample_rate_ * decay_ms) / 1000;
        size_t release_samples = (sample_rate_ * release_ms) / 1000;

        // Attack phase
        for (size_t i = 0; i < std::min(attack_samples, result.size()); ++i) {
            double gain = static_cast<double>(i) / attack_samples;
            result[i] = static_cast<int16_t>(result[i] * gain);
        }

        // Decay phase
        size_t decay_start = attack_samples;
        for (size_t i = 0; i < std::min(decay_samples, result.size() - decay_start); ++i) {
            double gain = 1.0 - (1.0 - sustain_level) * (static_cast<double>(i) / decay_samples);
            result[decay_start + i] = static_cast<int16_t>(result[decay_start + i] * gain);
        }

        // Sustain phase (no change)

        // Release phase
        size_t release_start = result.size() > release_samples ? result.size() - release_samples : 0;
        for (size_t i = 0; i < release_samples && (release_start + i) < result.size(); ++i) {
            double gain = sustain_level * (1.0 - static_cast<double>(i) / release_samples);
            result[release_start + i] = static_cast<int16_t>(result[release_start + i] * gain);
        }

        return result;
    }

private:
    /**
     * @brief Calculate number of samples for given duration
     */
    size_t SamplesToGenerate(uint32_t duration_ms) const {
        return (sample_rate_ * duration_ms) / 1000;
    }

    /**
     * @brief Clamp int32_t value to int16_t range
     */
    int16_t Clamp(int32_t value) const {
        if (value > 32767) return 32767;
        if (value < -32768) return -32768;
        return static_cast<int16_t>(value);
    }

    uint32_t sample_rate_;
    std::mt19937 rng_;
};

} // namespace test
} // namespace ffvoice

#endif // FFVOICE_TESTS_UTILS_TEST_SIGNAL_GENERATOR_H
