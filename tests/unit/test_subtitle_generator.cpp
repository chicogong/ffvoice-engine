/**
 * @file test_subtitle_generator.cpp
 * @brief Unit tests for SubtitleGenerator (transcript/subtitle output)
 * @note Only compiled when ENABLE_WHISPER is defined
 */

#ifdef ENABLE_WHISPER

    #include "utils/subtitle_generator.h"

    #include <gtest/gtest.h>

    #include <cstdint>
    #include <cstdio>
    #include <fstream>
    #include <sstream>
    #include <string>
    #include <vector>

using namespace ffvoice;

class SubtitleGeneratorTest : public ::testing::Test {
protected:
    void SetUp() override {
        // A unique-ish temp path per test, removed in TearDown().
        // testing::TempDir() is cross-platform (returns e.g. /tmp/ on POSIX,
        // %TEMP%\ on Windows) and is provided by GoogleTest.
        std::ostringstream path;
        path << ::testing::TempDir() << "ffvoice_subtitle_test_"
             << ::testing::UnitTest::GetInstance()->current_test_info()->name() << ".out";
        temp_path_ = path.str();
        std::remove(temp_path_.c_str());
    }

    void TearDown() override {
        std::remove(temp_path_.c_str());
    }

    // Read the whole temp file back into a string.
    std::string ReadFile() const {
        std::ifstream file(temp_path_, std::ios::binary);
        std::ostringstream oss;
        oss << file.rdbuf();
        return oss.str();
    }

    // True if 'haystack' contains 'needle'.
    static bool Contains(const std::string& haystack, const std::string& needle) {
        return haystack.find(needle) != std::string::npos;
    }

    // A representative set of segments: the first has per-word timestamps,
    // the second has an empty words vector.
    static std::vector<TranscriptionSegment> SampleSegments() {
        std::vector<TranscriptionSegment> segments;

        TranscriptionSegment seg0(0, 1500, "Hello world", 0.95f);
        seg0.words.emplace_back(0, 500, "Hello", 0.99f);
        seg0.words.emplace_back(500, 1500, " world", 0.88f);
        segments.push_back(seg0);

        // Second segment intentionally has no per-word data.
        segments.emplace_back(1500, 3200, "Second line", 0.72f);

        return segments;
    }

    std::string temp_path_;
};

// ============================================================================
// Generate — argument / edge handling
// ============================================================================

TEST_F(SubtitleGeneratorTest, Generate_EmptySegmentsReturnsFalse) {
    std::vector<TranscriptionSegment> empty;
    EXPECT_FALSE(SubtitleGenerator::Generate(empty, temp_path_, SubtitleGenerator::Format::JSON));
    EXPECT_FALSE(SubtitleGenerator::Generate(empty, temp_path_, SubtitleGenerator::Format::SRT));
    EXPECT_FALSE(SubtitleGenerator::Generate(empty, temp_path_, SubtitleGenerator::Format::VTT));
    EXPECT_FALSE(
        SubtitleGenerator::Generate(empty, temp_path_, SubtitleGenerator::Format::PlainText));
}

TEST_F(SubtitleGeneratorTest, Generate_ReturnsTrueAndWritesFile) {
    auto segments = SampleSegments();
    ASSERT_TRUE(SubtitleGenerator::Generate(segments, temp_path_, SubtitleGenerator::Format::JSON));

    std::ifstream file(temp_path_);
    EXPECT_TRUE(file.is_open());
}

TEST_F(SubtitleGeneratorTest, Generate_BadOutputPathReturnsFalse) {
    auto segments = SampleSegments();
    EXPECT_FALSE(SubtitleGenerator::Generate(segments, "/nonexistent_dir_ffvoice/output.json",
                                             SubtitleGenerator::Format::JSON));
}

// ============================================================================
// JSON format
// ============================================================================

