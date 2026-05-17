/**
 * @file test_audio_processor.cpp
 * @brief Unit tests for the audio processing chain
 *
 * Covers VolumeNormalizer, HighPassFilter and AudioProcessorChain.
 * These processors run inside the recording callback (apps/cli/main.cpp),
 * so their correctness directly affects every processed recording.
 */

#include "audio/audio_processor.h"

#include <gtest/gtest.h>

#include <cmath>
#include <cstdint>
#include <cstdlib>
#include <memory>
#include <vector>

using namespace ffvoice;

namespace {

/**
 * @brief Minimal AudioProcessor used to observe AudioProcessorChain behavior.
 *
 * Records how many times each interface method is invoked so chain ordering
 * and propagation can be asserted without depending on real DSP output.
 */
class CountingProcessor : public AudioProcessor {
public:
    bool Initialize(int sample_rate, int channels) override {
        sample_rate_ = sample_rate;
        channels_ = channels;
        ++init_calls;
        return init_should_succeed;
    }

    void Process(int16_t* samples, size_t num_samples) override {
        (void)samples;
        (void)num_samples;
        ++process_calls;
    }

    void Reset() override {
        ++reset_calls;
    }

    std::string GetName() const override {
        return "CountingProcessor";
    }

    int init_calls = 0;
    int process_calls = 0;
    int reset_calls = 0;
    bool init_should_succeed = true;
};

}  // namespace

class AudioProcessorTest : public ::testing::Test {
protected:
    static constexpr double kPi = 3.14159265358979323846;

    // Helper: a buffer of pure silence.
    std::vector<int16_t> Silence(size_t num_samples) {
        return std::vector<int16_t>(num_samples, 0);
    }

    // Helper: a constant-amplitude buffer (pure DC offset).
    std::vector<int16_t> Constant(size_t num_samples, int16_t value) {
        return std::vector<int16_t>(num_samples, value);
    }

    // Helper: a mono sine wave.
    std::vector<int16_t> SineWave(size_t num_samples, double frequency, int sample_rate,
                                  double amplitude) {
        std::vector<int16_t> samples(num_samples);
        for (size_t i = 0; i < num_samples; ++i) {
            samples[i] = static_cast<int16_t>(
                amplitude * std::sin(2.0 * kPi * frequency * static_cast<double>(i) / sample_rate));
        }
        return samples;
    }

    // Helper: root-mean-square level of a buffer.
    double Rms(const std::vector<int16_t>& samples) {
        if (samples.empty())
            return 0.0;
        double sum = 0.0;
        for (int16_t v : samples) {
            sum += static_cast<double>(v) * v;
        }
        return std::sqrt(sum / samples.size());
    }
};

// ============================================================================
// VolumeNormalizer
// ============================================================================

TEST_F(AudioProcessorTest, VolumeNormalizer_GetName) {
    VolumeNormalizer norm;
    EXPECT_EQ("VolumeNormalizer", norm.GetName());
}

TEST_F(AudioProcessorTest, VolumeNormalizer_Initialize) {
    VolumeNormalizer norm;
    EXPECT_TRUE(norm.Initialize(48000, 1));
    EXPECT_TRUE(norm.Initialize(44100, 2));
}

TEST_F(AudioProcessorTest, VolumeNormalizer_SilenceStaysSilent) {
    VolumeNormalizer norm;
    norm.Initialize(48000, 1);

    auto samples = Silence(4800);
    norm.Process(samples.data(), samples.size());

    EXPECT_DOUBLE_EQ(0.0, Rms(samples));
}

TEST_F(AudioProcessorTest, VolumeNormalizer_QuietSignalAmplified) {
    VolumeNormalizer norm;
    norm.Initialize(48000, 1);

    auto samples = Constant(48000, 800);  // quiet, 1 second
    double in_rms = Rms(samples);

    norm.Process(samples.data(), samples.size());

    // A quiet signal should be pushed up toward the target level.
    EXPECT_GT(Rms(samples), in_rms * 2.0);
}

TEST_F(AudioProcessorTest, VolumeNormalizer_LoudSignalAttenuated) {
    VolumeNormalizer norm;
    norm.Initialize(48000, 1);

    auto samples = Constant(48000, 28000);  // loud, 1 second
    double in_rms = Rms(samples);

    norm.Process(samples.data(), samples.size());

    // A loud signal should be pulled down to avoid clipping.
    EXPECT_LT(Rms(samples), in_rms * 0.85);
}

