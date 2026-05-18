"""
Tests for the ffvoice word-level-timestamp Python API:
the Word class and the word-timestamp additions to
WhisperConfig and TranscriptionSegment.
"""

import pytest


def test_word_construction():
    """Test that Word can be constructed with positional arguments"""
    try:
        from ffvoice import Word

        word = Word(100, 250, "hello", 0.95)
        assert word is not None
    except ImportError as e:
        pytest.skip(f"Module not built yet: {e}")


def test_word_attributes_return_constructor_values():
    """Test that Word attributes return the values passed to the constructor"""
    try:
        from ffvoice import Word

        word = Word(100, 250, "hello", 0.95)
        assert word.start_ms == 100
        assert word.end_ms == 250
        assert word.text == "hello"
        assert word.probability == pytest.approx(0.95)
    except ImportError as e:
        pytest.skip(f"Module not built yet: {e}")


def test_word_construction_with_keywords():
    """Test that Word can be constructed with keyword arguments"""
    try:
        from ffvoice import Word

        word = Word(start_ms=10, end_ms=20, text="world", probability=0.5)
        assert word.start_ms == 10
        assert word.end_ms == 20
        assert word.text == "world"
        assert word.probability == pytest.approx(0.5)
    except ImportError as e:
        pytest.skip(f"Module not built yet: {e}")


def test_word_start_ms_read_only():
    """Test that Word.start_ms is read-only"""
    try:
        from ffvoice import Word

        word = Word(100, 250, "hello", 0.95)
        with pytest.raises((AttributeError, TypeError)):
            word.start_ms = 999
    except ImportError as e:
        pytest.skip(f"Module not built yet: {e}")


def test_word_end_ms_read_only():
    """Test that Word.end_ms is read-only"""
    try:
        from ffvoice import Word

        word = Word(100, 250, "hello", 0.95)
        with pytest.raises((AttributeError, TypeError)):
            word.end_ms = 999
    except ImportError as e:
        pytest.skip(f"Module not built yet: {e}")


def test_word_text_read_only():
    """Test that Word.text is read-only"""
    try:
        from ffvoice import Word

        word = Word(100, 250, "hello", 0.95)
        with pytest.raises((AttributeError, TypeError)):
            word.text = "changed"
    except ImportError as e:
        pytest.skip(f"Module not built yet: {e}")


def test_word_probability_read_only():
    """Test that Word.probability is read-only"""
    try:
        from ffvoice import Word

        word = Word(100, 250, "hello", 0.95)
        with pytest.raises((AttributeError, TypeError)):
            word.probability = 0.1
    except ImportError as e:
        pytest.skip(f"Module not built yet: {e}")


def test_word_repr_contains_text():
    """Test that repr(Word) contains the word text"""
    try:
        from ffvoice import Word

        word = Word(100, 250, "hello", 0.95)
        assert "hello" in repr(word)
    except ImportError as e:
        pytest.skip(f"Module not built yet: {e}")


def test_whisper_config_word_timestamps_default():
    """Test that WhisperConfig.word_timestamps defaults to False"""
    try:
        from ffvoice import WhisperConfig

        config = WhisperConfig()
        assert config.word_timestamps is False
    except ImportError as e:
        pytest.skip(f"Module not built yet: {e}")


def test_whisper_config_word_timestamps_assignable():
    """Test that WhisperConfig.word_timestamps can be set to True"""
    try:
        from ffvoice import WhisperConfig

        config = WhisperConfig()
        config.word_timestamps = True
        assert config.word_timestamps is True
    except ImportError as e:
        pytest.skip(f"Module not built yet: {e}")


def test_whisper_config_input_sample_rate_default():
    """Test that WhisperConfig.input_sample_rate defaults to 48000"""
    try:
        from ffvoice import WhisperConfig

        config = WhisperConfig()
        assert config.input_sample_rate == 48000
    except ImportError as e:
        pytest.skip(f"Module not built yet: {e}")


def test_whisper_config_input_sample_rate_assignable():
    """Test that WhisperConfig.input_sample_rate can be modified"""
    try:
        from ffvoice import WhisperConfig

        config = WhisperConfig()
        config.input_sample_rate = 16000
        assert config.input_sample_rate == 16000
    except ImportError as e:
        pytest.skip(f"Module not built yet: {e}")


def test_transcription_segment_words_empty_by_default():
    """Test that TranscriptionSegment.words is an empty list by default"""
    try:
        from ffvoice import TranscriptionSegment

        segment = TranscriptionSegment(0, 1000, "hi")
        assert segment.words == []
    except ImportError as e:
        pytest.skip(f"Module not built yet: {e}")


if __name__ == "__main__":
    pytest.main([__file__, "-v"])