TEST_F(SubtitleGeneratorTest, JSON_HasSegmentsArrayWrapper) {
    auto segments = SampleSegments();
    ASSERT_TRUE(SubtitleGenerator::Generate(segments, temp_path_, SubtitleGenerator::Format::JSON));

    const std::string out = ReadFile();
    EXPECT_TRUE(Contains(out, "\"segments\""));
    // Object braces present.
    EXPECT_FALSE(out.empty());
    EXPECT_EQ('{', out[out.find_first_not_of(" \t\r\n")]);
}

TEST_F(SubtitleGeneratorTest, JSON_ContainsSegmentTimestamps) {
    auto segments = SampleSegments();
    ASSERT_TRUE(SubtitleGenerator::Generate(segments, temp_path_, SubtitleGenerator::Format::JSON));

    const std::string out = ReadFile();
    // Segment 0: 0 -> 1500
    EXPECT_TRUE(Contains(out, "\"start_ms\": 0"));
    EXPECT_TRUE(Contains(out, "\"end_ms\": 1500"));
    // Segment 1: 1500 -> 3200
    EXPECT_TRUE(Contains(out, "\"start_ms\": 1500"));
    EXPECT_TRUE(Contains(out, "\"end_ms\": 3200"));
}

TEST_F(SubtitleGeneratorTest, JSON_ContainsSegmentText) {
    auto segments = SampleSegments();
    ASSERT_TRUE(SubtitleGenerator::Generate(segments, temp_path_, SubtitleGenerator::Format::JSON));

    const std::string out = ReadFile();
    EXPECT_TRUE(Contains(out, "\"text\": \"Hello world\""));
    EXPECT_TRUE(Contains(out, "\"text\": \"Second line\""));
}

TEST_F(SubtitleGeneratorTest, JSON_ContainsConfidenceFixedPrecision) {
    auto segments = SampleSegments();
    ASSERT_TRUE(SubtitleGenerator::Generate(segments, temp_path_, SubtitleGenerator::Format::JSON));

    const std::string out = ReadFile();
    // Confidence is emitted with 2-decimal fixed-point precision.
    EXPECT_TRUE(Contains(out, "\"confidence\": 0.95"));
    EXPECT_TRUE(Contains(out, "\"confidence\": 0.72"));
}

TEST_F(SubtitleGeneratorTest, JSON_PopulatedWordsArray) {
    auto segments = SampleSegments();
    ASSERT_TRUE(SubtitleGenerator::Generate(segments, temp_path_, SubtitleGenerator::Format::JSON));

    const std::string out = ReadFile();
    // Word entries from the first segment.
    EXPECT_TRUE(Contains(out, "\"words\""));
    EXPECT_TRUE(Contains(out, "\"text\": \"Hello\""));
    EXPECT_TRUE(Contains(out, "\"text\": \" world\""));  // leading space preserved
    // Per-word timestamps.
    EXPECT_TRUE(Contains(out, "\"start_ms\": 0"));
    EXPECT_TRUE(Contains(out, "\"end_ms\": 500"));
    // Per-word probability with fixed precision.
    EXPECT_TRUE(Contains(out, "\"probability\": 0.99"));
    EXPECT_TRUE(Contains(out, "\"probability\": 0.88"));
}

TEST_F(SubtitleGeneratorTest, JSON_EmptyWordsRenderedAsEmptyArray) {
    auto segments = SampleSegments();
    ASSERT_TRUE(SubtitleGenerator::Generate(segments, temp_path_, SubtitleGenerator::Format::JSON));

    const std::string out = ReadFile();
    // The second segment has no per-word data -> "words": []
    EXPECT_TRUE(Contains(out, "\"words\": []"));
}

TEST_F(SubtitleGeneratorTest, JSON_SingleSegmentNoWords) {
    std::vector<TranscriptionSegment> segments;
    segments.emplace_back(100, 900, "Only one", 0.50f);

    ASSERT_TRUE(SubtitleGenerator::Generate(segments, temp_path_, SubtitleGenerator::Format::JSON));

    const std::string out = ReadFile();
    EXPECT_TRUE(Contains(out, "\"text\": \"Only one\""));
    EXPECT_TRUE(Contains(out, "\"start_ms\": 100"));
    EXPECT_TRUE(Contains(out, "\"end_ms\": 900"));
    EXPECT_TRUE(Contains(out, "\"words\": []"));
}

