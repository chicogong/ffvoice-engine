/**
 * @file subtitle_generator.cpp
 * @brief Implementation of subtitle/transcript generator
 */

#include "utils/subtitle_generator.h"

#include "utils/logger.h"

#include <fstream>
#include <iomanip>
#include <sstream>

namespace ffvoice {

std::string SubtitleGenerator::GenerateString(const std::vector<TranscriptionSegment>& segments,
                                              Format format) {
    if (segments.empty()) {
        return {};
    }

    switch (format) {
        case Format::PlainText:
            return GeneratePlainText(segments);
        case Format::SRT:
            return GenerateSRT(segments);
        case Format::VTT:
            return GenerateVTT(segments);
        case Format::JSON:
            return GenerateJSON(segments);
        default:
            LOG_ERROR("Unknown subtitle format");
            return {};
    }
}

bool SubtitleGenerator::Generate(const std::vector<TranscriptionSegment>& segments,
                                 const std::string& output_file, Format format) {
    if (segments.empty()) {
        LOG_WARNING("No segments to generate subtitles from");
        return false;
    }

    // Generate formatted content based on format
    std::string content = GenerateString(segments, format);
    if (content.empty()) {
        // GenerateString already logged the error for unknown format
        return false;
    }

    // Write in binary mode so newlines stay '\n' on every platform — text mode
    // would translate '\n' -> '\r\n' on Windows, diverging from GenerateString().
    std::ofstream file(output_file, std::ios::binary);
    if (!file.is_open()) {
        LOG_ERROR("Failed to open output file: %s", output_file.c_str());
        return false;
    }

    file << content;
    file.close();

    LOG_INFO("Generated subtitle file: %s (%zu segments)", output_file.c_str(), segments.size());
    return true;
}

std::string SubtitleGenerator::FormatTimeSRT(int64_t ms) {
    // SRT format: HH:MM:SS,mmm
    int hours = ms / 3600000;
    int minutes = (ms % 3600000) / 60000;
    int seconds = (ms % 60000) / 1000;
    int milliseconds = ms % 1000;

    std::ostringstream oss;
    oss << std::setfill('0') << std::setw(2) << hours << ":" << std::setw(2) << minutes << ":"
        << std::setw(2) << seconds << "," << std::setw(3) << milliseconds;

    return oss.str();
}

std::string SubtitleGenerator::FormatTimeVTT(int64_t ms) {
    // VTT format: HH:MM:SS.mmm
    int hours = ms / 3600000;
    int minutes = (ms % 3600000) / 60000;
    int seconds = (ms % 60000) / 1000;
    int milliseconds = ms % 1000;

    std::ostringstream oss;
    oss << std::setfill('0') << std::setw(2) << hours << ":" << std::setw(2) << minutes << ":"
        << std::setw(2) << seconds << "." << std::setw(3) << milliseconds;

    return oss.str();
}

std::string
SubtitleGenerator::GeneratePlainText(const std::vector<TranscriptionSegment>& segments) {
    std::ostringstream oss;

    for (const auto& segment : segments) {
        oss << segment.text << "\n";
    }

    return oss.str();
}

std::string SubtitleGenerator::GenerateSRT(const std::vector<TranscriptionSegment>& segments) {
    std::ostringstream oss;

    for (size_t i = 0; i < segments.size(); ++i) {
        const auto& segment = segments[i];

        // Sequence number (1-indexed)
        oss << (i + 1) << "\n";

        // Timestamp: start --> end
        oss << FormatTimeSRT(segment.start_ms) << " --> " << FormatTimeSRT(segment.end_ms) << "\n";

        // Text content
        oss << segment.text << "\n";

        // Blank line separator (except after last segment)
        if (i < segments.size() - 1) {
            oss << "\n";
        }
    }

    return oss.str();
}

std::string SubtitleGenerator::GenerateVTT(const std::vector<TranscriptionSegment>& segments) {
    std::ostringstream oss;

    // VTT header
    oss << "WEBVTT\n\n";

    for (size_t i = 0; i < segments.size(); ++i) {
        const auto& segment = segments[i];

        // Timestamp: start --> end
        oss << FormatTimeVTT(segment.start_ms) << " --> " << FormatTimeVTT(segment.end_ms) << "\n";

        // Text content
        oss << segment.text << "\n";

        // Blank line separator (except after last segment)
        if (i < segments.size() - 1) {
            oss << "\n";
        }
    }

    return oss.str();
}

namespace {

/**
 * @brief Escape a string for safe embedding as a JSON string value
 *
 * Handles the characters that would otherwise produce invalid JSON:
 *   "  -> \"      backslash -> \\      newline -> \n
 *   \r -> \r      tab       -> \t
 * Other control characters (0x00-0x1F) are emitted as \u00XX escapes so the
 * output is always valid JSON regardless of what the ASR produced.
 *
 * @param input Raw string (e.g. transcribed text)
 * @return Escaped string (without surrounding quotes)
 */
std::string EscapeJSON(const std::string& input) {
    std::ostringstream oss;

    for (unsigned char c : input) {
        switch (c) {
            case '"':
                oss << "\\\"";
                break;
            case '\\':
                oss << "\\\\";
                break;
            case '\n':
                oss << "\\n";
                break;
            case '\r':
                oss << "\\r";
                break;
            case '\t':
                oss << "\\t";
                break;
            default:
                if (c < 0x20) {
                    // Other control characters -> \u00XX
                    oss << "\\u" << std::hex << std::setfill('0') << std::setw(4)
                        << static_cast<int>(c) << std::dec;
                } else {
                    oss << c;
                }
                break;
        }
    }

    return oss.str();
}

}  // namespace

std::string SubtitleGenerator::GenerateJSON(const std::vector<TranscriptionSegment>& segments) {
    std::ostringstream oss;

    // Use fixed-point notation with 2 decimals for confidence/probability floats.
    oss << std::fixed << std::setprecision(2);

    oss << "{\n";
    oss << "  \"segments\": [";
    if (!segments.empty()) {
        oss << "\n";
    }

    for (size_t i = 0; i < segments.size(); ++i) {
        const auto& segment = segments[i];

        oss << "    {\n";
        oss << "      \"start_ms\": " << segment.start_ms << ",\n";
        oss << "      \"end_ms\": " << segment.end_ms << ",\n";
        oss << "      \"confidence\": " << segment.confidence << ",\n";
        oss << "      \"text\": \"" << EscapeJSON(segment.text) << "\",\n";

        // Per-word timestamps (empty array when no word data is available).
        if (segment.words.empty()) {
            oss << "      \"words\": []\n";
        } else {
            oss << "      \"words\": [\n";
            for (size_t w = 0; w < segment.words.size(); ++w) {
                const auto& word = segment.words[w];

                oss << "        { \"start_ms\": " << word.start_ms
                    << ", \"end_ms\": " << word.end_ms << ", \"text\": \"" << EscapeJSON(word.text)
                    << "\", \"probability\": " << word.probability << " }";
                oss << (w < segment.words.size() - 1 ? ",\n" : "\n");
            }
            oss << "      ]\n";
        }

        oss << "    }";
        oss << (i < segments.size() - 1 ? ",\n" : "\n");
    }

    if (!segments.empty()) {
        oss << "  ";
    }
    oss << "]\n";
    oss << "}\n";

    return oss.str();
}

}  // namespace ffvoice
