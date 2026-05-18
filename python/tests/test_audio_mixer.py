"""
Tests for the ffvoice AudioMixer Python bindings
"""

import pytest


def test_construction():
    """A freshly constructed mixer is not initialized."""
    try:
        from ffvoice import AudioMixer

        mixer = AudioMixer()
        assert mixer.is_initialized() is False
    except ImportError as e:
        pytest.skip(f"Module not built yet: {e}")


def test_initialize_mono():
    """initialize() succeeds for a mono mixer."""
    try:
        from ffvoice import AudioMixer

        mixer = AudioMixer()
        assert mixer.initialize(48000, 1) is True
        assert mixer.is_initialized() is True
        assert mixer.get_sample_rate() == 48000
        assert mixer.get_channels() == 1
    except ImportError as e:
        pytest.skip(f"Module not built yet: {e}")


def test_initialize_stereo():
    """initialize() succeeds for a stereo mixer."""
    try:
        from ffvoice import AudioMixer

        mixer = AudioMixer()
        assert mixer.initialize(16000, 2) is True
        assert mixer.is_initialized() is True
        assert mixer.get_sample_rate() == 16000
        assert mixer.get_channels() == 2
    except ImportError as e:
        pytest.skip(f"Module not built yet: {e}")


def test_initialize_rejects_bad_channels():
    """initialize() returns False for channel counts other than 1 or 2."""
    try:
        from ffvoice import AudioMixer

        for bad_channels in (0, 3, 4, -1):
            mixer = AudioMixer()
            assert mixer.initialize(48000, bad_channels) is False
            assert mixer.is_initialized() is False
    except ImportError as e:
        pytest.skip(f"Module not built yet: {e}")


def test_add_track_before_init_returns_minus_one():
    """add_track() returns -1 when the mixer is not initialized."""
    try:
        from ffvoice import AudioMixer

        mixer = AudioMixer()
        assert mixer.add_track() == -1
        assert mixer.get_track_count() == 0
    except ImportError as e:
        pytest.skip(f"Module not built yet: {e}")


def test_add_track_returns_distinct_ids():
    """add_track() returns distinct non-negative ids after init."""
    try:
        from ffvoice import AudioMixer

        mixer = AudioMixer()
        mixer.initialize(48000, 1)

        id1 = mixer.add_track()
        id2 = mixer.add_track()
        id3 = mixer.add_track()

        assert id1 >= 0
        assert id2 >= 0
        assert id3 >= 0
        assert len({id1, id2, id3}) == 3
    except ImportError as e:
        pytest.skip(f"Module not built yet: {e}")


def test_track_count():
    """get_track_count() reflects the number of tracks added."""
    try:
        from ffvoice import AudioMixer

        mixer = AudioMixer()
        mixer.initialize(48000, 1)
        assert mixer.get_track_count() == 0

        mixer.add_track()
        assert mixer.get_track_count() == 1

        mixer.add_track()
        mixer.add_track()
        assert mixer.get_track_count() == 3
    except ImportError as e:
        pytest.skip(f"Module not built yet: {e}")


def test_has_track():
    """has_track() reports whether a track id exists."""
    try:
        from ffvoice import AudioMixer

        mixer = AudioMixer()
        mixer.initialize(48000, 1)

        track_id = mixer.add_track()
        assert mixer.has_track(track_id) is True
        assert mixer.has_track(track_id + 999) is False
    except ImportError as e:
        pytest.skip(f"Module not built yet: {e}")


def test_remove_track():
    """remove_track() removes an existing track and reports success."""
    try:
        from ffvoice import AudioMixer

        mixer = AudioMixer()
        mixer.initialize(48000, 1)

        track_id = mixer.add_track()
        assert mixer.get_track_count() == 1

        assert mixer.remove_track(track_id) is True
        assert mixer.get_track_count() == 0
        assert mixer.has_track(track_id) is False

        # Removing again (or a missing id) returns False.
        assert mixer.remove_track(track_id) is False
    except ImportError as e:
        pytest.skip(f"Module not built yet: {e}")


def test_gain_set_and_get():
    """set_gain()/get_gain() round-trip an in-range value."""
    try:
        from ffvoice import AudioMixer

        mixer = AudioMixer()
        mixer.initialize(48000, 1)
        track_id = mixer.add_track()

        assert mixer.set_gain(track_id, 2.5) is True
        assert mixer.get_gain(track_id) == pytest.approx(2.5)

        # set_gain on a missing track returns False.
        assert mixer.set_gain(track_id + 999, 1.0) is False
    except ImportError as e:
        pytest.skip(f"Module not built yet: {e}")


