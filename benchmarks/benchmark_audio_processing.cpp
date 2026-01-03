/**
 * @file benchmark_audio_processing.cpp
 * @brief Performance benchmarks for audio processing components
 */

#include "audio/audio_processor.h"
#include "utils/signal_generator.h"

#ifdef ENABLE_RNNOISE
#include "audio/rnnoise_processor.h"
#endif

#include <benchmark/benchmark.h>
#include <vector>

using namespace ffvoice;

// =============================================================================
// VolumeNormalizer Benchmarks
// =============================================================================

static void BM_VolumeNormalizer_Process(benchmark::State& state) {
    const int sample_rate = 48000;
    const int channels = 1;
    const size_t num_samples = state.range(0);

    VolumeNormalizer normalizer(0.5f);
    normalizer.Initialize(sample_rate, channels);

    SignalGenerator generator;
    std::vector<int16_t> samples = generator.GenerateSineWave(440.0,
        static_cast<double>(num_samples) / sample_rate, sample_rate, 0.3);

    for (auto _ : state) {
        normalizer.Process(samples.data(), samples.size());
        benchmark::DoNotOptimize(samples.data());
        benchmark::ClobberMemory();
    }

    state.SetItemsProcessed(state.iterations() * num_samples);
    state.SetBytesProcessed(state.iterations() * num_samples * sizeof(int16_t));
}

BENCHMARK(BM_VolumeNormalizer_Process)
    ->Arg(256)      // Typical PortAudio buffer
    ->Arg(480)      // RNNoise frame size
    ->Arg(1024)
    ->Arg(4096)
    ->Unit(benchmark::kMicrosecond);

// =============================================================================
// HighPassFilter Benchmarks
// =============================================================================

static void BM_HighPassFilter_Process(benchmark::State& state) {
    const int sample_rate = 48000;
    const int channels = 1;
    const size_t num_samples = state.range(0);

    HighPassFilter filter(80.0f);
    filter.Initialize(sample_rate, channels);

    SignalGenerator generator;
    std::vector<int16_t> samples = generator.GenerateSineWave(440.0,
        static_cast<double>(num_samples) / sample_rate, sample_rate, 0.3);

    for (auto _ : state) {
        filter.Process(samples.data(), samples.size());
        benchmark::DoNotOptimize(samples.data());
        benchmark::ClobberMemory();
    }

    state.SetItemsProcessed(state.iterations() * num_samples);
    state.SetBytesProcessed(state.iterations() * num_samples * sizeof(int16_t));
}

BENCHMARK(BM_HighPassFilter_Process)
    ->Arg(256)
    ->Arg(480)
    ->Arg(1024)
    ->Arg(4096)
    ->Unit(benchmark::kMicrosecond);

// =============================================================================
// AudioProcessorChain Benchmarks
// =============================================================================

static void BM_ProcessorChain_MultipleProcessors(benchmark::State& state) {
    const int sample_rate = 48000;
    const int channels = 1;
    const size_t num_samples = state.range(0);

    AudioProcessorChain chain;
    chain.AddProcessor(std::make_unique<HighPassFilter>(80.0f));
    chain.AddProcessor(std::make_unique<VolumeNormalizer>(0.5f));
    chain.Initialize(sample_rate, channels);

    SignalGenerator generator;
    std::vector<int16_t> samples = generator.GenerateSineWave(440.0,
        static_cast<double>(num_samples) / sample_rate, sample_rate, 0.3);

    for (auto _ : state) {
        chain.Process(samples.data(), samples.size());
        benchmark::DoNotOptimize(samples.data());
        benchmark::ClobberMemory();
    }

    state.SetItemsProcessed(state.iterations() * num_samples);
    state.SetBytesProcessed(state.iterations() * num_samples * sizeof(int16_t));
}

BENCHMARK(BM_ProcessorChain_MultipleProcessors)
    ->Arg(256)
    ->Arg(480)
    ->Arg(1024)
    ->Unit(benchmark::kMicrosecond);

// =============================================================================
// RNNoise Benchmarks (if enabled)
// =============================================================================

#ifdef ENABLE_RNNOISE
static void BM_RNNoise_Process(benchmark::State& state) {
    const int sample_rate = 48000;
    const int channels = 1;
    const size_t num_samples = 480;  // RNNoise requires 480 samples

    RNNoiseConfig config;
    config.enable_vad = state.range(0) == 1;
    RNNoiseProcessor rnnoise(config);
    rnnoise.Initialize(sample_rate, channels);

    SignalGenerator generator;
    std::vector<int16_t> samples = generator.GenerateSineWave(440.0, 0.01, sample_rate, 0.3);
    auto noise = generator.GenerateWhiteNoise(samples.size(), sample_rate, 0.1);

    // Mix signal + noise
    for (size_t i = 0; i < samples.size(); ++i) {
        samples[i] = static_cast<int16_t>(
            std::clamp(static_cast<int32_t>(samples[i]) + noise[i],
                       static_cast<int32_t>(INT16_MIN),
                       static_cast<int32_t>(INT16_MAX)));
    }

    for (auto _ : state) {
        rnnoise.Process(samples.data(), num_samples);
        benchmark::DoNotOptimize(samples.data());
        benchmark::ClobberMemory();
    }

    state.SetItemsProcessed(state.iterations() * num_samples);
    state.SetBytesProcessed(state.iterations() * num_samples * sizeof(int16_t));
}

BENCHMARK(BM_RNNoise_Process)
    ->Arg(0)  // VAD disabled
    ->Arg(1)  // VAD enabled
    ->Unit(benchmark::kMicrosecond);
#endif

// =============================================================================
// Signal Generator Benchmarks
// =============================================================================

static void BM_SignalGenerator_SineWave(benchmark::State& state) {
    const int sample_rate = 48000;
    const size_t num_samples = state.range(0);
    SignalGenerator generator;

    for (auto _ : state) {
        auto samples = generator.GenerateSineWave(440.0,
            static_cast<double>(num_samples) / sample_rate, sample_rate, 0.5);
        benchmark::DoNotOptimize(samples.data());
    }

    state.SetItemsProcessed(state.iterations() * num_samples);
}

BENCHMARK(BM_SignalGenerator_SineWave)
    ->Arg(256)
    ->Arg(1024)
    ->Arg(4096)
    ->Arg(48000)  // 1 second
    ->Unit(benchmark::kMicrosecond);

static void BM_SignalGenerator_WhiteNoise(benchmark::State& state) {
    const int sample_rate = 48000;
    const size_t num_samples = state.range(0);
    SignalGenerator generator;

    for (auto _ : state) {
        auto samples = generator.GenerateWhiteNoise(num_samples, sample_rate, 0.5);
        benchmark::DoNotOptimize(samples.data());
    }

    state.SetItemsProcessed(state.iterations() * num_samples);
}

BENCHMARK(BM_SignalGenerator_WhiteNoise)
    ->Arg(256)
    ->Arg(1024)
    ->Arg(4096)
    ->Arg(48000)
    ->Unit(benchmark::kMicrosecond);
