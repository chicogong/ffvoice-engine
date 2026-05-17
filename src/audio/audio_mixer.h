/**
 * @file audio_mixer.h
 * @brief Multi-track audio mixer
 */

#pragma once

#include <cstddef>
#include <cstdint>
#include <vector>

namespace ffvoice {

/**
 * @brief One track's input block for a single AudioMixer::MixBlock() call.
 */
struct MixerInput {
    int track_id;            ///< Track id returned by AudioMixer::AddTrack().
    const int16_t* samples;  ///< Interleaved int16 block; nullptr means silence.
};

/**
 * @brief Mixes multiple int16 audio tracks down to a single output buffer.
 *
 * Each track has an independent linear gain, stereo pan, and mute flag; a
 * master gain is applied to the summed result. Mixing accumulates in float and
 * clamps to int16, so combining loud tracks saturates cleanly instead of
 * wrapping around.
 *
 * The mixer keeps only track configuration as state — MixBlock() does not
 * retain audio between calls. It is not thread-safe: configure and mix from a
 * single thread, or synchronize externally.
 *
 * Usage:
 * @code
 * AudioMixer mixer;
 * mixer.Initialize(48000, 2);                  // stereo
 * int voice = mixer.AddTrack(1.0f, -0.3f);     // slightly left
 * int music = mixer.AddTrack(0.5f,  0.0f);     // quieter, centered
 *
 * std::vector<int16_t> out(block);
 * mixer.MixBlock({{voice, voice_block}, {music, music_block}}, out.data(), block);
 * @endcode
 */
class AudioMixer {
public:
    /// Sentinel returned by AddTrack() on failure; never a valid track id.
    static constexpr int kInvalidTrack = -1;

    /// Maximum linear gain accepted for a track or the master (~ +18 dB).
    static constexpr float kMaxGain = 8.0f;

    AudioMixer() = default;

    /**
     * @brief Initialize the mixer. Clears any existing tracks and master gain.
     * @param sample_rate Sample rate in Hz (must be > 0).
     * @param channels 1 (mono) or 2 (stereo).
     * @return true on success.
     */
    bool Initialize(int sample_rate, int channels);

    bool IsInitialized() const {
        return initialized_;
    }
    int GetSampleRate() const {
        return sample_rate_;
    }
    int GetChannels() const {
        return channels_;
    }

    /**
     * @brief Add a track.
     * @param gain Initial linear gain, clamped to [0, kMaxGain].
     * @param pan Initial stereo pan, clamped to [-1, 1] (ignored for mono).
     * @return New track id (>= 0), or kInvalidTrack if the mixer is not initialized.
     */
    int AddTrack(float gain = 1.0f, float pan = 0.0f);

    /// Remove a track. Returns true if the track existed.
    bool RemoveTrack(int track_id);

    /// True if a track with this id exists.
    bool HasTrack(int track_id) const;

    /// Number of tracks currently registered.
    size_t GetTrackCount() const {
        return tracks_.size();
    }

    /// Set a track's linear gain (clamped to [0, kMaxGain]). Returns false if missing.
    bool SetGain(int track_id, float gain);

    /// Set a track's stereo pan: -1 = hard left, 0 = center, +1 = hard right.
    bool SetPan(int track_id, float pan);

    /// Mute or unmute a track. Returns false if the track is missing.
    bool SetMute(int track_id, bool muted);

    /// Get a track's gain (0.0f if the track does not exist).
    float GetGain(int track_id) const;

    /// Get a track's pan (0.0f if the track does not exist).
    float GetPan(int track_id) const;

    /// Whether a track is muted (false if the track does not exist).
    bool IsMuted(int track_id) const;

    /// Set the master gain applied to the summed mix (clamped to [0, kMaxGain]).
    void SetMasterGain(float gain);

    float GetMasterGain() const {
        return master_gain_;
    }

    /**
     * @brief Mix one block of audio.
     *
     * Every block referenced by @p inputs must contain exactly @p num_samples
     * interleaved int16 samples (frames * channels). Tracks absent from
     * @p inputs, muted tracks, entries with a null pointer, and entries whose
     * id is unknown all contribute silence. @p output receives @p num_samples
     * mixed samples.
     *
     * @return false if the mixer is not initialized, @p num_samples is not a
     *         multiple of the channel count, or @p output is null.
     */
    bool MixBlock(const std::vector<MixerInput>& inputs, int16_t* output,
                  size_t num_samples) const;

    /// Remove all tracks and reset the master gain to 1.0 (stays initialized).
    void Reset();

private:
    struct Track {
        int id;
        float gain;
        float pan;
        bool muted;
    };

    const Track* FindTrack(int track_id) const;
    Track* FindTrack(int track_id);

    bool initialized_ = false;
    int sample_rate_ = 0;
    int channels_ = 0;
    int next_track_id_ = 0;
    float master_gain_ = 1.0f;
    std::vector<Track> tracks_;

    // Float scratch buffer reused across MixBlock() calls to avoid per-call
    // allocation on the audio path. mutable because MixBlock() is logically const.
    mutable std::vector<float> accumulator_;
};

}  // namespace ffvoice
