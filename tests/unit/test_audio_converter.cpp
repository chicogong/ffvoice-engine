/**
 * @file test_audio_converter.cpp
 * @brief Unit tests for AudioConverter
 * @note Only compiled when ENABLE_WHISPER is defined
 */

#ifdef ENABLE_WHISPER

#include "utils/audio_converter.h"

#include <gtest/gtest.h>

#include <cmath>
#include <vector>
#include <limits>

using namespace ffvoice;

class AudioConverterTest : public ::testing::Test {
protected:
    // Helper: Check if two float arrays are approximately equal
    bool FloatsApproxEqual(const float* a, const float* b, size_t size, float epsilon = 1e-5f) {
        for (size_t i = 0; i < size; ++i) {
            if (std::abs(a[i] - b[i]) > epsilon) {
                return false;
            }
        }
        return true;
    }
};

// =============================================================================
// Int16ToFloat Tests
// =============================================================================

TEST_F(AudioConverterTest, Int16ToFloat_ZeroSamples) {
    std::vector<int16_t> input = {0, 0, 0, 0};
    std::vector<float> output(4);
    
    AudioConverter::Int16ToFloat(input.data(), input.size(), output.data());
    
    for (const auto& sample : output) {
        EXPECT_FLOAT_EQ(0.0f, sample);
    }
}

TEST_F(AudioConverterTest, Int16ToFloat_MaxPositive) {
    std::vector<int16_t> input = {32767};
    std::vector<float> output(1);
    
    AudioConverter::Int16ToFloat(input.data(), input.size(), output.data());
    
    // Should be close to 1.0 (32767/32768 = 0.999969...)
    EXPECT_NEAR(1.0f, output[0], 0.001f);
}

TEST_F(AudioConverterTest, Int16ToFloat_MaxNegative) {
    std::vector<int16_t> input = {-32768};
    std::vector<float> output(1);
    
    AudioConverter::Int16ToFloat(input.data(), input.size(), output.data());
    
    // Should be exactly -1.0
    EXPECT_FLOAT_EQ(-1.0f, output[0]);
}

TEST_F(AudioConverterTest, Int16ToFloat_MixedValues) {
    std::vector<int16_t> input = {0, 16384, -16384, 32767, -32768};
    std::vector<float> output(5);
    
    AudioConverter::Int16ToFloat(input.data(), input.size(), output.data());
    
    EXPECT_FLOAT_EQ(0.0f, output[0]);
    EXPECT_NEAR(0.5f, output[1], 0.001f);
    EXPECT_NEAR(-0.5f, output[2], 0.001f);
    EXPECT_NEAR(1.0f, output[3], 0.001f);
    EXPECT_FLOAT_EQ(-1.0f, output[4]);
}

// =============================================================================
// FloatToInt16 Tests
// =============================================================================

TEST_F(AudioConverterTest, FloatToInt16_ZeroSamples) {
    std::vector<float> input = {0.0f, 0.0f, 0.0f};
    std::vector<int16_t> output(3);
    
    AudioConverter::FloatToInt16(input.data(), input.size(), output.data());
    
    for (const auto& sample : output) {
        EXPECT_EQ(0, sample);
    }
}

TEST_F(AudioConverterTest, FloatToInt16_MaxPositive) {
    std::vector<float> input = {1.0f};
    std::vector<int16_t> output(1);
    
    AudioConverter::FloatToInt16(input.data(), input.size(), output.data());
    
    EXPECT_EQ(32767, output[0]);
}

TEST_F(AudioConverterTest, FloatToInt16_MaxNegative) {
    std::vector<float> input = {-1.0f};
    std::vector<int16_t> output(1);
    
    AudioConverter::FloatToInt16(input.data(), input.size(), output.data());
    
    // Implementation may use -32767 to avoid asymmetry issues
    EXPECT_LE(output[0], -32767);
}

TEST_F(AudioConverterTest, FloatToInt16_Clipping) {
    // Values outside [-1, 1] should be clamped
    std::vector<float> input = {2.0f, -2.0f, 1.5f, -1.5f};
    std::vector<int16_t> output(4);
    
    AudioConverter::FloatToInt16(input.data(), input.size(), output.data());
    
    EXPECT_EQ(32767, output[0]);   // Clamped to max
    EXPECT_LE(output[1], -32767);  // Clamped to min (may be -32767 or -32768)
    EXPECT_EQ(32767, output[2]);   // Clamped to max
    EXPECT_LE(output[3], -32767);  // Clamped to min
}

TEST_F(AudioConverterTest, FloatToInt16_RoundTrip) {
    // Convert int16 -> float -> int16 should be close to original
    std::vector<int16_t> original = {0, 100, -100, 16000, -16000, 32000, -32000};
    std::vector<float> intermediate(original.size());
    std::vector<int16_t> result(original.size());
    
    AudioConverter::Int16ToFloat(original.data(), original.size(), intermediate.data());
    AudioConverter::FloatToInt16(intermediate.data(), intermediate.size(), result.data());
    
    for (size_t i = 0; i < original.size(); ++i) {
        EXPECT_NEAR(original[i], result[i], 1);  // Allow rounding error of 1
    }
}

// =============================================================================
// StereoToMono Tests
// =============================================================================

TEST_F(AudioConverterTest, StereoToMono_Silence) {
    std::vector<float> stereo = {0.0f, 0.0f, 0.0f, 0.0f};  // 2 frames
    std::vector<float> mono(2);
    
    AudioConverter::StereoToMono(stereo.data(), 2, mono.data());
    
    EXPECT_FLOAT_EQ(0.0f, mono[0]);
    EXPECT_FLOAT_EQ(0.0f, mono[1]);
}

