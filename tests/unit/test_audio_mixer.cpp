/**
 * @file test_audio_mixer.cpp
 * @brief Unit tests for AudioMixer (multi-track mixing)
 */

#include "audio/audio_mixer.h"

#include <gtest/gtest.h>

#include <cstdint>
#include <vector>

using namespace ffvoice;

class AudioMixerTest : public ::testing::Test {
protected:
    // Helper: a constant-valued interleaved block.
    std::vector<int16_t> Block(size_t num_samples, int16_t value) {
        return std::vector<int16_t>(num_samples, value);
    }
};

// ============================================================================
// Initialization
// ============================================================================

TEST_F(AudioMixerTest, NotInitializedByDefault) {
    AudioMixer mixer;
    EXPECT_FALSE(mixer.IsInitialized());
}

TEST_F(AudioMixerTest, Initialize_Mono) {
    AudioMixer mixer;
    EXPECT_TRUE(mixer.Initialize(48000, 1));
    EXPECT_TRUE(mixer.IsInitialized());
    EXPECT_EQ(48000, mixer.GetSampleRate());
    EXPECT_EQ(1, mixer.GetChannels());
}

TEST_F(AudioMixerTest, Initialize_Stereo) {
    AudioMixer mixer;
    EXPECT_TRUE(mixer.Initialize(44100, 2));
    EXPECT_EQ(2, mixer.GetChannels());
}

TEST_F(AudioMixerTest, Initialize_RejectsBadSampleRate) {
    AudioMixer mixer;
    EXPECT_FALSE(mixer.Initialize(0, 1));
    EXPECT_FALSE(mixer.Initialize(-48000, 1));
    EXPECT_FALSE(mixer.IsInitialized());
}

TEST_F(AudioMixerTest, Initialize_RejectsBadChannelCount) {
    AudioMixer mixer;
    EXPECT_FALSE(mixer.Initialize(48000, 0));
    EXPECT_FALSE(mixer.Initialize(48000, 3));
}

// ============================================================================
// Track management
// ============================================================================

TEST_F(AudioMixerTest, AddTrack_BeforeInitializeFails) {
    AudioMixer mixer;
    EXPECT_EQ(AudioMixer::kInvalidTrack, mixer.AddTrack());
}

TEST_F(AudioMixerTest, AddTrack_ReturnsDistinctIds) {
    AudioMixer mixer;
    mixer.Initialize(48000, 1);

    int a = mixer.AddTrack();
    int b = mixer.AddTrack();
    int c = mixer.AddTrack();

    EXPECT_GE(a, 0);
    EXPECT_NE(a, b);
    EXPECT_NE(b, c);
    EXPECT_NE(a, c);
    EXPECT_EQ(3u, mixer.GetTrackCount());
}

TEST_F(AudioMixerTest, AddTrack_StoresInitialGainAndPan) {
    AudioMixer mixer;
    mixer.Initialize(48000, 2);

    int t = mixer.AddTrack(0.5f, -0.25f);
    EXPECT_FLOAT_EQ(0.5f, mixer.GetGain(t));
    EXPECT_FLOAT_EQ(-0.25f, mixer.GetPan(t));
    EXPECT_FALSE(mixer.IsMuted(t));
}

TEST_F(AudioMixerTest, AddTrack_ClampsGainAndPan) {
    AudioMixer mixer;
    mixer.Initialize(48000, 2);

    int high = mixer.AddTrack(999.0f, 5.0f);
    EXPECT_FLOAT_EQ(AudioMixer::kMaxGain, mixer.GetGain(high));
    EXPECT_FLOAT_EQ(1.0f, mixer.GetPan(high));

    int low = mixer.AddTrack(-1.0f, -5.0f);
    EXPECT_FLOAT_EQ(0.0f, mixer.GetGain(low));
    EXPECT_FLOAT_EQ(-1.0f, mixer.GetPan(low));
}

TEST_F(AudioMixerTest, HasTrack_TrueForAddedFalseForUnknown) {
    AudioMixer mixer;
    mixer.Initialize(48000, 1);

    int t = mixer.AddTrack();
    EXPECT_TRUE(mixer.HasTrack(t));
    EXPECT_FALSE(mixer.HasTrack(t + 999));
}

TEST_F(AudioMixerTest, RemoveTrack_Works) {
    AudioMixer mixer;
    mixer.Initialize(48000, 1);

    int t = mixer.AddTrack();
    EXPECT_TRUE(mixer.RemoveTrack(t));
    EXPECT_FALSE(mixer.HasTrack(t));
    EXPECT_EQ(0u, mixer.GetTrackCount());

    EXPECT_FALSE(mixer.RemoveTrack(t));  // already gone
}

