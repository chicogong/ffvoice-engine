/**
 * @file test_diarizer.cpp
 * @brief Unit tests for the Diarizer class and MergeIntoSegments
 * @note MergeIntoSegments tests always run; Diarizer tests (via the
 *       diarize_fn seam) are only compiled when ENABLE_DIARIZATION is defined.
 */

#include "audio/diarizer.h"

#include <gtest/gtest.h>

#include <vector>

using namespace ffvoice;

// =============================================================================
// MergeIntoSegments tests — always compiled (no model files, no diarization)
// =============================================================================

class MergeIntoSegmentsTest : public ::testing::Test {
protected:
    /// Build a TranscriptionSegment with the given timing (text/confidence
    /// are irrelevant to merging; speaker_id defaults to -1).
    static TranscriptionSegment MakeSegment(int64_t start_ms, int64_t end_ms) {
        return TranscriptionSegment(start_ms, end_ms, "text", 1.0f);
    }
};

TEST_F(MergeIntoSegmentsTest, NoSpeakers_AllSegmentsStayUnknown) {
    std::vector<TranscriptionSegment> segments = {
        MakeSegment(0, 1000),
        MakeSegment(1000, 2000),
    };
    std::vector<SpeakerSegment> speakers;  // empty

    MergeIntoSegments(segments, speakers);

    for (const auto& seg : segments) {
        EXPECT_EQ(-1, seg.speaker_id);
    }
}

TEST_F(MergeIntoSegmentsTest, NoSegments_DoesNotCrash) {
    std::vector<TranscriptionSegment> segments;  // empty
    std::vector<SpeakerSegment> speakers = {
        SpeakerSegment(0, 1000, 0),
        SpeakerSegment(1000, 2000, 1),
    };

    MergeIntoSegments(segments, speakers);

    EXPECT_TRUE(segments.empty());
}

TEST_F(MergeIntoSegmentsTest, ExactOverlap_SpeakerAssigned) {
    std::vector<TranscriptionSegment> segments = {MakeSegment(0, 1000)};
    std::vector<SpeakerSegment> speakers = {SpeakerSegment(0, 1000, 3)};

    MergeIntoSegments(segments, speakers);

    EXPECT_EQ(3, segments[0].speaker_id);
}

TEST_F(MergeIntoSegmentsTest, PartialOverlap_LargerOverlapSpeakerWins) {
    // Segment [400, 1000]. Speaker 0 covers [0, 500] (overlap 100);
    // speaker 1 covers [500, 1200] (overlap 500) — speaker 1 must win.
    std::vector<TranscriptionSegment> segments = {MakeSegment(400, 1000)};
    std::vector<SpeakerSegment> speakers = {
        SpeakerSegment(0, 500, 0),
        SpeakerSegment(500, 1200, 1),
    };

    MergeIntoSegments(segments, speakers);

    EXPECT_EQ(1, segments[0].speaker_id);
}

TEST_F(MergeIntoSegmentsTest, NoOverlap_SegmentStaysUnknown) {
    // Segment [2000, 3000] does not overlap any speaker span.
    std::vector<TranscriptionSegment> segments = {MakeSegment(2000, 3000)};
    std::vector<SpeakerSegment> speakers = {
        SpeakerSegment(0, 500, 0),
        SpeakerSegment(500, 1000, 1),
    };

    MergeIntoSegments(segments, speakers);

    EXPECT_EQ(-1, segments[0].speaker_id);
}

TEST_F(MergeIntoSegmentsTest, TiedOverlap_EarlierSpeakerSegmentWins) {
    // Segment [0, 1000]. Speaker 0 covers [0, 500] (overlap 500);
    // speaker 1 covers [500, 1000] (overlap 500). On a tie the earlier
    // (lower-index) speaker segment, speaker 0, must win.
    std::vector<TranscriptionSegment> segments = {MakeSegment(0, 1000)};
    std::vector<SpeakerSegment> speakers = {
        SpeakerSegment(0, 500, 0),
        SpeakerSegment(500, 1000, 1),
    };

    MergeIntoSegments(segments, speakers);

    EXPECT_EQ(0, segments[0].speaker_id);
}

