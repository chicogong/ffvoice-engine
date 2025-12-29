/**
 * @file whisper_processor.cpp
 * @brief Implementation of Whisper ASR processor
 */

#include "audio/whisper_processor.h"

#include "utils/audio_converter.h"
#include "utils/logger.h"

#ifdef ENABLE_WHISPER
    #include "whisper.h"
#endif

#include <algorithm>
#include <chrono>
#include <cstring>

namespace ffvoice {

std::string WhisperProcessor::GetModelTypeName(WhisperModelType type) {
    switch (type) {
        case WhisperModelType::TINY:
            return "tiny";
        case WhisperModelType::BASE:
            return "base";
        case WhisperModelType::SMALL:
            return "small";
        case WhisperModelType::MEDIUM:
            return "medium";
        case WhisperModelType::LARGE:
            return "large";
        default:
            return "unknown";
    }
}

WhisperProcessor::WhisperProcessor(const WhisperConfig& config)
    : config_(config)
#ifdef ENABLE_WHISPER
      ,
      ctx_(nullptr)
#endif
{
}

WhisperProcessor::~WhisperProcessor() {
#ifdef ENABLE_WHISPER
    if (ctx_) {
        whisper_free(ctx_);
        ctx_ = nullptr;
    }
#endif
}

bool WhisperProcessor::Initialize() {
#ifdef ENABLE_WHISPER
    if (ctx_) {
        LOG_WARNING("WhisperProcessor already initialized");
        return true;
    }

    LOG_INFO("Initializing Whisper processor...");
    LOG_INFO("  Model: %s", config_.model_path.c_str());
    LOG_INFO("  Language: %s", config_.language.c_str());
    LOG_INFO("  Threads: %d", config_.n_threads);

    // Load the model with default context params
    struct whisper_context_params cparams = whisper_context_default_params();
    ctx_ = whisper_init_from_file_with_params(config_.model_path.c_str(), cparams);
    if (!ctx_) {
        last_error_ = "Failed to load whisper model from: " + config_.model_path;
        LOG_ERROR("%s", last_error_.c_str());
        return false;
    }

    LOG_INFO("Whisper model loaded successfully");
    return true;
#else
    last_error_ = "Whisper support not enabled (rebuild with -DENABLE_WHISPER=ON)";
    LOG_ERROR("%s", last_error_.c_str());
    return false;
#endif
}

bool WhisperProcessor::IsInitialized() const {
#ifdef ENABLE_WHISPER
    return ctx_ != nullptr;
#else
    return false;
#endif
}

bool WhisperProcessor::TranscribeFile(const std::string& audio_file,
                                      std::vector<TranscriptionSegment>& segments) {
#ifdef ENABLE_WHISPER
    if (!IsInitialized()) {
        last_error_ = "WhisperProcessor not initialized";
        LOG_ERROR("%s", last_error_.c_str());
        return false;
    }

    LOG_INFO("Transcribing audio file: %s", audio_file.c_str());

    // Load and convert audio file to whisper format (16kHz, float, mono)
    std::vector<float> pcm_data;
    if (!LoadAudioFile(audio_file, pcm_data)) {
        last_error_ = "Failed to load audio file: " + audio_file;
        LOG_ERROR("%s", last_error_.c_str());
        return false;
    }

    LOG_INFO("Audio loaded: %zu samples (%.2f seconds)", pcm_data.size(),
             pcm_data.size() / 16000.0);

    // Get default parameters
    auto params = GetDefaultParams();

    // Run inference
    LOG_INFO("Running Whisper inference...");
    int result = whisper_full(ctx_, params, pcm_data.data(), pcm_data.size());

    if (result != 0) {
        last_error_ = "Whisper inference failed with code: " + std::to_string(result);
        LOG_ERROR("%s", last_error_.c_str());
        return false;
    }

    // Extract transcription segments
    ExtractSegments(segments);

    LOG_INFO("Transcription complete: %zu segments", segments.size());
    return true;
#else
    last_error_ = "Whisper support not enabled";
    LOG_ERROR("%s", last_error_.c_str());
    return false;
#endif
}

bool WhisperProcessor::TranscribeBuffer(const int16_t* samples, size_t num_samples,
                                        std::vector<TranscriptionSegment>& segments) {
#ifdef ENABLE_WHISPER
    if (!IsInitialized()) {
        last_error_ = "WhisperProcessor not initialized";
        LOG_ERROR("%s", last_error_.c_str());
        return false;
    }

    // Start performance timing
    auto start_time = std::chrono::high_resolution_clock::now();

    // Convert buffer to whisper format (16kHz, float, mono)
    std::vector<float> pcm_data;
    if (!ConvertBufferToFloat(samples, num_samples, pcm_data)) {
        last_error_ = "Failed to convert audio buffer";
        LOG_ERROR("%s", last_error_.c_str());
        return false;
    }

    auto convert_time = std::chrono::high_resolution_clock::now();

    // Get default parameters
    auto params = GetDefaultParams();

    // Run inference
    int result = whisper_full(ctx_, params, pcm_data.data(), pcm_data.size());

    auto inference_time = std::chrono::high_resolution_clock::now();

    if (result != 0) {
        last_error_ = "Whisper inference failed with code: " + std::to_string(result);
        LOG_ERROR("%s", last_error_.c_str());
        return false;
    }

    // Extract transcription segments
    ExtractSegments(segments);

    auto end_time = std::chrono::high_resolution_clock::now();

    // Calculate performance metrics
    if (config_.enable_performance_metrics) {
        auto total_ms =
            std::chrono::duration<double, std::milli>(end_time - start_time).count();
        auto convert_ms =
            std::chrono::duration<double, std::milli>(convert_time - start_time).count();
        auto inference_ms =
            std::chrono::duration<double, std::milli>(inference_time - convert_time).count();
        auto extract_ms =
            std::chrono::duration<double, std::milli>(end_time - inference_time).count();

        last_inference_time_ms_ = total_ms;

        double audio_duration_s = static_cast<double>(num_samples) / 48000.0;
        double realtime_factor = audio_duration_s * 1000.0 / total_ms;

        LOG_INFO("Performance: total=%.1fms (convert=%.1fms, inference=%.1fms, extract=%.1fms), "
                 "audio=%.2fs, RTF=%.2fx",
                 total_ms, convert_ms, inference_ms, extract_ms, audio_duration_s, realtime_factor);
    }

    return true;
#else
    last_error_ = "Whisper support not enabled";
    LOG_ERROR("%s", last_error_.c_str());
    return false;
#endif
}

#ifdef ENABLE_WHISPER

struct whisper_full_params WhisperProcessor::GetDefaultParams() {
    // Use greedy sampling strategy (fastest)
    struct whisper_full_params params = whisper_full_default_params(WHISPER_SAMPLING_GREEDY);

