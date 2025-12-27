/**
 * @file test_signal_generator.cpp
 * @brief Unit tests for SignalGenerator
 */

#include "utils/signal_generator.h"

#include <gtest/gtest.h>

#include <algorithm>
#include <cmath>

using namespace ffvoice;

constexpr double PI = 3.14159265358979323846;

class SignalGeneratorTest : public ::testing::Test {
protected:
    // Helper: Calculate RMS value
    double CalculateRMS(const std::vector<int16_t>& samples) {
        if (samples.empty())
            return 0.0;

        double sum = 0.0;
        for (const auto& sample : samples) {
            sum += static_cast<double>(sample) * sample;
        }
        return std::sqrt(sum / samples.size());
    }

    // Helper: Count zero crossings
    size_t CountZeroCrossings(const std::vector<int16_t>& samples) {
        if (samples.size() < 2)
            return 0;

        size_t count = 0;
        for (size_t i = 1; i < samples.size(); ++i) {
            if ((samples[i - 1] >= 0 && samples[i] < 0) ||
                (samples[i - 1] < 0 && samples[i] >= 0)) {
                count++;
            }
        }
        return count;
    }

    // Helper: Find peak amplitude
    int16_t FindPeak(const std::vector<int16_t>& samples) {
        if (samples.empty())
            return 0;
        auto it = std::max_element(samples.begin(), samples.end(),
                                   [](int16_t a, int16_t b) { return std::abs(a) < std::abs(b); });
        return *it;
    }
};

// ============================================================================
// Sine Wave Tests
// ============================================================================

TEST_F(SignalGeneratorTest, GenerateSineWaveBasic) {
    auto samples = SignalGenerator::GenerateSineWave(440.0, 1.0, 48000, 0.5);

    // Check size
    EXPECT_EQ(samples.size(), 48000);

    // Check it's not all zeros
    double rms = CalculateRMS(samples);
    EXPECT_GT(rms, 0.0);
}

TEST_F(SignalGeneratorTest, SineWaveDuration) {
    // Test different durations
    auto samples_1s = SignalGenerator::GenerateSineWave(440.0, 1.0, 48000);
    auto samples_2s = SignalGenerator::GenerateSineWave(440.0, 2.0, 48000);
    auto samples_05s = SignalGenerator::GenerateSineWave(440.0, 0.5, 48000);

    EXPECT_EQ(samples_1s.size(), 48000);
    EXPECT_EQ(samples_2s.size(), 96000);
    EXPECT_EQ(samples_05s.size(), 24000);
}

TEST_F(SignalGeneratorTest, SineWaveSampleRate) {
    // Test different sample rates
    auto samples_44k = SignalGenerator::GenerateSineWave(440.0, 1.0, 44100);
    auto samples_48k = SignalGenerator::GenerateSineWave(440.0, 1.0, 48000);
    auto samples_96k = SignalGenerator::GenerateSineWave(440.0, 1.0, 96000);

    EXPECT_EQ(samples_44k.size(), 44100);
    EXPECT_EQ(samples_48k.size(), 48000);
    EXPECT_EQ(samples_96k.size(), 96000);
}

TEST_F(SignalGeneratorTest, SineWaveAmplitude) {
    // Test different amplitudes
    auto samples_full = SignalGenerator::GenerateSineWave(440.0, 1.0, 48000, 1.0);
    auto samples_half = SignalGenerator::GenerateSineWave(440.0, 1.0, 48000, 0.5);
    auto samples_quarter = SignalGenerator::GenerateSineWave(440.0, 1.0, 48000, 0.25);

    double rms_full = CalculateRMS(samples_full);
    double rms_half = CalculateRMS(samples_half);
    double rms_quarter = CalculateRMS(samples_quarter);

    // Higher amplitude should have higher RMS
    EXPECT_GT(rms_full, rms_half);
    EXPECT_GT(rms_half, rms_quarter);

    // Check approximate ratios
    EXPECT_NEAR(rms_full / rms_half, 2.0, 0.1);
    EXPECT_NEAR(rms_half / rms_quarter, 2.0, 0.1);
}