// ============================================================================
// Per-track controls
// ============================================================================

TEST_F(AudioMixerTest, SetGain_UpdatesAndClamps) {
    AudioMixer mixer;
    mixer.Initialize(48000, 1);
    int t = mixer.AddTrack();

    EXPECT_TRUE(mixer.SetGain(t, 2.0f));
    EXPECT_FLOAT_EQ(2.0f, mixer.GetGain(t));

    EXPECT_TRUE(mixer.SetGain(t, 999.0f));
    EXPECT_FLOAT_EQ(AudioMixer::kMaxGain, mixer.GetGain(t));

    EXPECT_TRUE(mixer.SetGain(t, -3.0f));
    EXPECT_FLOAT_EQ(0.0f, mixer.GetGain(t));
}

TEST_F(AudioMixerTest, SetPan_UpdatesAndClamps) {
    AudioMixer mixer;
    mixer.Initialize(48000, 2);
    int t = mixer.AddTrack();

    EXPECT_TRUE(mixer.SetPan(t, 0.5f));
    EXPECT_FLOAT_EQ(0.5f, mixer.GetPan(t));

    EXPECT_TRUE(mixer.SetPan(t, 9.0f));
    EXPECT_FLOAT_EQ(1.0f, mixer.GetPan(t));

    EXPECT_TRUE(mixer.SetPan(t, -9.0f));
    EXPECT_FLOAT_EQ(-1.0f, mixer.GetPan(t));
}

TEST_F(AudioMixerTest, SetMute_Works) {
    AudioMixer mixer;
    mixer.Initialize(48000, 1);
    int t = mixer.AddTrack();

    EXPECT_TRUE(mixer.SetMute(t, true));
    EXPECT_TRUE(mixer.IsMuted(t));
    EXPECT_TRUE(mixer.SetMute(t, false));
    EXPECT_FALSE(mixer.IsMuted(t));
}

TEST_F(AudioMixerTest, Controls_OnUnknownTrackFail) {
    AudioMixer mixer;
    mixer.Initialize(48000, 1);

    EXPECT_FALSE(mixer.SetGain(123, 1.0f));
    EXPECT_FALSE(mixer.SetPan(123, 0.0f));
    EXPECT_FALSE(mixer.SetMute(123, true));
    EXPECT_FLOAT_EQ(0.0f, mixer.GetGain(123));
    EXPECT_FLOAT_EQ(0.0f, mixer.GetPan(123));
    EXPECT_FALSE(mixer.IsMuted(123));
}

TEST_F(AudioMixerTest, MasterGain_DefaultsToOneAndClamps) {
    AudioMixer mixer;
    mixer.Initialize(48000, 1);
    EXPECT_FLOAT_EQ(1.0f, mixer.GetMasterGain());

    mixer.SetMasterGain(0.25f);
    EXPECT_FLOAT_EQ(0.25f, mixer.GetMasterGain());

    mixer.SetMasterGain(999.0f);
    EXPECT_FLOAT_EQ(AudioMixer::kMaxGain, mixer.GetMasterGain());

    mixer.SetMasterGain(-1.0f);
    EXPECT_FLOAT_EQ(0.0f, mixer.GetMasterGain());
}

// ============================================================================
// MixBlock — mixing behavior
// ============================================================================

TEST_F(AudioMixerTest, MixBlock_NoTracksProducesSilence) {
    AudioMixer mixer;
    mixer.Initialize(48000, 1);

    std::vector<int16_t> out(256, 123);
    ASSERT_TRUE(mixer.MixBlock({}, out.data(), out.size()));
    for (int16_t s : out) {
        EXPECT_EQ(0, s);
    }
}

TEST_F(AudioMixerTest, MixBlock_SingleTrackUnityGainPassesThrough) {
    AudioMixer mixer;
    mixer.Initialize(48000, 1);
    int t = mixer.AddTrack(1.0f, 0.0f);

    auto in = Block(256, 1000);
    std::vector<int16_t> out(256);
    ASSERT_TRUE(mixer.MixBlock({{t, in.data()}}, out.data(), out.size()));
    for (int16_t s : out) {
        EXPECT_EQ(1000, s);
    }
}

TEST_F(AudioMixerTest, MixBlock_GainScalesAmplitude) {
    AudioMixer mixer;
    mixer.Initialize(48000, 1);
    int t = mixer.AddTrack(0.5f, 0.0f);

    auto in = Block(128, 2000);
    std::vector<int16_t> out(128);
    ASSERT_TRUE(mixer.MixBlock({{t, in.data()}}, out.data(), out.size()));
    for (int16_t s : out) {
        EXPECT_EQ(1000, s);  // 2000 * 0.5
    }
}