TEST_F(AudioConverterTest, StereoToMono_IdenticalChannels) {
    std::vector<float> stereo = {0.5f, 0.5f, -0.5f, -0.5f};  // 2 frames
    std::vector<float> mono(2);
    
    AudioConverter::StereoToMono(stereo.data(), 2, mono.data());
    
    EXPECT_FLOAT_EQ(0.5f, mono[0]);
    EXPECT_FLOAT_EQ(-0.5f, mono[1]);
}

TEST_F(AudioConverterTest, StereoToMono_DifferentChannels) {
    std::vector<float> stereo = {1.0f, 0.0f, 0.0f, 1.0f};  // 2 frames: (1,0), (0,1)
    std::vector<float> mono(2);
    
    AudioConverter::StereoToMono(stereo.data(), 2, mono.data());
    
    EXPECT_FLOAT_EQ(0.5f, mono[0]);  // (1+0)/2 = 0.5
    EXPECT_FLOAT_EQ(0.5f, mono[1]);  // (0+1)/2 = 0.5
}

TEST_F(AudioConverterTest, StereoToMono_Cancellation) {
    // Left and right cancel out
    std::vector<float> stereo = {0.5f, -0.5f, 1.0f, -1.0f};
    std::vector<float> mono(2);
    
    AudioConverter::StereoToMono(stereo.data(), 2, mono.data());
    
    EXPECT_FLOAT_EQ(0.0f, mono[0]);  // (0.5 + -0.5)/2 = 0
    EXPECT_FLOAT_EQ(0.0f, mono[1]);  // (1.0 + -1.0)/2 = 0
}

// =============================================================================
// Resample Tests
// =============================================================================

TEST_F(AudioConverterTest, Resample_SameRate) {
    std::vector<float> input = {0.0f, 0.5f, 1.0f, 0.5f, 0.0f};
    std::vector<float> output(5);
    
    AudioConverter::Resample(input.data(), input.size(), 48000,
                            output.data(), output.size(), 48000);
    
    for (size_t i = 0; i < input.size(); ++i) {
        EXPECT_NEAR(input[i], output[i], 0.01f);
    }
}

TEST_F(AudioConverterTest, Resample_Downsample3x) {
    // 48kHz to 16kHz (3:1 ratio)
    // Create a simple signal: 9 samples -> 3 samples
    std::vector<float> input = {0.0f, 0.1f, 0.2f, 0.3f, 0.4f, 0.5f, 0.6f, 0.7f, 0.8f};
    size_t output_size = input.size() / 3;
    std::vector<float> output(output_size);
    
    AudioConverter::Resample(input.data(), input.size(), 48000,
                            output.data(), output_size, 16000);
    
    // Output should sample from input at intervals
    EXPECT_EQ(3u, output_size);
    // Linear interpolation should give reasonable values
    EXPECT_GE(output[0], 0.0f);
    EXPECT_LE(output[2], 0.9f);
}

TEST_F(AudioConverterTest, Resample_Upsample2x) {
    // 24kHz to 48kHz (1:2 ratio)
    std::vector<float> input = {0.0f, 1.0f, 0.0f};
    size_t output_size = input.size() * 2;
    std::vector<float> output(output_size);
    
    AudioConverter::Resample(input.data(), input.size(), 24000,
                            output.data(), output_size, 48000);
    
    EXPECT_EQ(6u, output_size);
    // First sample should be close to 0
    EXPECT_NEAR(0.0f, output[0], 0.1f);
}

TEST_F(AudioConverterTest, Resample_PreservesEnergy) {
    // Create a sine wave and verify energy is approximately preserved
    const size_t input_size = 480;  // 10ms at 48kHz
    const size_t output_size = 160; // 10ms at 16kHz
    std::vector<float> input(input_size);
    std::vector<float> output(output_size);
    
    // Generate sine wave
    for (size_t i = 0; i < input_size; ++i) {
        input[i] = std::sin(2.0 * M_PI * 440.0 * i / 48000.0);
    }
    
    AudioConverter::Resample(input.data(), input_size, 48000,
                            output.data(), output_size, 16000);
    
    // Calculate RMS of both
    double input_rms = 0.0, output_rms = 0.0;
    for (size_t i = 0; i < input_size; ++i) {
        input_rms += input[i] * input[i];
    }
    for (size_t i = 0; i < output_size; ++i) {
        output_rms += output[i] * output[i];
    }
    input_rms = std::sqrt(input_rms / input_size);
    output_rms = std::sqrt(output_rms / output_size);
    
    // RMS should be similar (within 20%)
    EXPECT_NEAR(input_rms, output_rms, input_rms * 0.2);
}

// =============================================================================
// LoadAndConvert Tests (File I/O)
// =============================================================================

TEST_F(AudioConverterTest, LoadAndConvert_NonexistentFile) {
    std::vector<float> pcm_data;
    bool result = AudioConverter::LoadAndConvert("/nonexistent/path/file.wav", pcm_data);
    
    EXPECT_FALSE(result);
    EXPECT_TRUE(pcm_data.empty());
}

TEST_F(AudioConverterTest, LoadAndConvert_UnsupportedExtension) {
    std::vector<float> pcm_data;
    bool result = AudioConverter::LoadAndConvert("/tmp/test.mp3", pcm_data);
    
    EXPECT_FALSE(result);
}

#endif  // ENABLE_WHISPER