TEST_F(SubtitleGeneratorTest, JSON_EscapesDoubleQuoteInText) {
    std::vector<TranscriptionSegment> segments;
    // Text contains a literal double-quote that must be escaped as \" in JSON.
    segments.emplace_back(0, 1000, "She said \"hi\" loudly", 0.80f);

    ASSERT_TRUE(SubtitleGenerator::Generate(segments, temp_path_, SubtitleGenerator::Format::JSON));

    const std::string out = ReadFile();
    // The escaped sequence \" must be present...
    EXPECT_TRUE(Contains(out, "She said \\\"hi\\\" loudly"));
    // ...and an unescaped «"hi"» must NOT appear.
    EXPECT_FALSE(Contains(out, "said \"hi\""));
}

TEST_F(SubtitleGeneratorTest, JSON_EscapesBackslashAndControlChars) {
    std::vector<TranscriptionSegment> segments;
    // Backslash, newline and tab all need escaping.
    segments.emplace_back(0, 1000, "path\\to\tfile\nnext", 0.60f);

    ASSERT_TRUE(SubtitleGenerator::Generate(segments, temp_path_, SubtitleGenerator::Format::JSON));

    const std::string out = ReadFile();
    EXPECT_TRUE(Contains(out, "path\\\\to"));  // literal backslash escaped as double backslash
    EXPECT_TRUE(Contains(out, "\\t"));         // tab escaped
    EXPECT_TRUE(Contains(out, "\\nnext"));     // newline escaped
}

TEST_F(SubtitleGeneratorTest, JSON_EscapesDoubleQuoteInWordText) {
    std::vector<TranscriptionSegment> segments;
    TranscriptionSegment seg(0, 1000, "quote test", 0.90f);
    seg.words.emplace_back(0, 500, "\"hi\"", 0.95f);
    segments.push_back(seg);

    ASSERT_TRUE(SubtitleGenerator::Generate(segments, temp_path_, SubtitleGenerator::Format::JSON));

    const std::string out = ReadFile();
    // Word text quote escaped inside the words array.
    EXPECT_TRUE(Contains(out, "\\\"hi\\\""));
}

// ============================================================================
// SRT / VTT / PlainText — sanity checks
// ============================================================================

TEST_F(SubtitleGeneratorTest, SRT_ContainsSequenceAndTimecodes) {
    auto segments = SampleSegments();
    ASSERT_TRUE(SubtitleGenerator::Generate(segments, temp_path_, SubtitleGenerator::Format::SRT));

    const std::string out = ReadFile();
    // SRT uses 1-indexed sequence numbers.
    EXPECT_TRUE(Contains(out, "1\n"));
    EXPECT_TRUE(Contains(out, "2\n"));
    // SRT timecodes use a comma before milliseconds: HH:MM:SS,mmm
    EXPECT_TRUE(Contains(out, "00:00:00,000 --> 00:00:01,500"));
    EXPECT_TRUE(Contains(out, "00:00:01,500 --> 00:00:03,200"));
    EXPECT_TRUE(Contains(out, "Hello world"));
    EXPECT_TRUE(Contains(out, "Second line"));
}

TEST_F(SubtitleGeneratorTest, VTT_HasHeaderAndDottedTimecodes) {
    auto segments = SampleSegments();
    ASSERT_TRUE(SubtitleGenerator::Generate(segments, temp_path_, SubtitleGenerator::Format::VTT));

    const std::string out = ReadFile();
    // VTT files must begin with the WEBVTT signature.
    EXPECT_EQ(0u, out.find("WEBVTT"));
    // VTT timecodes use a dot before milliseconds: HH:MM:SS.mmm
    EXPECT_TRUE(Contains(out, "00:00:00.000 --> 00:00:01.500"));
    EXPECT_TRUE(Contains(out, "Hello world"));
}

