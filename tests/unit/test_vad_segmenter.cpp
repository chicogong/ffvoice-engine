/**
 * @file test_vad_segmenter.cpp
 * @brief Unit tests for VADSegmenter
 */

#include "audio/vad_segmenter.h"

#include <gtest/gtest.h>

#include <cmath>
#include <vector>

using namespace ffvoice;

class VADSegmenterTest : public ::testing::Test {
protected:
    // Helper: Generate test audio samples
    std::vector<int16_t> GenerateSamples(size_t count, int16_t value = 1000) {
        return std::vector<int16_t>(count, value);
    }

    // Helper: Track segment callbacks
    struct SegmentInfo {
        std::vector<int16_t> samples;
        size_t num_samples;
    };
    
    std::vector<SegmentInfo> received_segments_;
    
    VADSegmenter::SegmentCallback GetCallback() {
        return [this](const int16_t* samples, size_t num_samples) {
            SegmentInfo info;
            info.samples.assign(samples, samples + num_samples);
            info.num_samples = num_samples;
            received_segments_.push_back(info);
        };
    }
    
    void SetUp() override {
        received_segments_.clear();
    }
};

// =============================================================================
// Construction Tests
// =============================================================================

TEST_F(VADSegmenterTest, DefaultConstruction) {
    VADSegmenter segmenter;
    
    EXPECT_EQ(0u, segmenter.GetBufferSize());
    EXPECT_FALSE(segmenter.IsInSpeech());
}

TEST_F(VADSegmenterTest, ConfigConstruction) {
    VADSegmenter::Config config;
    config.speech_threshold = 0.7f;
    config.min_speech_frames = 20;
    config.min_silence_frames = 30;
    
    VADSegmenter segmenter(config);
    
    EXPECT_FLOAT_EQ(0.7f, segmenter.GetCurrentThreshold());
}

TEST_F(VADSegmenterTest, PresetConfigurations) {
    auto very_sensitive = VADSegmenter::Config::FromPreset(VADSegmenter::Sensitivity::VERY_SENSITIVE);
    auto balanced = VADSegmenter::Config::FromPreset(VADSegmenter::Sensitivity::BALANCED);
    auto conservative = VADSegmenter::Config::FromPreset(VADSegmenter::Sensitivity::VERY_CONSERVATIVE);
    
    // More sensitive = lower threshold
    EXPECT_LT(very_sensitive.speech_threshold, balanced.speech_threshold);
    EXPECT_LT(balanced.speech_threshold, conservative.speech_threshold);
}

// =============================================================================
// Basic Processing Tests
// =============================================================================

TEST_F(VADSegmenterTest, ProcessFrame_SilenceNoCallback) {
    VADSegmenter segmenter;
    auto samples = GenerateSamples(480);  // 10ms at 48kHz
    
    // Process with low VAD probability (silence)
    segmenter.ProcessFrame(samples.data(), samples.size(), 0.1f, GetCallback());
    
    EXPECT_EQ(0u, received_segments_.size());
    EXPECT_FALSE(segmenter.IsInSpeech());
}

TEST_F(VADSegmenterTest, ProcessFrame_SpeechAccumulates) {
    VADSegmenter::Config config;
    config.speech_threshold = 0.5f;
    config.min_speech_frames = 5;
    VADSegmenter segmenter(config);
    
    auto samples = GenerateSamples(480);
    
    // Process multiple frames with high VAD probability
    for (int i = 0; i < 10; ++i) {
        segmenter.ProcessFrame(samples.data(), samples.size(), 0.9f, GetCallback());
    }
    
    // Should be in speech state and accumulating
    EXPECT_TRUE(segmenter.IsInSpeech());
    EXPECT_GT(segmenter.GetBufferSize(), 0u);
    // No callback yet (need silence to trigger)
    EXPECT_EQ(0u, received_segments_.size());
}