    // Set language
    if (config_.language != "auto") {
        params.language = config_.language.c_str();
    } else {
        params.language = nullptr;  // Auto-detect language
    }

    // Set number of threads
    params.n_threads = config_.n_threads;

    // Translation option
    params.translate = config_.translate;

    // Print options
    params.print_realtime = false;
    params.print_progress = config_.print_progress;
    params.print_timestamps = config_.print_timestamps;
    params.print_special = false;

    // Token options
    params.token_timestamps = true;  // Enable token-level timestamps
    params.suppress_blank = true;    // Suppress blank outputs
    params.suppress_non_speech_tokens = true;

    // Beam size (greedy = 1 beam)
    params.greedy.best_of = 1;

    return params;
}

bool WhisperProcessor::LoadAudioFile(const std::string& filename, std::vector<float>& pcm_data) {
    // Load and convert audio file to Whisper format (16kHz, float32, mono)
    if (!AudioConverter::LoadAndConvert(filename, pcm_data, 16000)) {
        last_error_ = "Failed to load and convert audio file: " + filename;
        LOG_ERROR("%s", last_error_.c_str());
        return false;
    }
    return true;
}

bool WhisperProcessor::ConvertBufferToFloat(const int16_t* samples, size_t num_samples,
                                            std::vector<float>& pcm_data) {
    // Real-time mode implementation (Milestone 4)
    // Currently assumes: 48kHz sample rate, mono channel
    // This assumption matches the standard recording configuration for real-time transcription

    // Convert int16_t 48kHz mono -> float 16kHz mono for Whisper
    // Reuse conversion_buffer_ to avoid repeated allocations
    if (conversion_buffer_.size() < num_samples) {
        conversion_buffer_.resize(num_samples);
    }
    AudioConverter::Int16ToFloat(samples, num_samples, conversion_buffer_.data());

    // Resample 48kHz -> 16kHz
    size_t output_size = static_cast<size_t>(num_samples * (16000.0 / 48000.0));
    pcm_data.resize(output_size);
    AudioConverter::Resample(conversion_buffer_.data(), num_samples, 48000, pcm_data.data(),
                             output_size, 16000);

    return true;
}

void WhisperProcessor::ExtractSegments(std::vector<TranscriptionSegment>& segments) {
    segments.clear();

    // Get number of segments
    const int n_segments = whisper_full_n_segments(ctx_);

    for (int i = 0; i < n_segments; ++i) {
        // Get segment text
        const char* text = whisper_full_get_segment_text(ctx_, i);

        // Get timestamps (in centiseconds, convert to milliseconds)
        const int64_t t0 = whisper_full_get_segment_t0(ctx_, i) * 10;  // centiseconds -> ms
        const int64_t t1 = whisper_full_get_segment_t1(ctx_, i) * 10;

        // Skip empty segments
        if (!text || std::strlen(text) == 0) {
            continue;
        }

        // Create segment
        TranscriptionSegment segment(t0, t1, text);

        // Add to results
        segments.push_back(segment);

        // Optional: Print segment
        if (config_.print_timestamps) {
            LOG_INFO("[%ld -> %ld] %s", static_cast<long>(t0), static_cast<long>(t1), text);
        }
    }
}

#endif  // ENABLE_WHISPER

}  // namespace ffvoice