TEST_F(AudioMixerTest, MixBlock_SumsMultipleTracks) {
    AudioMixer mixer;
    mixer.Initialize(48000, 1);
    int a = mixer.AddTrack();
    int b = mixer.AddTrack();

    auto block_a = Block(128, 1000);
    auto block_b = Block(128, 2500);
    std::vector<int16_t> out(128);
    ASSERT_TRUE(mixer.MixBlock({{a, block_a.data()}, {b, block_b.data()}}, out.data(), out.size()));
    for (int16_t s : out) {
        EXPECT_EQ(3500, s);  // 1000 + 2500
    }
}

TEST_F(AudioMixerTest, MixBlock_MutedTrackContributesSilence) {
    AudioMixer mixer;
    mixer.Initialize(48000, 1);
    int audible = mixer.AddTrack();
    int silent = mixer.AddTrack();
    mixer.SetMute(silent, true);

    auto block_audible = Block(64, 1000);
    auto block_silent = Block(64, 9000);
    std::vector<int16_t> out(64);
    ASSERT_TRUE(mixer.MixBlock({{audible, block_audible.data()}, {silent, block_silent.data()}},
                               out.data(), out.size()));
    for (int16_t s : out) {
        EXPECT_EQ(1000, s);  // muted track ignored
    }
}

TEST_F(AudioMixerTest, MixBlock_MasterGainApplied) {
    AudioMixer mixer;
    mixer.Initialize(48000, 1);
    int t = mixer.AddTrack(1.0f, 0.0f);
    mixer.SetMasterGain(0.5f);

    auto in = Block(64, 2000);
    std::vector<int16_t> out(64);
    ASSERT_TRUE(mixer.MixBlock({{t, in.data()}}, out.data(), out.size()));
    for (int16_t s : out) {
        EXPECT_EQ(1000, s);  // 2000 * masterGain 0.5
    }
}

TEST_F(AudioMixerTest, MixBlock_ClampsPositivePeakInsteadOfWrapping) {
    AudioMixer mixer;
    mixer.Initialize(48000, 1);
    int a = mixer.AddTrack();
    int b = mixer.AddTrack();

    auto loud = Block(64, 25000);  // 25000 + 25000 = 50000, beyond int16 max
    std::vector<int16_t> out(64);
    ASSERT_TRUE(mixer.MixBlock({{a, loud.data()}, {b, loud.data()}}, out.data(), out.size()));
    for (int16_t s : out) {
        EXPECT_EQ(32767, s);  // clamped, not wrapped to a negative value
    }
}

TEST_F(AudioMixerTest, MixBlock_ClampsNegativePeak) {
    AudioMixer mixer;
    mixer.Initialize(48000, 1);
    int a = mixer.AddTrack();
    int b = mixer.AddTrack();

    auto loud = Block(64, -25000);
    std::vector<int16_t> out(64);
    ASSERT_TRUE(mixer.MixBlock({{a, loud.data()}, {b, loud.data()}}, out.data(), out.size()));
    for (int16_t s : out) {
        EXPECT_EQ(-32768, s);
    }
}

TEST_F(AudioMixerTest, MixBlock_UnknownTrackIdIgnored) {
    AudioMixer mixer;
    mixer.Initialize(48000, 1);
    int t = mixer.AddTrack();

    auto good = Block(64, 1000);
    auto stray = Block(64, 9000);
    std::vector<int16_t> out(64);
    ASSERT_TRUE(
        mixer.MixBlock({{t, good.data()}, {t + 777, stray.data()}}, out.data(), out.size()));
    for (int16_t s : out) {
        EXPECT_EQ(1000, s);  // unknown id contributes nothing
    }
}

TEST_F(AudioMixerTest, MixBlock_NullSamplesIgnored) {
    AudioMixer mixer;
    mixer.Initialize(48000, 1);
    int a = mixer.AddTrack();
    int b = mixer.AddTrack();

    auto block_a = Block(64, 1500);
    std::vector<int16_t> out(64);
    ASSERT_TRUE(mixer.MixBlock({{a, block_a.data()}, {b, nullptr}}, out.data(), out.size()));
    for (int16_t s : out) {
        EXPECT_EQ(1500, s);  // null block treated as silence
    }
}

TEST_F(AudioMixerTest, MixBlock_TrackNotInInputsIsSilent) {
    AudioMixer mixer;
    mixer.Initialize(48000, 1);
    int a = mixer.AddTrack();
    mixer.AddTrack();  // second track exists but is not fed any input

    auto block_a = Block(64, 1000);
    std::vector<int16_t> out(64);
    ASSERT_TRUE(mixer.MixBlock({{a, block_a.data()}}, out.data(), out.size()));
    for (int16_t s : out) {
        EXPECT_EQ(1000, s);
    }
}