def test_pan_set_and_get():
    """set_pan()/get_pan() round-trip an in-range value."""
    try:
        from ffvoice import AudioMixer

        mixer = AudioMixer()
        mixer.initialize(48000, 2)
        track_id = mixer.add_track()

        assert mixer.set_pan(track_id, -0.5) is True
        assert mixer.get_pan(track_id) == pytest.approx(-0.5)

        assert mixer.set_pan(track_id, 0.75) is True
        assert mixer.get_pan(track_id) == pytest.approx(0.75)

        # set_pan on a missing track returns False.
        assert mixer.set_pan(track_id + 999, 0.0) is False
    except ImportError as e:
        pytest.skip(f"Module not built yet: {e}")


def test_mute_set_and_get():
    """set_mute()/is_muted() round-trip the mute flag."""
    try:
        from ffvoice import AudioMixer

        mixer = AudioMixer()
        mixer.initialize(48000, 1)
        track_id = mixer.add_track()

        # Tracks start unmuted.
        assert mixer.is_muted(track_id) is False

        assert mixer.set_mute(track_id, True) is True
        assert mixer.is_muted(track_id) is True

        assert mixer.set_mute(track_id, False) is True
        assert mixer.is_muted(track_id) is False

        # set_mute on a missing track returns False.
        assert mixer.set_mute(track_id + 999, True) is False
    except ImportError as e:
        pytest.skip(f"Module not built yet: {e}")


def test_gain_clamped_to_range():
    """Track gain is clamped to [0.0, 8.0]."""
    try:
        from ffvoice import AudioMixer

        mixer = AudioMixer()
        mixer.initialize(48000, 1)
        track_id = mixer.add_track()

        # Above the maximum clamps to 8.0.
        mixer.set_gain(track_id, 100.0)
        assert mixer.get_gain(track_id) == pytest.approx(8.0)

        # Below the minimum clamps to 0.0.
        mixer.set_gain(track_id, -5.0)
        assert mixer.get_gain(track_id) == pytest.approx(0.0)

        # add_track() also clamps its initial gain.
        clamped_track = mixer.add_track(gain=50.0)
        assert mixer.get_gain(clamped_track) == pytest.approx(8.0)
    except ImportError as e:
        pytest.skip(f"Module not built yet: {e}")


def test_pan_clamped_to_range():
    """Track pan is clamped to [-1.0, 1.0]."""
    try:
        from ffvoice import AudioMixer

        mixer = AudioMixer()
        mixer.initialize(48000, 2)
        track_id = mixer.add_track()

        # Above the maximum clamps to 1.0.
        mixer.set_pan(track_id, 5.0)
        assert mixer.get_pan(track_id) == pytest.approx(1.0)

        # Below the minimum clamps to -1.0.
        mixer.set_pan(track_id, -5.0)
        assert mixer.get_pan(track_id) == pytest.approx(-1.0)

        # add_track() also clamps its initial pan.
        clamped_track = mixer.add_track(pan=-3.0)
        assert mixer.get_pan(clamped_track) == pytest.approx(-1.0)
    except ImportError as e:
        pytest.skip(f"Module not built yet: {e}")


def test_master_gain_default_and_set():
    """Master gain defaults to 1.0 and is settable and clamped."""
    try:
        from ffvoice import AudioMixer

        mixer = AudioMixer()
        mixer.initialize(48000, 1)

        # Default master gain is 1.0.
        assert mixer.get_master_gain() == pytest.approx(1.0)

        # In-range value round-trips.
        mixer.set_master_gain(3.0)
        assert mixer.get_master_gain() == pytest.approx(3.0)

        # Clamped to [0.0, 8.0].
        mixer.set_master_gain(100.0)
        assert mixer.get_master_gain() == pytest.approx(8.0)

        mixer.set_master_gain(-2.0)
        assert mixer.get_master_gain() == pytest.approx(0.0)
    except ImportError as e:
        pytest.skip(f"Module not built yet: {e}")


def test_reset_clears_tracks_and_master_gain():
    """reset() removes all tracks and restores master gain to 1.0."""
    try:
        from ffvoice import AudioMixer

        mixer = AudioMixer()
        mixer.initialize(48000, 1)

        mixer.add_track()
        mixer.add_track()
        mixer.set_master_gain(4.0)
        assert mixer.get_track_count() == 2

        mixer.reset()

        assert mixer.get_track_count() == 0
        assert mixer.get_master_gain() == pytest.approx(1.0)
        # The mixer remains initialized after reset.
        assert mixer.is_initialized() is True
    except ImportError as e:
        pytest.skip(f"Module not built yet: {e}")


