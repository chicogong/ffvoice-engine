/**
 * @file signal_generator.cpp
 * @brief Signal generator implementation
 */

#include "signal_generator.h"

#include <cmath>
#include <random>

namespace ffvoice {

constexpr double PI = 3.14159265358979323846;
constexpr int16_t MAX_INT16 = 32767;

std::vector<int16_t> SignalGenerator::GenerateSineWave(double frequency, double duration,
                                                       int sample_rate, double amplitude) {
    size_t num_samples = static_cast<size_t>(duration * sample_rate);
    std::vector<int16_t> samples(num_samples);

    double angular_frequency = 2.0 * PI * frequency;

    for (size_t i = 0; i < num_samples; ++i) {
        double t = static_cast<double>(i) / sample_rate;
        double value = amplitude * std::sin(angular_frequency * t);
        samples[i] = static_cast<int16_t>(value * MAX_INT16);
    }

    return samples;
}

std::vector<int16_t> SignalGenerator::GenerateSilence(double duration, int sample_rate) {
    size_t num_samples = static_cast<size_t>(duration * sample_rate);
    return std::vector<int16_t>(num_samples, 0);
}

std::vector<int16_t> SignalGenerator::GenerateWhiteNoise(double duration, int sample_rate,
                                                         double amplitude) {
    size_t num_samples = static_cast<size_t>(duration * sample_rate);
    std::vector<int16_t> samples(num_samples);

    std::random_device rd;
    std::mt19937 gen(rd());
    std::uniform_real_distribution<> dis(-1.0, 1.0);

    for (size_t i = 0; i < num_samples; ++i) {
        double value = amplitude * dis(gen);
        samples[i] = static_cast<int16_t>(value * MAX_INT16);
    }

    return samples;
}

}  // namespace ffvoice