// ============================================================================
// MixBlock — stereo panning
// ============================================================================

TEST_F(AudioMixerTest, MixBlock_HardLeftPanSilencesRightChannel) {
    AudioMixer mixer;
    mixer.Initialize(48000, 2);
    int t = mixer.AddTrack(1.0f, -1.0f);  // hard left

    auto in = Block(128, 1000);  // 64 stereo frames
    std::vector<int16_t> out(128);
    ASSERT_TRUE(mixer.MixBlock({{t, in.data()}}, out.data(), out.size()));
    for (size_t i = 0; i < out.size(); ++i) {
        if (i % 2 == 0) {
            EXPECT_EQ(1000, out[i]) << "left sample " << i;
        } else {
            EXPECT_EQ(0, out[i]) << "right sample " << i;
        }
    }
}

TEST_F(AudioMixerTest, MixBlock_HardRightPanSilencesLeftChannel) {
    AudioMixer mixer;
    mixer.Initialize(48000, 2);
    int t = mixer.AddTrack(1.0f, 1.0f);  // hard right

    auto in = Block(128, 1000);
    std::vector<int16_t> out(128);
    ASSERT_TRUE(mixer.MixBlock({{t, in.data()}}, out.data(), out.size()));
    for (size_t i = 0; i < out.size(); ++i) {
        if (i % 2 == 0) {
            EXPECT_EQ(0, out[i]) << "left sample " << i;
        } else {
            EXPECT_EQ(1000, out[i]) << "right sample " << i;
        }
    }
}

TEST_F(AudioMixerTest, MixBlock_CenterPanKeepsBothChannels) {
    AudioMixer mixer;
    mixer.Initialize(48000, 2);
    int t = mixer.AddTrack(1.0f, 0.0f);  // centered

    auto in = Block(128, 1200);
    std::vector<int16_t> out(128);
    ASSERT_TRUE(mixer.MixBlock({{t, in.data()}}, out.data(), out.size()));
    for (int16_t s : out) {
        EXPECT_EQ(1200, s);  // both channels unchanged
    }
}

TEST_F(AudioMixerTest, MixBlock_PanIgnoredForMono) {
    AudioMixer mixer;
    mixer.Initialize(48000, 1);
    int t = mixer.AddTrack(1.0f, 0.0f);
    mixer.SetPan(t, -1.0f);  // hard left, but mono output ignores pan

    auto in = Block(64, 1000);
    std::vector<int16_t> out(64);
    ASSERT_TRUE(mixer.MixBlock({{t, in.data()}}, out.data(), out.size()));
    for (int16_t s : out) {
        EXPECT_EQ(1000, s);
    }
}

// ============================================================================
// MixBlock — argument validation
// ============================================================================

TEST_F(AudioMixerTest, MixBlock_BeforeInitializeFails) {
    AudioMixer mixer;
    std::vector<int16_t> out(64);
    EXPECT_FALSE(mixer.MixBlock({}, out.data(), out.size()));
}

TEST_F(AudioMixerTest, MixBlock_RejectsNonChannelMultiple) {
    AudioMixer mixer;
    mixer.Initialize(48000, 2);  // stereo

    std::vector<int16_t> out(7);
    EXPECT_FALSE(mixer.MixBlock({}, out.data(), 7));  // 7 not a multiple of 2
}

TEST_F(AudioMixerTest, MixBlock_NullOutputFails) {
    AudioMixer mixer;
    mixer.Initialize(48000, 1);
    EXPECT_FALSE(mixer.MixBlock({}, nullptr, 64));
}

TEST_F(AudioMixerTest, MixBlock_ZeroSamplesIsNoop) {
    AudioMixer mixer;
    mixer.Initialize(48000, 1);
    std::vector<int16_t> out(4, 7);
    EXPECT_TRUE(mixer.MixBlock({}, out.data(), 0));
}

// ============================================================================
// Reset
// ============================================================================

TEST_F(AudioMixerTest, Reset_ClearsTracksAndMasterGain) {
    AudioMixer mixer;
    mixer.Initialize(48000, 2);
    mixer.AddTrack();
    mixer.AddTrack();
    mixer.SetMasterGain(2.0f);

    mixer.Reset();

    EXPECT_EQ(0u, mixer.GetTrackCount());
    EXPECT_FLOAT_EQ(1.0f, mixer.GetMasterGain());
    EXPECT_TRUE(mixer.IsInitialized());  // Reset keeps the mixer initialized

    // Still usable after Reset.
    int t = mixer.AddTrack();
    EXPECT_TRUE(mixer.HasTrack(t));
}