TEST_F(AudioProcessorTest, VolumeNormalizer_ZeroSamplesIsNoop) {
    VolumeNormalizer norm;
    norm.Initialize(48000, 1);

    std::vector<int16_t> empty;
    norm.Process(empty.data(), 0);  // must not crash on an empty buffer
    SUCCEED();
}

TEST_F(AudioProcessorTest, VolumeNormalizer_StereoDoesNotCrash) {
    VolumeNormalizer norm;
    norm.Initialize(48000, 2);

    auto samples = Constant(9600, 5000);  // 4800 interleaved stereo frames
    norm.Process(samples.data(), samples.size());
    SUCCEED();
}

TEST_F(AudioProcessorTest, VolumeNormalizer_ResetRestoresInitialGain) {
    VolumeNormalizer norm;
    norm.Initialize(48000, 1);

    auto run1 = Constant(48000, 25000);
    norm.Process(run1.data(), run1.size());  // gain ramps away from 1.0

    norm.Reset();

    auto run2 = Constant(48000, 25000);
    norm.Process(run2.data(), run2.size());

    // After Reset() the gain restarts at 1.0, so identical input -> identical output.
    EXPECT_EQ(run1, run2);
}

// ============================================================================
// HighPassFilter
// ============================================================================

TEST_F(AudioProcessorTest, HighPassFilter_GetName) {
    HighPassFilter hpf;
    EXPECT_EQ("HighPassFilter", hpf.GetName());
}

TEST_F(AudioProcessorTest, HighPassFilter_Initialize) {
    HighPassFilter hpf(80.0f);
    EXPECT_TRUE(hpf.Initialize(48000, 1));
    EXPECT_TRUE(hpf.Initialize(48000, 2));
}

TEST_F(AudioProcessorTest, HighPassFilter_RemovesDcOffset) {
    HighPassFilter hpf(80.0f);
    hpf.Initialize(48000, 1);

    auto samples = Constant(4800, 8000);  // pure DC
    double in_rms = Rms(samples);

    hpf.Process(samples.data(), samples.size());

    EXPECT_LT(Rms(samples), in_rms * 0.3);  // DC strongly attenuated
    EXPECT_LT(std::abs(static_cast<int>(samples.back())), 50);  // tail decayed to ~0
}

TEST_F(AudioProcessorTest, HighPassFilter_PassesHighFrequency) {
    HighPassFilter hpf(80.0f);
    hpf.Initialize(48000, 1);

    auto samples = SineWave(4800, 4000.0, 48000, 10000.0);
    double in_rms = Rms(samples);

    hpf.Process(samples.data(), samples.size());

    // A tone well above the 80 Hz cutoff should pass nearly unchanged.
    EXPECT_GT(Rms(samples), in_rms * 0.7);
}

TEST_F(AudioProcessorTest, HighPassFilter_AttenuatesLowMoreThanHigh) {
    HighPassFilter low_hpf(80.0f);
    low_hpf.Initialize(48000, 1);
    auto low = SineWave(4800, 30.0, 48000, 10000.0);  // below cutoff
    low_hpf.Process(low.data(), low.size());

    HighPassFilter high_hpf(80.0f);
    high_hpf.Initialize(48000, 1);
    auto high = SineWave(4800, 2000.0, 48000, 10000.0);  // above cutoff
    high_hpf.Process(high.data(), high.size());

    // Equal-amplitude inputs: the sub-cutoff tone must end up quieter.
    EXPECT_LT(Rms(low), Rms(high) * 0.7);
}

TEST_F(AudioProcessorTest, HighPassFilter_ZeroSamplesIsNoop) {
    HighPassFilter hpf(80.0f);
    hpf.Initialize(48000, 1);

    std::vector<int16_t> empty;
    hpf.Process(empty.data(), 0);  // must not crash on an empty buffer
    SUCCEED();
}

TEST_F(AudioProcessorTest, HighPassFilter_ResetClearsState) {
    HighPassFilter hpf(80.0f);
    hpf.Initialize(48000, 1);

    auto run1 = Constant(480, 8000);
    hpf.Process(run1.data(), run1.size());

    hpf.Reset();

    auto run2 = Constant(480, 8000);
    hpf.Process(run2.data(), run2.size());

    // Reset() clears the filter memory, so the second run reproduces the first.
    EXPECT_EQ(run1, run2);
}

