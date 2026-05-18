/**
 * @file audio_mixer.cpp
 * @brief Multi-track audio mixer implementation
 */

#include "audio/audio_mixer.h"

#include "utils/logger.h"

#include <algorithm>
#include <string>

namespace ffvoice {

namespace {
constexpr float kInt16Min = -32768.0f;
constexpr float kInt16Max = 32767.0f;
}  // namespace

bool AudioMixer::Initialize(int sample_rate, int channels) {
    if (sample_rate <= 0) {
        LOG_ERROR("AudioMixer: invalid sample rate %d", sample_rate);
        return false;
    }
    if (channels != 1 && channels != 2) {
        LOG_ERROR("AudioMixer: channels must be 1 or 2, got %d", channels);
        return false;
    }

    sample_rate_ = sample_rate;
    channels_ = channels;
    next_track_id_ = 0;
    master_gain_ = 1.0f;
    tracks_.clear();
    initialized_ = true;

    LOG_INFO("AudioMixer initialized: %dHz, %d channel(s)", sample_rate, channels);
    return true;
}

int AudioMixer::AddTrack(float gain, float pan) {
    if (!initialized_) {
        LOG_ERROR("AudioMixer::AddTrack called before Initialize()");
        return kInvalidTrack;
    }

    Track track;
    track.id = next_track_id_++;
    track.gain = std::clamp(gain, 0.0f, kMaxGain);
    track.pan = std::clamp(pan, -1.0f, 1.0f);
    track.muted = false;
    tracks_.push_back(track);
    return track.id;
}

bool AudioMixer::RemoveTrack(int track_id) {
    for (auto it = tracks_.begin(); it != tracks_.end(); ++it) {
        if (it->id == track_id) {
            tracks_.erase(it);
            return true;
        }
    }
    return false;
}

bool AudioMixer::HasTrack(int track_id) const {
    return FindTrack(track_id) != nullptr;
}

const AudioMixer::Track* AudioMixer::FindTrack(int track_id) const {
    for (const auto& track : tracks_) {
        if (track.id == track_id) {
            return &track;
        }
    }
    return nullptr;
}

AudioMixer::Track* AudioMixer::FindTrack(int track_id) {
    for (auto& track : tracks_) {
        if (track.id == track_id) {
            return &track;
        }
    }
    return nullptr;
}

bool AudioMixer::SetGain(int track_id, float gain) {
    Track* track = FindTrack(track_id);
    if (track == nullptr) {
        return false;
    }
    track->gain = std::clamp(gain, 0.0f, kMaxGain);
    return true;
}

bool AudioMixer::SetPan(int track_id, float pan) {
    Track* track = FindTrack(track_id);
    if (track == nullptr) {
        return false;
    }
    track->pan = std::clamp(pan, -1.0f, 1.0f);
    return true;
}

bool AudioMixer::SetMute(int track_id, bool muted) {
    Track* track = FindTrack(track_id);
    if (track == nullptr) {
        return false;
    }
    track->muted = muted;
    return true;
}

float AudioMixer::GetGain(int track_id) const {
    const Track* track = FindTrack(track_id);
    return track != nullptr ? track->gain : 0.0f;
}

float AudioMixer::GetPan(int track_id) const {
    const Track* track = FindTrack(track_id);
    return track != nullptr ? track->pan : 0.0f;
}

bool AudioMixer::IsMuted(int track_id) const {
    const Track* track = FindTrack(track_id);
    return track != nullptr ? track->muted : false;
}

void AudioMixer::SetMasterGain(float gain) {
    master_gain_ = std::clamp(gain, 0.0f, kMaxGain);
}

bool AudioMixer::MixBlock(const std::vector<MixerInput>& inputs, int16_t* output,
                          size_t num_samples) const {
    if (!initialized_) {
        return false;
    }
    if (num_samples % static_cast<size_t>(channels_) != 0) {
        return false;
    }
    if (num_samples == 0) {
        return true;  // nothing to mix
    }
    if (output == nullptr) {
        return false;
    }

    // Accumulate every track's contribution in float so that summing loud
    // tracks does not clip until the final conversion back to int16.
    if (accumulator_.size() < num_samples) {
        accumulator_.resize(num_samples);
    }
    std::fill(accumulator_.begin(), accumulator_.begin() + static_cast<std::ptrdiff_t>(num_samples),
              0.0f);

    const size_t channels = static_cast<size_t>(channels_);

    for (const auto& input : inputs) {
        if (input.samples == nullptr) {
            continue;
        }
        const Track* track = FindTrack(input.track_id);
        if (track == nullptr || track->muted) {
            continue;
        }

        // Per-channel gain: linear "balance" pan for stereo, plain gain for mono.
        float channel_gain[2] = {track->gain, track->gain};
        if (channels_ == 2) {
            const float pan = track->pan;
            channel_gain[0] = track->gain * (pan <= 0.0f ? 1.0f : 1.0f - pan);
            channel_gain[1] = track->gain * (pan >= 0.0f ? 1.0f : 1.0f + pan);
        }

        for (size_t i = 0; i < num_samples; ++i) {
            accumulator_[i] += static_cast<float>(input.samples[i]) * channel_gain[i % channels];
        }
    }

    // Apply master gain and clamp into the int16 output.
    for (size_t i = 0; i < num_samples; ++i) {
        const float value = std::clamp(accumulator_[i] * master_gain_, kInt16Min, kInt16Max);
        output[i] = static_cast<int16_t>(value);
    }
    return true;
}

void AudioMixer::Reset() {
    tracks_.clear();
    next_track_id_ = 0;
    master_gain_ = 1.0f;
}

}  // namespace ffvoice
