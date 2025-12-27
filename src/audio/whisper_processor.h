/**
 * @file whisper_processor.h
 * @brief Whisper ASR (Automatic Speech Recognition) processor
 *
 * Provides offline and real-time speech-to-text transcription using whisper.cpp
 */

#pragma once

#include <cstdint>
#include <memory>
#include <string>
#include <vector>

#ifdef ENABLE_WHISPER
extern "C" {
// Forward declarations for whisper.cpp C API
struct whisper_context;
struct whisper_full_params;
}
#endif

namespace ffvoice {

/**
 * @brief Transcription segment with timestamp and text
 */
struct TranscriptionSegment {
    int64_t start_ms;  ///< Start time in milliseconds
    int64_t end_ms;    ///< End time in milliseconds
    std::string text;  ///< Transcribed text content
    float confidence;  ///< Confidence score (0.0-1.0)

    TranscriptionSegment() : start_ms(0), end_ms(0), confidence(0.0f) {
    }

    TranscriptionSegment(int64_t start, int64_t end, const std::string& txt, float conf = 0.0f)
        : start_ms(start), end_ms(end), text(txt), confidence(conf) {
    }
};

/**
 * @brief Whisper model size/type selection
 */
enum class WhisperModelType {
    TINY,    ///< Fastest, lowest accuracy (~39MB, ~10x realtime on CPU)
    BASE,    ///< Balanced speed/accuracy (~74MB, ~7x realtime on CPU)
    SMALL,   ///< Better accuracy (~244MB, ~3x realtime on CPU)
    MEDIUM,  ///< High accuracy (~769MB, ~1x realtime on CPU)
    LARGE    ///< Best accuracy (~1550MB, <1x realtime on CPU)
};

/**
 * @brief Configuration for Whisper processor
 */
struct WhisperConfig {
    std::string model_path = WHISPER_MODEL_PATH;  ///< Path to whisper model file
    std::string language = "auto";                ///< Language code ("auto", "zh", "en", etc.)
    WhisperModelType model_type = WhisperModelType::TINY;  ///< Model size selection
    int n_threads = 4;                            ///< Number of threads for inference
    bool translate = false;                       ///< Translate to English if true
    bool print_progress = true;                   ///< Print progress during processing
    bool print_timestamps = false;                ///< Print timestamps with text
    bool enable_performance_metrics = false;      ///< Enable performance timing metrics
};

/**
 * @brief Whisper ASR processor for speech-to-text transcription
 *
 * This class provides offline transcription of audio files using OpenAI's Whisper model.
 * Unlike AudioProcessor which is designed for real-time streaming processing,
 * WhisperProcessor operates on complete audio files or buffers for batch processing.
 *
 * Key differences from AudioProcessor:
 * - AudioProcessor: Real-time, in-place processing, <10ms latency
 * - WhisperProcessor: Offline/batch processing, seconds latency acceptable
 *
 * Usage example:
 * @code
 * WhisperConfig config;
 * config.language = "zh";  // Chinese
 * WhisperProcessor processor(config);
 *
 * if (processor.Initialize()) {
 *     std::vector<TranscriptionSegment> segments;
 *     processor.TranscribeFile("speech.wav", segments);
 *
 *     for (const auto& seg : segments) {
 *         std::cout << seg.text << std::endl;
 *     }
 * }
 * @endcode
 */
class WhisperProcessor {
public:
    /**
     * @brief Construct a new Whisper Processor
     * @param config Configuration parameters
     */
    explicit WhisperProcessor(const WhisperConfig& config = WhisperConfig{});

    /**
     * @brief Destructor - cleans up whisper context
     */
    ~WhisperProcessor();

    // Disable copy (whisper context is not copyable)
    WhisperProcessor(const WhisperProcessor&) = delete;
    WhisperProcessor& operator=(const WhisperProcessor&) = delete;

    /**
     * @brief Initialize the processor and load the whisper model
     * @return true if successful, false otherwise
     */
    bool Initialize();

    /**
     * @brief Transcribe an audio file (offline mode)
     * @param audio_file Path to audio file (WAV/FLAC)
     * @param segments Output vector of transcription segments
     * @return true if successful, false otherwise
     */
    bool TranscribeFile(const std::string& audio_file, std::vector<TranscriptionSegment>& segments);

    /**
     * @brief Transcribe audio buffer (for real-time mode - Phase 2)
     * @param samples Audio samples (int16_t format)
     * @param num_samples Number of samples (total, not per channel)
     * @param segments Output vector of transcription segments
     * @return true if successful, false otherwise
     */
    bool TranscribeBuffer(const int16_t* samples, size_t num_samples,
                          std::vector<TranscriptionSegment>& segments);

    /**
     * @brief Check if processor is initialized
     * @return true if initialized, false otherwise
     */
    bool IsInitialized() const;

    /**
     * @brief Get the last error message
     * @return Error message string
     */
    std::string GetLastError() const {
        return last_error_;
    }

    /**
     * @brief Get the last inference time in milliseconds
     * @return Inference time in ms (only valid if enable_performance_metrics is true)
     */
    double GetLastInferenceTimeMs() const {
        return last_inference_time_ms_;
    }

    /**
     * @brief Get the model type name string
     * @param type Model type enum
     * @return Model type name (e.g., "tiny", "base", "small")
     */
    static std::string GetModelTypeName(WhisperModelType type);

private:
    WhisperConfig config_;
    std::string last_error_;
    double last_inference_time_ms_ = 0.0;  ///< Last inference time in milliseconds

#ifdef ENABLE_WHISPER
    struct whisper_context* ctx_ = nullptr;

    // Reusable buffers to avoid repeated allocations
    mutable std::vector<float> conversion_buffer_;  ///< Reusable buffer for audio conversion
    mutable std::vector<float> resample_buffer_;    ///< Reusable buffer for resampling

    /**
     * @brief Get default whisper parameters
     * @return Default parameters for whisper_full()
     */
    struct whisper_full_params GetDefaultParams();

    /**
     * @brief Load and convert audio file to whisper format
     * @param filename Path to audio file
     * @param pcm_data Output PCM data (16kHz, float32, mono)
     * @return true if successful, false otherwise
     */
    bool LoadAudioFile(const std::string& filename, std::vector<float>& pcm_data);

    /**
     * @brief Convert audio buffer to whisper format
     * @param samples Input samples (int16_t, any sample rate)
     * @param num_samples Number of samples
     * @param pcm_data Output PCM data (16kHz, float32, mono)
     * @return true if successful, false otherwise
     */
    bool ConvertBufferToFloat(const int16_t* samples, size_t num_samples,
                              std::vector<float>& pcm_data);

    /**
     * @brief Extract transcription results from whisper context
     * @param segments Output vector of transcription segments
     */
    void ExtractSegments(std::vector<TranscriptionSegment>& segments);
#endif
};

}  // namespace ffvoice