TEST_F(SignalGeneratorTest, SineWaveFrequency) {
    int sample_rate = 48000;
    double duration = 1.0;

    // Generate 440Hz and 880Hz tones
    auto samples_440 = SignalGenerator::GenerateSineWave(440.0, duration, sample_rate);
    auto samples_880 = SignalGenerator::GenerateSineWave(880.0, duration, sample_rate);

    // Count zero crossings (should be approximately 2 * frequency)
    size_t crossings_440 = CountZeroCrossings(samples_440);
    size_t crossings_880 = CountZeroCrossings(samples_880);

    // 440Hz should have ~880 crossings, 880Hz should have ~1760 crossings
    EXPECT_NEAR(crossings_440, 880.0, 50.0);
    EXPECT_NEAR(crossings_880, 1760.0, 50.0);

    // Higher frequency should have more crossings
    EXPECT_GT(crossings_880, crossings_440);
}

TEST_F(SignalGeneratorTest, SineWavePeakAmplitude) {
    // Amplitude 0.5 should give peak around 16383 (half of 32767)
    auto samples = SignalGenerator::GenerateSineWave(440.0, 1.0, 48000, 0.5);

    int16_t peak = FindPeak(samples);
    EXPECT_NEAR(std::abs(peak), 16383, 500);  // Allow some tolerance
}

TEST_F(SignalGeneratorTest, SineWaveSymmetry) {
    // Sine wave should be symmetric around zero
    auto samples = SignalGenerator::GenerateSineWave(440.0, 1.0, 48000, 0.5);

    int64_t sum = 0;
    for (const auto& sample : samples) {
        sum += sample;
    }

    // Average should be close to zero
    double average = static_cast<double>(sum) / samples.size();
    EXPECT_NEAR(average, 0.0, 10.0);
}

// ============================================================================
// Silence Tests
// ============================================================================

TEST_F(SignalGeneratorTest, GenerateSilenceBasic) {
    auto samples = SignalGenerator::GenerateSilence(1.0, 48000);

    EXPECT_EQ(samples.size(), 48000);

    // All samples should be zero
    for (const auto& sample : samples) {
        EXPECT_EQ(sample, 0);
    }
}

TEST_F(SignalGeneratorTest, SilenceDuration) {
    auto samples_1s = SignalGenerator::GenerateSilence(1.0, 48000);
    auto samples_2s = SignalGenerator::GenerateSilence(2.0, 48000);
    auto samples_05s = SignalGenerator::GenerateSilence(0.5, 48000);

    EXPECT_EQ(samples_1s.size(), 48000);
    EXPECT_EQ(samples_2s.size(), 96000);
    EXPECT_EQ(samples_05s.size(), 24000);
}

TEST_F(SignalGeneratorTest, SilenceRMS) {
    auto samples = SignalGenerator::GenerateSilence(1.0, 48000);
    double rms = CalculateRMS(samples);
    EXPECT_EQ(rms, 0.0);
}

// ============================================================================
// White Noise Tests
// ============================================================================

TEST_F(SignalGeneratorTest, GenerateWhiteNoiseBasic) {
    auto samples = SignalGenerator::GenerateWhiteNoise(1.0, 48000, 0.1);

    EXPECT_EQ(samples.size(), 48000);

    // Should not be all zeros
    double rms = CalculateRMS(samples);
    EXPECT_GT(rms, 0.0);
}

TEST_F(SignalGeneratorTest, WhiteNoiseDuration) {
    auto samples_1s = SignalGenerator::GenerateWhiteNoise(1.0, 48000);
    auto samples_2s = SignalGenerator::GenerateWhiteNoise(2.0, 48000);

    EXPECT_EQ(samples_1s.size(), 48000);
    EXPECT_EQ(samples_2s.size(), 96000);
}

TEST_F(SignalGeneratorTest, WhiteNoiseAmplitude) {
    auto samples_low = SignalGenerator::GenerateWhiteNoise(1.0, 48000, 0.1);
    auto samples_high = SignalGenerator::GenerateWhiteNoise(1.0, 48000, 0.5);

    double rms_low = CalculateRMS(samples_low);
    double rms_high = CalculateRMS(samples_high);

    // Higher amplitude should have higher RMS
    EXPECT_GT(rms_high, rms_low);
}