TEST_F(MergeIntoSegmentsTest, MultiSpeaker_ThreeSegmentsTwoSpeakers) {
    std::vector<TranscriptionSegment> segments = {
        MakeSegment(0, 1000),     // overlaps speaker 0
        MakeSegment(1000, 2000),  // overlaps speaker 1
        MakeSegment(2000, 3000),  // overlaps speaker 0 again
    };
    std::vector<SpeakerSegment> speakers = {
        SpeakerSegment(0, 1000, 0),
        SpeakerSegment(1000, 2000, 1),
        SpeakerSegment(2000, 3000, 0),
    };

    MergeIntoSegments(segments, speakers);

    EXPECT_EQ(0, segments[0].speaker_id);
    EXPECT_EQ(1, segments[1].speaker_id);
    EXPECT_EQ(0, segments[2].speaker_id);
}

TEST_F(MergeIntoSegmentsTest, SegmentInGapBetweenSpeakers_StaysUnknown) {
    // Speakers cover [0, 1000] and [2000, 3000]; the segment [1200, 1800]
    // sits entirely inside the gap and must remain unassigned.
    std::vector<TranscriptionSegment> segments = {MakeSegment(1200, 1800)};
    std::vector<SpeakerSegment> speakers = {
        SpeakerSegment(0, 1000, 0),
        SpeakerSegment(2000, 3000, 1),
    };

    MergeIntoSegments(segments, speakers);

    EXPECT_EQ(-1, segments[0].speaker_id);
}

TEST_F(MergeIntoSegmentsTest, PreSetSpeakerId_OverwrittenWhenOverlapFound) {
    // A segment that already carries a (stale) speaker_id must be overwritten
    // when an overlapping speaker segment is found.
    std::vector<TranscriptionSegment> segments = {MakeSegment(0, 1000)};
    segments[0].speaker_id = 7;  // stale value
    std::vector<SpeakerSegment> speakers = {SpeakerSegment(0, 1000, 2)};

    MergeIntoSegments(segments, speakers);

    EXPECT_EQ(2, segments[0].speaker_id);
}

#ifdef ENABLE_DIARIZATION

// =============================================================================
// Diarizer tests — only compiled when ENABLE_DIARIZATION is on.
// All tests use the diarize_fn seam so no model files are required.
// =============================================================================

class DiarizerTest : public ::testing::Test {
protected:
    /// Build a DiarizerConfig with a seam that returns @p canned segments and
    /// records the samples / sample_rate it was called with.
    DiarizerConfig MakeSeamConfig(std::vector<SpeakerSegment> canned) {
        DiarizerConfig cfg;
        cfg.diarize_fn = [this, canned](const std::vector<float>& samples,
                                        int sample_rate) -> std::vector<SpeakerSegment> {
            seam_called_ = true;
            seam_samples_ = samples;
            seam_sample_rate_ = sample_rate;
            return canned;
        };
        return cfg;
    }

    bool seam_called_ = false;
    std::vector<float> seam_samples_;
    int seam_sample_rate_ = 0;
};

TEST_F(DiarizerTest, Construction_DoesNotCrash) {
    Diarizer diarizer(MakeSeamConfig({}));
    EXPECT_FALSE(diarizer.IsInitialized());
    EXPECT_TRUE(diarizer.GetLastError().empty());
}

TEST_F(DiarizerTest, Init_WithSeam_Succeeds) {
    Diarizer diarizer(MakeSeamConfig({}));

    EXPECT_TRUE(diarizer.Init());
    EXPECT_TRUE(diarizer.IsInitialized());
    EXPECT_TRUE(diarizer.GetLastError().empty());
}

TEST_F(DiarizerTest, DoubleInit_IsIdempotent) {
    Diarizer diarizer(MakeSeamConfig({}));

    EXPECT_TRUE(diarizer.Init());
    EXPECT_TRUE(diarizer.Init());  // second call is a no-op
    EXPECT_TRUE(diarizer.IsInitialized());
}

TEST_F(DiarizerTest, DiarizeBeforeInit_ReturnsEmpty) {
    Diarizer diarizer(MakeSeamConfig({SpeakerSegment(0, 1000, 0)}));

    std::vector<float> samples(16000, 0.1f);
    auto result = diarizer.Diarize(samples, 16000);

    EXPECT_TRUE(result.empty());
    EXPECT_FALSE(seam_called_) << "Diarize before Init must not invoke the seam";
}

