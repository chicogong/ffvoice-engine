/**
 * @file benchmark_audio_conversion.cpp
 * @brief Performance benchmarks for audio conversion and I/O
 */

#ifdef ENABLE_WHISPER

#include "utils/audio_converter.h"
#include "utils/signal_generator.h"
#include "media/wav_writer.h"

#include <benchmark/benchmark.h>
#include <vector>
#include <cstdio>

using namespace ffvoice;

// =============================================================================
// Audio Conversion Benchmarks
// =============================================================================

static void BM_AudioConverter_Int16ToFloat(benchmark::State& state) {
    const size_t num_samples = state.range(0);
    SignalGenerator generator;
    std::vector<int16_t> int_samples = generator.GenerateSineWave(440.0,
        static_cast<double>(num_samples) / 16000, 16000, 0.5);
    std::vector<float> float_samples(num_samples);

    for (auto _ : state) {
        AudioConverter::Int16ToFloat(int_samples.data(), num_samples, float_samples.data());
        benchmark::DoNotOptimize(float_samples.data());
    }

    state.SetItemsProcessed(state.iterations() * num_samples);
    state.SetBytesProcessed(state.iterations() * num_samples * sizeof(int16_t));
}

BENCHMARK(BM_AudioConverter_Int16ToFloat)
    ->Arg(480)
    ->Arg(1024)
    ->Arg(4096)
    ->Arg(16000)   // 1 second @ 16kHz
    ->Arg(48000)   // 1 second @ 48kHz
    ->Unit(benchmark::kMicrosecond);

static void BM_AudioConverter_StereoToMono(benchmark::State& state) {
    const size_t num_frames = state.range(0);
    const size_t num_samples = num_frames * 2;  // Stereo
    std::vector<float> stereo_samples(num_samples);
    std::vector<float> mono_samples(num_frames);

    // Fill with test data
    for (size_t i = 0; i < num_samples; i += 2) {
        stereo_samples[i] = static_cast<float>(i) / num_samples;      // Left
        stereo_samples[i + 1] = static_cast<float>(i + 1) / num_samples;  // Right
    }

    for (auto _ : state) {
        AudioConverter::StereoToMono(stereo_samples.data(), num_frames, mono_samples.data());
        benchmark::DoNotOptimize(mono_samples.data());
    }

    state.SetItemsProcessed(state.iterations() * num_frames);
    state.SetBytesProcessed(state.iterations() * num_samples * sizeof(float));
}

BENCHMARK(BM_AudioConverter_StereoToMono)
    ->Arg(480)
    ->Arg(1024)
    ->Arg(4096)
    ->Arg(16000)
    ->Arg(48000)
    ->Unit(benchmark::kMicrosecond);

static void BM_AudioConverter_Resample(benchmark::State& state) {
    const size_t input_size = state.range(0);
    const int in_sample_rate = 48000;
    const int out_sample_rate = 16000;
    const size_t output_size = (input_size * out_sample_rate) / in_sample_rate;

    std::vector<float> input_samples(input_size);
    std::vector<float> output_samples(output_size);

    for (size_t i = 0; i < input_size; ++i) {
        input_samples[i] = std::sin(2.0 * M_PI * 440.0 * i / in_sample_rate);
    }

    for (auto _ : state) {
        AudioConverter::Resample(input_samples.data(), input_size, in_sample_rate,
                                output_samples.data(), output_size, out_sample_rate);
        benchmark::DoNotOptimize(output_samples.data());
    }

    state.SetItemsProcessed(state.iterations() * input_size);
    state.SetBytesProcessed(state.iterations() * input_size * sizeof(float));
}

BENCHMARK(BM_AudioConverter_Resample)
    ->Arg(480)
    ->Arg(1024)
    ->Arg(4096)
    ->Arg(48000)   // 1 second @ 48kHz
    ->Unit(benchmark::kMicrosecond);

// =============================================================================
// WAV Writer Benchmarks
// =============================================================================

static void BM_WavWriter_WriteSamples(benchmark::State& state) {
    const int sample_rate = 48000;
    const int channels = 1;
    const size_t num_samples = state.range(0);
    const std::string test_file = "/tmp/benchmark_wav.wav";

    SignalGenerator generator;
    std::vector<int16_t> samples = generator.GenerateSineWave(440.0,
        static_cast<double>(num_samples) / sample_rate, sample_rate, 0.5);

    for (auto _ : state) {
        state.PauseTiming();
        WavWriter writer;
        writer.Open(test_file, sample_rate, channels, 16);
        state.ResumeTiming();

        writer.WriteSamples(samples);

        state.PauseTiming();
        writer.Close();
        std::remove(test_file.c_str());
        state.ResumeTiming();
    }

    state.SetItemsProcessed(state.iterations() * num_samples);
    state.SetBytesProcessed(state.iterations() * num_samples * sizeof(int16_t));
}

BENCHMARK(BM_WavWriter_WriteSamples)
    ->Arg(480)
    ->Arg(1024)
    ->Arg(4096)
    ->Arg(48000)
    ->Unit(benchmark::kMicrosecond);

// =============================================================================
// Combined Conversion Pipeline Benchmarks
// =============================================================================

static void BM_FullConversionPipeline(benchmark::State& state) {
    const size_t num_frames = state.range(0);
    const int in_sample_rate = 48000;
    const int out_sample_rate = 16000;

    // Generate stereo int16 samples
    std::vector<int16_t> stereo_int16(num_frames * 2);
    std::vector<float> float_samples(num_frames * 2);
    std::vector<float> mono_samples(num_frames);
    const size_t resampled_size = (num_frames * out_sample_rate) / in_sample_rate;
    std::vector<float> resampled(resampled_size);

    for (size_t i = 0; i < stereo_int16.size(); ++i) {
        stereo_int16[i] = static_cast<int16_t>(
            32767.0 * std::sin(2.0 * M_PI * 440.0 * (i / 2) / in_sample_rate));
    }

    for (auto _ : state) {
        // Step 1: int16 → float
        AudioConverter::Int16ToFloat(stereo_int16.data(), stereo_int16.size(), float_samples.data());

        // Step 2: stereo → mono
        AudioConverter::StereoToMono(float_samples.data(), num_frames, mono_samples.data());

        // Step 3: resample 48kHz → 16kHz
        AudioConverter::Resample(mono_samples.data(), num_frames, in_sample_rate,
                                resampled.data(), resampled_size, out_sample_rate);

        benchmark::DoNotOptimize(resampled.data());
    }

    state.SetItemsProcessed(state.iterations() * num_frames);
}

BENCHMARK(BM_FullConversionPipeline)
    ->Arg(480)
    ->Arg(1024)
    ->Arg(4096)
    ->Arg(48000)
    ->Unit(benchmark::kMicrosecond);

#endif  // ENABLE_WHISPER