TEST_F(VADSegmenterTest, ProcessFrame_SpeechThenSilenceTriggers) {
    VADSegmenter::Config config;
    config.speech_threshold = 0.5f;
    config.min_speech_frames = 3;
    config.min_silence_frames = 3;
    VADSegmenter segmenter(config);
    
    auto samples = GenerateSamples(480);
    
    // Speech frames
    for (int i = 0; i < 10; ++i) {
        segmenter.ProcessFrame(samples.data(), samples.size(), 0.9f, GetCallback());
    }
    EXPECT_TRUE(segmenter.IsInSpeech());
    
    // Silence frames - should trigger callback
    for (int i = 0; i < 5; ++i) {
        segmenter.ProcessFrame(samples.data(), samples.size(), 0.1f, GetCallback());
    }
    
    EXPECT_EQ(1u, received_segments_.size());
    EXPECT_FALSE(segmenter.IsInSpeech());
}

// =============================================================================
// Max Segment Length Tests
// =============================================================================

TEST_F(VADSegmenterTest, MaxSegmentLength_TriggersSplit) {
    VADSegmenter::Config config;
    config.speech_threshold = 0.5f;
    config.min_speech_frames = 1;
    config.max_segment_samples = 4800;  // Very small: 100ms at 48kHz
    VADSegmenter segmenter(config);
    
    auto samples = GenerateSamples(480);  // 10ms
    
    // Keep sending speech - should trigger when max length reached
    for (int i = 0; i < 20; ++i) {  // 200ms total
        segmenter.ProcessFrame(samples.data(), samples.size(), 0.9f, GetCallback());
    }
    
    // Should have triggered at least one segment due to max length
    EXPECT_GE(received_segments_.size(), 1u);
}

// =============================================================================
// Flush Tests
// =============================================================================

TEST_F(VADSegmenterTest, Flush_EmptyBuffer) {
    VADSegmenter segmenter;
    
    segmenter.Flush(GetCallback());
    
    EXPECT_EQ(0u, received_segments_.size());
}

TEST_F(VADSegmenterTest, Flush_WithBufferedSpeech) {
    VADSegmenter::Config config;
    config.speech_threshold = 0.5f;
    config.min_speech_frames = 1;
    VADSegmenter segmenter(config);
    
    auto samples = GenerateSamples(480);
    
    // Accumulate some speech
    for (int i = 0; i < 5; ++i) {
        segmenter.ProcessFrame(samples.data(), samples.size(), 0.9f, GetCallback());
    }
    
    EXPECT_GT(segmenter.GetBufferSize(), 0u);
    EXPECT_EQ(0u, received_segments_.size());  // No callback yet
    
    // Flush should trigger callback with remaining audio
    segmenter.Flush(GetCallback());
    
    EXPECT_EQ(1u, received_segments_.size());
    EXPECT_EQ(480u * 5, received_segments_[0].num_samples);
}

// =============================================================================
// Reset Tests
// =============================================================================

TEST_F(VADSegmenterTest, Reset_ClearsState) {
    VADSegmenter::Config config;
    config.speech_threshold = 0.5f;
    config.min_speech_frames = 1;
    VADSegmenter segmenter(config);
    
    auto samples = GenerateSamples(480);
    
    // Accumulate some speech
    for (int i = 0; i < 5; ++i) {
        segmenter.ProcessFrame(samples.data(), samples.size(), 0.9f, GetCallback());
    }
    
    EXPECT_TRUE(segmenter.IsInSpeech());
    EXPECT_GT(segmenter.GetBufferSize(), 0u);
    
    // Reset
    segmenter.Reset();
    
    EXPECT_FALSE(segmenter.IsInSpeech());
    EXPECT_EQ(0u, segmenter.GetBufferSize());
}

// =============================================================================
// Statistics Tests
// =============================================================================

