/**
 * @file diarizer.h
 * @brief Speaker diarization wrapping the sherpa-onnx offline C API
 *
 * Diarizer answers "who spoke when": it splits an audio buffer into
 * per-speaker time segments (pyannote segmentation + 3D-Speaker embedding +
 * clustering).  MergeIntoSegments then stamps those speaker labels onto
 * Whisper TranscriptionSegments by largest temporal overlap.
 *
 * The Diarizer class is only available when ENABLE_DIARIZATION is defined.
 * SpeakerSegment and MergeIntoSegments are always compiled so callers and
 * tests can use them in any build configuration.
 */

#pragma once

#include "audio/whisper_processor.h"

#include <cstdint>
#include <functional>
#include <string>
#include <vector>

#ifdef ENABLE_DIARIZATION
extern "C" {
    #include "sherpa-onnx/c-api/c-api.h"
}
#endif

namespace ffvoice {

/**
 * @brief One diarization segment: a contiguous span attributed to one speaker.
 *
 * Unconditionally compiled so it can be used (and unit-tested) regardless of
 * whether ENABLE_DIARIZATION is set.
 */
struct SpeakerSegment {
    int64_t start_ms;    ///< Segment start time in milliseconds
    int64_t end_ms;      ///< Segment end time in milliseconds
    int32_t speaker_id;  ///< Speaker index, 0-based; -1 = unknown

    SpeakerSegment() : start_ms(0), end_ms(0), speaker_id(-1) {
    }

    SpeakerSegment(int64_t start, int64_t end, int32_t speaker)
        : start_ms(start), end_ms(end), speaker_id(speaker) {
    }
};

/**
 * @brief Assign a speaker_id to each TranscriptionSegment via temporal overlap.
 *
 * For every segment, the SpeakerSegment with the largest millisecond overlap
 * wins (ties resolved to the earlier SpeakerSegment).  Segments with no overlap
 * keep their existing speaker_id of -1.  An empty @p speakers list leaves every
 * segment untouched.
 *
 * @param segments Transcription segments to annotate (modified in place).
 * @param speakers Diarization segments produced by Diarizer::Diarize().
 */
void MergeIntoSegments(std::vector<TranscriptionSegment>& segments,
                       const std::vector<SpeakerSegment>& speakers);

#ifdef ENABLE_DIARIZATION

/**
 * @brief Configuration for the Diarizer.
 *
 * Model paths default to the compile-time paths baked in by CMake when
 * ENABLE_DIARIZATION is on; override them to use other models.
 */
struct DiarizerConfig {
    #ifdef DIARIZATION_SEGMENTATION_MODEL_PATH
    /// Pyannote speaker-segmentation model (.onnx).
    std::string segmentation_model_path = DIARIZATION_SEGMENTATION_MODEL_PATH;
    #else
    std::string segmentation_model_path = "";
    #endif

    #ifdef DIARIZATION_EMBEDDING_MODEL_PATH
    /// Speaker-embedding model (.onnx).
    std::string embedding_model_path = DIARIZATION_EMBEDDING_MODEL_PATH;
    #else
    std::string embedding_model_path = "";
    #endif

    /// Expected number of speakers; -1 = auto-detect via cluster_threshold.
    int num_speakers = -1;

    /// Clustering distance threshold (used only when num_speakers <= 0).
    float cluster_threshold = 0.5f;

    /// Inference threads for the segmentation and embedding models.
    int num_threads = 2;

    /**
     * @brief Test-seam for the diarization back-end.
     *
     * When set, Diarize() calls this function instead of sherpa-onnx, and
     * Init() becomes a no-op success requiring no model files.  Mirrors
     * LiveCaptionerConfig::transcribe_fn — enables unit tests with no models.
     */
    std::function<std::vector<SpeakerSegment>(const std::vector<float>&, int)> diarize_fn = nullptr;
};

/**
 * @brief Offline speaker diarization engine.
 *
 * Wraps the sherpa-onnx offline speaker-diarization C API with RAII over the
 * opaque C handle.  Usage:
 * @code
 * DiarizerConfig cfg;
 * Diarizer diarizer(cfg);
 * if (diarizer.Init()) {
 *     auto speakers = diarizer.Diarize(samples, 16000);
 *     MergeIntoSegments(transcription_segments, speakers);
 * }
 * @endcode
 */
class Diarizer {
public:
    /**
     * @brief Construct a Diarizer with the given configuration.
     * @param config Configuration parameters (copied internally).
     */
    explicit Diarizer(const DiarizerConfig& config = DiarizerConfig{});

    /**
     * @brief Destructor — releases the underlying sherpa-onnx handle.
     */
    ~Diarizer();

    // Non-copyable (owns an opaque C handle).
    Diarizer(const Diarizer&) = delete;
    Diarizer& operator=(const Diarizer&) = delete;

    /**
     * @brief Initialize the diarizer and load the models.
     *
     * Idempotent: a second call after success is a no-op returning true.
     * When config.diarize_fn is set, this is a no-op success and no model
     * files are required.
     *
     * @return true on success; false on failure (see GetLastError()).
     */
    bool Init();

    /**
     * @brief Run speaker diarization on a buffer of mono float samples.
     *
     * Returns an empty vector if the diarizer is not initialized or @p samples
     * is empty.  When config.diarize_fn is set, the seam is invoked instead of
     * sherpa-onnx.  Otherwise @p sample_rate must equal GetExpectedSampleRate().
     *
     * @param samples     Mono PCM samples normalized to [-1, 1].
     * @param sample_rate Sample rate of @p samples in Hz.
     * @return Speaker segments sorted by start time (empty on failure).
     */
    std::vector<SpeakerSegment> Diarize(const std::vector<float>& samples, int sample_rate);

    /**
     * @brief Query whether Init() has succeeded.
     * @return true once Init() has completed successfully.
     */
    bool IsInitialized() const;

    /**
     * @brief Return the last error message (empty string if no error).
     * @return Human-readable error description.
     */
    std::string GetLastError() const;

    /**
     * @brief Return the input sample rate the models expect (Hz).
     *
     * In test-seam mode this returns 16000 without consulting any model.
     *
     * @return Expected sample rate in Hz (typically 16000).
     */
    int GetExpectedSampleRate() const;

private:
    DiarizerConfig config_;     ///< Configuration (copy)
    std::string last_error_;    ///< Last error message
    bool initialized_ = false;  ///< True after Init() succeeds

    /// Opaque sherpa-onnx handle; nullptr until Init() (and in seam mode).
    const SherpaOnnxOfflineSpeakerDiarization* handle_ = nullptr;
};

#endif  // ENABLE_DIARIZATION

}  // namespace ffvoice