def test_mix_block_single_track_passthrough():
    """A single unity-gain track passes through unchanged."""
    try:
        import numpy as np
        from ffvoice import AudioMixer

        mixer = AudioMixer()
        mixer.initialize(48000, 1)
        track_id = mixer.add_track()  # default gain 1.0

        samples = np.array([0, 100, -100, 5000, -5000, 32000], dtype=np.int16)
        mixed = mixer.mix_block({track_id: samples})

        assert mixed.dtype == np.int16
        assert len(mixed) == len(samples)
        np.testing.assert_array_equal(mixed, samples)
    except ImportError as e:
        pytest.skip(f"Module not built yet: {e}")


def test_mix_block_two_tracks_summed():
    """Two tracks are summed sample by sample."""
    try:
        import numpy as np
        from ffvoice import AudioMixer

        mixer = AudioMixer()
        mixer.initialize(48000, 1)
        track_a = mixer.add_track()
        track_b = mixer.add_track()

        a = np.array([100, 200, -300, 400], dtype=np.int16)
        b = np.array([10, -20, 30, -40], dtype=np.int16)
        mixed = mixer.mix_block({track_a: a, track_b: b})

        expected = np.array([110, 180, -270, 360], dtype=np.int16)
        assert mixed.dtype == np.int16
        np.testing.assert_array_equal(mixed, expected)
    except ImportError as e:
        pytest.skip(f"Module not built yet: {e}")


def test_mix_block_gain_scaling():
    """Track gain scales the samples in the mixed output."""
    try:
        import numpy as np
        from ffvoice import AudioMixer

        mixer = AudioMixer()
        mixer.initialize(48000, 1)
        track_id = mixer.add_track()
        mixer.set_gain(track_id, 2.0)

        samples = np.array([100, -200, 300, -400], dtype=np.int16)
        mixed = mixer.mix_block({track_id: samples})

        expected = np.array([200, -400, 600, -800], dtype=np.int16)
        np.testing.assert_array_equal(mixed, expected)
    except ImportError as e:
        pytest.skip(f"Module not built yet: {e}")


def test_mix_block_clipping_saturates():
    """Summed values beyond the int16 range saturate at 32767."""
    try:
        import numpy as np
        from ffvoice import AudioMixer

        mixer = AudioMixer()
        mixer.initialize(48000, 1)
        track_a = mixer.add_track()
        track_b = mixer.add_track()

        # 30000 + 30000 = 60000, well above the int16 max of 32767.
        a = np.array([30000, 30000, 30000], dtype=np.int16)
        b = np.array([30000, 30000, 30000], dtype=np.int16)
        mixed = mixer.mix_block({track_a: a, track_b: b})

        assert mixed.dtype == np.int16
        np.testing.assert_array_equal(mixed, np.full(3, 32767, dtype=np.int16))
    except ImportError as e:
        pytest.skip(f"Module not built yet: {e}")


def test_mix_block_clipping_saturates_negative():
    """Summed values below the int16 range saturate at -32768."""
    try:
        import numpy as np
        from ffvoice import AudioMixer

        mixer = AudioMixer()
        mixer.initialize(48000, 1)
        track_a = mixer.add_track()
        track_b = mixer.add_track()

        # -30000 + -30000 = -60000, well below the int16 min of -32768.
        a = np.array([-30000, -30000, -30000], dtype=np.int16)
        b = np.array([-30000, -30000, -30000], dtype=np.int16)
        mixed = mixer.mix_block({track_a: a, track_b: b})

        assert mixed.dtype == np.int16
        np.testing.assert_array_equal(mixed, np.full(3, -32768, dtype=np.int16))
    except ImportError as e:
        pytest.skip(f"Module not built yet: {e}")


def test_mix_block_muted_track_excluded():
    """A muted track contributes nothing to the mix."""
    try:
        import numpy as np
        from ffvoice import AudioMixer

        mixer = AudioMixer()
        mixer.initialize(48000, 1)
        track_a = mixer.add_track()
        track_b = mixer.add_track()
        mixer.set_mute(track_b, True)

        a = np.array([100, 200, 300, 400], dtype=np.int16)
        b = np.array([1000, 2000, 3000, 4000], dtype=np.int16)
        mixed = mixer.mix_block({track_a: a, track_b: b})

        # Only track_a contributes.
        np.testing.assert_array_equal(mixed, a)
    except ImportError as e:
        pytest.skip(f"Module not built yet: {e}")


if __name__ == "__main__":
    pytest.main([__file__, "-v"])