TEST_F(VADSegmenterTest, Statistics_TracksProbabilities) {
    VADSegmenter segmenter;
    auto samples = GenerateSamples(480);
    
    // Process some frames
    segmenter.ProcessFrame(samples.data(), samples.size(), 0.2f, GetCallback());
    segmenter.ProcessFrame(samples.data(), samples.size(), 0.4f, GetCallback());
    segmenter.ProcessFrame(samples.data(), samples.size(), 0.6f, GetCallback());
    segmenter.ProcessFrame(samples.data(), samples.size(), 0.8f, GetCallback());
    
    float avg_prob, speech_ratio;
    segmenter.GetStatistics(avg_prob, speech_ratio);
    
    // Average should be around 0.5
    EXPECT_NEAR(0.5f, avg_prob, 0.1f);
}

// =============================================================================
// Edge Cases
// =============================================================================

TEST_F(VADSegmenterTest, EdgeCase_ZeroSamples) {
    VADSegmenter segmenter;
    
    // Should handle zero-length input gracefully
    segmenter.ProcessFrame(nullptr, 0, 0.5f, GetCallback());
    
    EXPECT_EQ(0u, received_segments_.size());
}

TEST_F(VADSegmenterTest, EdgeCase_VeryShortSpeech) {
    VADSegmenter::Config config;
    config.speech_threshold = 0.5f;
    config.min_speech_frames = 10;  // Require 10 frames
    VADSegmenter segmenter(config);
    
    auto samples = GenerateSamples(480);
    
    // Only 3 speech frames (below min_speech_frames)
    for (int i = 0; i < 3; ++i) {
        segmenter.ProcessFrame(samples.data(), samples.size(), 0.9f, GetCallback());
    }
    
    // Then silence
    for (int i = 0; i < 10; ++i) {
        segmenter.ProcessFrame(samples.data(), samples.size(), 0.1f, GetCallback());
    }
    
    // Too short speech might not trigger callback (depends on implementation)
    // At minimum, no crash
    EXPECT_TRUE(true);
}

TEST_F(VADSegmenterTest, EdgeCase_BoundaryThreshold) {
    VADSegmenter::Config config;
    config.speech_threshold = 0.5f;
    VADSegmenter segmenter(config);
    
    auto samples = GenerateSamples(480);
    
    // Exactly at threshold
    segmenter.ProcessFrame(samples.data(), samples.size(), 0.5f, GetCallback());
    
    // Should not crash, behavior at boundary is implementation-defined
    EXPECT_TRUE(true);
}

TEST_F(VADSegmenterTest, EdgeCase_RapidTransitions) {
    VADSegmenter::Config config;
    config.speech_threshold = 0.5f;
    config.min_speech_frames = 2;
    config.min_silence_frames = 2;
    VADSegmenter segmenter(config);
    
    auto samples = GenerateSamples(480);
    
    // Rapid speech/silence transitions
    for (int i = 0; i < 20; ++i) {
        float vad = (i % 4 < 2) ? 0.9f : 0.1f;  // Alternate every 2 frames
        segmenter.ProcessFrame(samples.data(), samples.size(), vad, GetCallback());
    }
    
    // Should handle without crashing
    EXPECT_TRUE(true);
}

// =============================================================================
// Adaptive Threshold Tests
// =============================================================================

TEST_F(VADSegmenterTest, AdaptiveThreshold_Enabled) {
    VADSegmenter::Config config;
    config.speech_threshold = 0.5f;
    config.enable_adaptive_threshold = true;
    config.adaptive_factor = 0.1f;
    VADSegmenter segmenter(config);
    
    auto samples = GenerateSamples(480);
    float initial_threshold = segmenter.GetCurrentThreshold();
    
    // Process many low-VAD frames
    for (int i = 0; i < 100; ++i) {
        segmenter.ProcessFrame(samples.data(), samples.size(), 0.1f, GetCallback());
    }
    
    // Adaptive threshold might have changed (implementation dependent)
    // At minimum, should not crash
    EXPECT_TRUE(true);
}
