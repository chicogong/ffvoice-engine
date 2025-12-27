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

bool SubtitleGenerator::Generate(const std::vector<TranscriptionSegment>& segments,
                                 const std::string& output_file, Format format) {
    if (segments.empty()) {
        LOG_WARNING("No segments to generate subtitles from");
        return false;
    }

    // Generate formatted content based on format
    std::string content;
    switch (format) {
        case Format::PlainText:
            content = GeneratePlainText(segments);
            break;
        case Format::SRT:
            content = GenerateSRT(segments);
            break;
        case Format::VTT:
            content = GenerateVTT(segments);
            break;
        default:
            LOG_ERROR("Unknown subtitle format");
            return false;
    }

    // Write to file
    std::ofstream file(output_file);
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

}  // namespace ffvoice