TEST_F(SignalGeneratorTest, WhiteNoiseRandomness) {
    // Generate two noise sequences
    auto noise1 = SignalGenerator::GenerateWhiteNoise(0.1, 48000, 0.1);
    auto noise2 = SignalGenerator::GenerateWhiteNoise(0.1, 48000, 0.1);

    // They should be different (not identical)
    bool all_same = true;
    for (size_t i = 0; i < noise1.size(); ++i) {
        if (noise1[i] != noise2[i]) {
            all_same = false;
            break;
        }
    }
    EXPECT_FALSE(all_same);
}

TEST_F(SignalGeneratorTest, WhiteNoiseDistribution) {
    auto samples = SignalGenerator::GenerateWhiteNoise(1.0, 48000, 0.5);

    // Count positive and negative samples
    int positive = 0, negative = 0;
    for (const auto& sample : samples) {
        if (sample > 0)
            positive++;
        else if (sample < 0)
            negative++;
    }

    // Should be roughly 50/50
    double ratio = static_cast<double>(positive) / samples.size();
    EXPECT_NEAR(ratio, 0.5, 0.05);  // Allow 5% deviation
}

TEST_F(SignalGeneratorTest, WhiteNoiseZeroCrossings) {
    // White noise should have many zero crossings
    auto samples = SignalGenerator::GenerateWhiteNoise(1.0, 48000, 0.1);

    size_t crossings = CountZeroCrossings(samples);

    // Should have at least 25% of samples as crossings (white noise is very noisy)
    EXPECT_GT(crossings, samples.size() / 4);
}

// ============================================================================
// Edge Cases
// ============================================================================

TEST_F(SignalGeneratorTest, ZeroDuration) {
    auto samples = SignalGenerator::GenerateSineWave(440.0, 0.0, 48000);
    EXPECT_EQ(samples.size(), 0);
}

TEST_F(SignalGeneratorTest, VeryShortDuration) {
    // 1 millisecond
    auto samples = SignalGenerator::GenerateSineWave(440.0, 0.001, 48000);
    EXPECT_EQ(samples.size(), 48);  // 48000 * 0.001 = 48
}

TEST_F(SignalGeneratorTest, ZeroAmplitude) {
    auto samples = SignalGenerator::GenerateSineWave(440.0, 1.0, 48000, 0.0);

    // Should be all zeros
    for (const auto& sample : samples) {
        EXPECT_EQ(sample, 0);
    }
}

TEST_F(SignalGeneratorTest, LowFrequency) {
    // Very low frequency (sub-sonic)
    auto samples = SignalGenerator::GenerateSineWave(1.0, 1.0, 48000);
    EXPECT_EQ(samples.size(), 48000);

    // Should have very few zero crossings
    size_t crossings = CountZeroCrossings(samples);
    EXPECT_LT(crossings, 10);  // ~2 crossings expected for 1Hz
}

TEST_F(SignalGeneratorTest, HighFrequency) {
    // High frequency near Nyquist limit
    auto samples = SignalGenerator::GenerateSineWave(20000.0, 1.0, 48000);
    EXPECT_EQ(samples.size(), 48000);

    // Should still generate valid samples
    double rms = CalculateRMS(samples);
    EXPECT_GT(rms, 0.0);
}

// ============================================================================
// Comparison Tests
// ============================================================================

TEST_F(SignalGeneratorTest, SineVsSilence) {
    auto sine = SignalGenerator::GenerateSineWave(440.0, 1.0, 48000, 0.5);
    auto silence = SignalGenerator::GenerateSilence(1.0, 48000);

    double rms_sine = CalculateRMS(sine);
    double rms_silence = CalculateRMS(silence);

    EXPECT_GT(rms_sine, rms_silence);
    EXPECT_EQ(rms_silence, 0.0);
}

TEST_F(SignalGeneratorTest, SineVsNoise) {
    auto sine = SignalGenerator::GenerateSineWave(440.0, 1.0, 48000, 0.1);
    auto noise = SignalGenerator::GenerateWhiteNoise(1.0, 48000, 0.1);

    // Noise should have more zero crossings than a pure tone
    size_t crossings_sine = CountZeroCrossings(sine);
    size_t crossings_noise = CountZeroCrossings(noise);

    EXPECT_GT(crossings_noise, crossings_sine);
}