TEST_F(AudioProcessorTest, HighPassFilter_StereoRemovesDcBothChannels) {
    HighPassFilter hpf(80.0f);
    hpf.Initialize(48000, 2);

    auto samples = Constant(9600, 8000);  // 4800 stereo frames of DC
    hpf.Process(samples.data(), samples.size());

    // Each channel keeps independent state; both tails must decay to ~0.
    EXPECT_LT(std::abs(static_cast<int>(samples[samples.size() - 2])), 50);  // last L
    EXPECT_LT(std::abs(static_cast<int>(samples[samples.size() - 1])), 50);  // last R
}

// ============================================================================
// AudioProcessorChain
// ============================================================================

TEST_F(AudioProcessorTest, Chain_GetName) {
    AudioProcessorChain chain;
    EXPECT_EQ("AudioProcessorChain", chain.GetName());
}

TEST_F(AudioProcessorTest, Chain_EmptyChainHasNoProcessors) {
    AudioProcessorChain chain;
    EXPECT_EQ(0u, chain.GetProcessorCount());
}

TEST_F(AudioProcessorTest, Chain_EmptyChainIsPassthrough) {
    AudioProcessorChain chain;
    chain.Initialize(48000, 1);

    auto samples = SineWave(480, 440.0, 48000, 10000.0);
    auto original = samples;

    chain.Process(samples.data(), samples.size());

    EXPECT_EQ(original, samples);  // an empty chain must not alter the audio
}

TEST_F(AudioProcessorTest, Chain_AddProcessorIncrementsCount) {
    AudioProcessorChain chain;

    chain.AddProcessor(std::make_unique<HighPassFilter>(80.0f));
    EXPECT_EQ(1u, chain.GetProcessorCount());

    chain.AddProcessor(std::make_unique<VolumeNormalizer>());
    EXPECT_EQ(2u, chain.GetProcessorCount());
}

TEST_F(AudioProcessorTest, Chain_AddNullProcessorIsIgnored) {
    AudioProcessorChain chain;
    chain.AddProcessor(nullptr);
    EXPECT_EQ(0u, chain.GetProcessorCount());
}

TEST_F(AudioProcessorTest, Chain_InitializeReturnsFalseWhenChildFails) {
    AudioProcessorChain chain;

    auto failing = std::make_unique<CountingProcessor>();
    failing->init_should_succeed = false;
    chain.AddProcessor(std::move(failing));

    EXPECT_FALSE(chain.Initialize(48000, 1));
}

TEST_F(AudioProcessorTest, Chain_ProcessRunsEveryProcessor) {
    AudioProcessorChain chain;

    auto p1 = std::make_unique<CountingProcessor>();
    auto p2 = std::make_unique<CountingProcessor>();
    CountingProcessor* p1_raw = p1.get();
    CountingProcessor* p2_raw = p2.get();
    chain.AddProcessor(std::move(p1));
    chain.AddProcessor(std::move(p2));

    ASSERT_TRUE(chain.Initialize(48000, 1));

    auto samples = Silence(480);
    chain.Process(samples.data(), samples.size());

    EXPECT_EQ(1, p1_raw->init_calls);
    EXPECT_EQ(1, p2_raw->init_calls);
    EXPECT_EQ(1, p1_raw->process_calls);
    EXPECT_EQ(1, p2_raw->process_calls);
}

TEST_F(AudioProcessorTest, Chain_ResetPropagatesToEveryProcessor) {
    AudioProcessorChain chain;

    auto p = std::make_unique<CountingProcessor>();
    CountingProcessor* p_raw = p.get();
    chain.AddProcessor(std::move(p));

    chain.Reset();

    EXPECT_EQ(1, p_raw->reset_calls);
}

TEST_F(AudioProcessorTest, Chain_AppliesProcessingEffect) {
    AudioProcessorChain chain;
    chain.AddProcessor(std::make_unique<HighPassFilter>(80.0f));
    ASSERT_TRUE(chain.Initialize(48000, 1));

    auto samples = Constant(4800, 8000);  // pure DC
    double in_rms = Rms(samples);

    chain.Process(samples.data(), samples.size());

    // The chained high-pass filter must actually run on the buffer.
    EXPECT_LT(Rms(samples), in_rms * 0.3);
}