TEST_F(DiarizerTest, DiarizeWithEmptySamples_ReturnsEmpty) {
    Diarizer diarizer(MakeSeamConfig({SpeakerSegment(0, 1000, 0)}));
    ASSERT_TRUE(diarizer.Init());

    std::vector<float> empty;
    auto result = diarizer.Diarize(empty, 16000);

    EXPECT_TRUE(result.empty());
    EXPECT_FALSE(seam_called_) << "Empty samples must short-circuit before the seam";
}

TEST_F(DiarizerTest, Diarize_InvokesSeamAndReturnsItsResult) {
    std::vector<SpeakerSegment> canned = {
        SpeakerSegment(0, 1000, 0),
        SpeakerSegment(1000, 2000, 1),
    };
    Diarizer diarizer(MakeSeamConfig(canned));
    ASSERT_TRUE(diarizer.Init());

    std::vector<float> samples(16000, 0.2f);
    auto result = diarizer.Diarize(samples, 16000);

    EXPECT_TRUE(seam_called_);
    ASSERT_EQ(2u, result.size());
    EXPECT_EQ(0, result[0].speaker_id);
    EXPECT_EQ(0, result[0].start_ms);
    EXPECT_EQ(1000, result[0].end_ms);
    EXPECT_EQ(1, result[1].speaker_id);
    EXPECT_EQ(1000, result[1].start_ms);
    EXPECT_EQ(2000, result[1].end_ms);
}

TEST_F(DiarizerTest, Diarize_SeamReceivesExactSamplesAndSampleRate) {
    Diarizer diarizer(MakeSeamConfig({}));
    ASSERT_TRUE(diarizer.Init());

    std::vector<float> samples = {0.1f, -0.2f, 0.3f, -0.4f, 0.5f};
    diarizer.Diarize(samples, 16000);

    ASSERT_TRUE(seam_called_);
    EXPECT_EQ(16000, seam_sample_rate_);
    ASSERT_EQ(samples.size(), seam_samples_.size());
    for (size_t i = 0; i < samples.size(); ++i) {
        EXPECT_FLOAT_EQ(samples[i], seam_samples_[i]);
    }
}

TEST_F(DiarizerTest, Diarize_SeamReturningEmpty_IsHandled) {
    Diarizer diarizer(MakeSeamConfig({}));  // seam returns no segments
    ASSERT_TRUE(diarizer.Init());

    std::vector<float> samples(8000, 0.1f);
    auto result = diarizer.Diarize(samples, 16000);

    EXPECT_TRUE(seam_called_);
    EXPECT_TRUE(result.empty());
}

TEST_F(DiarizerTest, Init_NoSeamEmptyModelPaths_Fails) {
    DiarizerConfig cfg;  // no diarize_fn
    cfg.segmentation_model_path = "";
    cfg.embedding_model_path = "";
    Diarizer diarizer(cfg);

    EXPECT_FALSE(diarizer.Init());
    EXPECT_FALSE(diarizer.IsInitialized());
    EXPECT_FALSE(diarizer.GetLastError().empty());
}

TEST_F(DiarizerTest, GetExpectedSampleRate_SeamMode_Returns16000) {
    Diarizer diarizer(MakeSeamConfig({}));
    ASSERT_TRUE(diarizer.Init());

    EXPECT_EQ(16000, diarizer.GetExpectedSampleRate());
}

TEST_F(DiarizerTest, Integration_DiarizeThenMergeIntoSegments) {
    // Seam yields two speaker spans; merging them onto three transcription
    // segments must land the right speaker_id on each.
    std::vector<SpeakerSegment> canned = {
        SpeakerSegment(0, 1000, 0),
        SpeakerSegment(1000, 2000, 1),
    };
    Diarizer diarizer(MakeSeamConfig(canned));
    ASSERT_TRUE(diarizer.Init());

    std::vector<float> samples(32000, 0.15f);
    auto speakers = diarizer.Diarize(samples, 16000);
    ASSERT_EQ(2u, speakers.size());

    std::vector<TranscriptionSegment> segments = {
        TranscriptionSegment(0, 800, "hello", 1.0f),
        TranscriptionSegment(900, 1800, "world", 1.0f),
        TranscriptionSegment(2100, 2500, "gap", 1.0f),  // outside both spans
    };

    MergeIntoSegments(segments, speakers);

    EXPECT_EQ(0, segments[0].speaker_id);
    EXPECT_EQ(1, segments[1].speaker_id);
    EXPECT_EQ(-1, segments[2].speaker_id);
}

#endif  // ENABLE_DIARIZATION
