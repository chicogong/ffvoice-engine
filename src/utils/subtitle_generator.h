/**
 * @file subtitle_generator.h
 * @brief Subtitle/transcript generator for Whisper ASR output
 *
 * Converts transcription segments to various subtitle formats:
 * - Plain text (no timestamps)
 * - SRT (SubRip format)
 * - VTT (WebVTT format)
 */

#pragma once

#include "audio/whisper_processor.h"

#include <string>
#include <vector>

namespace ffvoice {

/**
 * @brief Subtitle and transcript generator
 *
 * Converts TranscriptionSegment vectors into various text/subtitle formats.
 *
 * Supported formats:
 * - PlainText: Simple text output with no timestamps
 * - SRT: SubRip format (standard subtitle format, .srt files)
 * - VTT: WebVTT format (web-based subtitle format, .vtt files)
 *
 * Usage example:
 * @code
 * std::vector<TranscriptionSegment> segments;
 * // ... populate segments from WhisperProcessor ...
 *
 * // Generate SRT subtitles
 * SubtitleGenerator::Generate(segments, "output.srt",
 *                            SubtitleGenerator::Format::SRT);
 *
 * // Generate plain text transcript
 * SubtitleGenerator::Generate(segments, "transcript.txt",
 *                            SubtitleGenerator::Format::PlainText);
 * @endcode
 */
class SubtitleGenerator {
public:
    /**
     * @brief Output format types
     */
    enum class Format {
        PlainText,  ///< Plain text without timestamps
        SRT,        ///< SubRip format (.srt)
        VTT         ///< WebVTT format (.vtt)
    };

    /**
     * @brief Generate subtitle/transcript file
     *
     * @param segments Transcription segments from Whisper
     * @param output_file Output file path
     * @param format Output format
     * @return true if successful, false otherwise
     */
    static bool Generate(const std::vector<TranscriptionSegment>& segments,
                         const std::string& output_file, Format format);

private:
    /**
     * @brief Format timestamp for SRT format (HH:MM:SS,mmm)
     * @param ms Timestamp in milliseconds
     * @return Formatted timestamp string
     */
    static std::string FormatTimeSRT(int64_t ms);

    /**
     * @brief Format timestamp for VTT format (HH:MM:SS.mmm)
     * @param ms Timestamp in milliseconds
     * @return Formatted timestamp string
     */
    static std::string FormatTimeVTT(int64_t ms);

    /**
     * @brief Generate plain text output
     * @param segments Transcription segments
     * @return Formatted text string
     */
    static std::string GeneratePlainText(const std::vector<TranscriptionSegment>& segments);

    /**
     * @brief Generate SRT format output
     * @param segments Transcription segments
     * @return Formatted SRT string
     */
    static std::string GenerateSRT(const std::vector<TranscriptionSegment>& segments);

    /**
     * @brief Generate VTT format output
     * @param segments Transcription segments
     * @return Formatted VTT string
     */
    static std::string GenerateVTT(const std::vector<TranscriptionSegment>& segments);
};

}  // namespace ffvoice