TEST_F(SubtitleGeneratorTest, PlainText_ContainsOnlyTextNoTimecodes) {
    auto segments = SampleSegments();
    ASSERT_TRUE(
        SubtitleGenerator::Generate(segments, temp_path_, SubtitleGenerator::Format::PlainText));

    const std::string out = ReadFile();
    EXPECT_TRUE(Contains(out, "Hello world"));
    EXPECT_TRUE(Contains(out, "Second line"));
    // No timecode arrows in plain text output.
    EXPECT_FALSE(Contains(out, "-->"));
}

// ============================================================================
// GenerateString — matches Generate() file output for each format
// ============================================================================

TEST_F(SubtitleGeneratorTest, GenerateString_EmptySegments_ReturnsEmptyString) {
    std::vector<TranscriptionSegment> empty;
    EXPECT_TRUE(
        SubtitleGenerator::GenerateString(empty, SubtitleGenerator::Format::PlainText).empty());
    EXPECT_TRUE(SubtitleGenerator::GenerateString(empty, SubtitleGenerator::Format::SRT).empty());
    EXPECT_TRUE(SubtitleGenerator::GenerateString(empty, SubtitleGenerator::Format::VTT).empty());
    EXPECT_TRUE(SubtitleGenerator::GenerateString(empty, SubtitleGenerator::Format::JSON).empty());
}

TEST_F(SubtitleGeneratorTest, GenerateString_PlainText_MatchesFile) {
    auto segments = SampleSegments();

    // Write via Generate() and read back
    ASSERT_TRUE(
        SubtitleGenerator::Generate(segments, temp_path_, SubtitleGenerator::Format::PlainText));
    const std::string file_content = ReadFile();

    // GenerateString should produce identical content
    const std::string str_content =
        SubtitleGenerator::GenerateString(segments, SubtitleGenerator::Format::PlainText);

    EXPECT_EQ(file_content, str_content);
    EXPECT_FALSE(str_content.empty());
}

TEST_F(SubtitleGeneratorTest, GenerateString_SRT_MatchesFile) {
    auto segments = SampleSegments();

    ASSERT_TRUE(SubtitleGenerator::Generate(segments, temp_path_, SubtitleGenerator::Format::SRT));
    const std::string file_content = ReadFile();

    const std::string str_content =
        SubtitleGenerator::GenerateString(segments, SubtitleGenerator::Format::SRT);

    EXPECT_EQ(file_content, str_content);
    EXPECT_FALSE(str_content.empty());
}

TEST_F(SubtitleGeneratorTest, GenerateString_VTT_MatchesFile) {
    auto segments = SampleSegments();

    ASSERT_TRUE(SubtitleGenerator::Generate(segments, temp_path_, SubtitleGenerator::Format::VTT));
    const std::string file_content = ReadFile();

    const std::string str_content =
        SubtitleGenerator::GenerateString(segments, SubtitleGenerator::Format::VTT);

    EXPECT_EQ(file_content, str_content);
    EXPECT_FALSE(str_content.empty());
}

TEST_F(SubtitleGeneratorTest, GenerateString_JSON_MatchesFile) {
    auto segments = SampleSegments();

    ASSERT_TRUE(SubtitleGenerator::Generate(segments, temp_path_, SubtitleGenerator::Format::JSON));
    const std::string file_content = ReadFile();

    const std::string str_content =
        SubtitleGenerator::GenerateString(segments, SubtitleGenerator::Format::JSON);

    EXPECT_EQ(file_content, str_content);
    EXPECT_FALSE(str_content.empty());
}

TEST_F(SubtitleGeneratorTest, GenerateString_SingleSegment_ContainsText) {
    std::vector<TranscriptionSegment> segments;
    segments.emplace_back(0, 1000, "Hello from GenerateString", 0.90f);

    const std::string result =
        SubtitleGenerator::GenerateString(segments, SubtitleGenerator::Format::PlainText);

    EXPECT_TRUE(Contains(result, "Hello from GenerateString"));
}

#endif  // ENABLE_WHISPER
