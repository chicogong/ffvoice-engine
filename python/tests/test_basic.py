"""
Basic tests for ffvoice Python bindings
"""

import pytest


def test_import():
    """Test that ffvoice module can be imported"""
    try:
        import ffvoice
        assert ffvoice.__version__ == "0.4.0"
    except ImportError as e:
        pytest.skip(f"Module not built yet: {e}")


def test_transcription_segment():
    """Test TranscriptionSegment class"""
    try:
        from ffvoice import TranscriptionSegment

        segment = TranscriptionSegment(0, 1000, "Hello world")
        assert segment.start_time_ms == 0
        assert segment.end_time_ms == 1000
        assert segment.text == "Hello world"
        assert "Hello world" in repr(segment)
    except ImportError as e:
        pytest.skip(f"Module not built yet: {e}")


def test_whisper_model_type():
    """Test WhisperModelType enum"""
    try:
        from ffvoice import WhisperModelType

        # Check that all model types exist
        assert hasattr(WhisperModelType, "TINY")
        assert hasattr(WhisperModelType, "BASE")
        assert hasattr(WhisperModelType, "SMALL")
        assert hasattr(WhisperModelType, "MEDIUM")
        assert hasattr(WhisperModelType, "LARGE")
    except ImportError as e:
        pytest.skip(f"Module not built yet: {e}")


def test_whisper_config():
    """Test WhisperConfig class"""
    try:
        from ffvoice import WhisperConfig, WhisperModelType

        config = WhisperConfig()
        assert config.language == "auto"
        assert config.n_threads == 4
        assert config.translate == False
        assert config.enable_performance_metrics == False

        # Test modification
        config.language = "en"
        config.model_type = WhisperModelType.TINY
        config.n_threads = 8

        assert config.language == "en"
        assert config.model_type == WhisperModelType.TINY
        assert config.n_threads == 8
    except ImportError as e:
        pytest.skip(f"Module not built yet: {e}")


def test_audio_capture_config():
    """Test AudioCaptureConfig class"""
    try:
        from ffvoice import AudioCaptureConfig

        config = AudioCaptureConfig()
        assert config.sample_rate == 48000
        assert config.channels == 1
        assert config.frames_per_buffer == 256
        assert config.device_index == -1

        # Test modification
        config.sample_rate = 16000
        config.channels = 2
        assert config.sample_rate == 16000
        assert config.channels == 2
    except ImportError as e:
        pytest.skip(f"Module not built yet: {e}")


def test_vad_sensitivity():
    """Test VAD sensitivity presets"""
    try:
        from ffvoice import VADSensitivity, VADConfig

        # Test enum values exist
        assert hasattr(VADSensitivity, "VERY_SENSITIVE")
        assert hasattr(VADSensitivity, "SENSITIVE")
        assert hasattr(VADSensitivity, "BALANCED")
        assert hasattr(VADSensitivity, "CONSERVATIVE")
        assert hasattr(VADSensitivity, "VERY_CONSERVATIVE")

        # Test preset creation
        config = VADConfig.from_preset(VADSensitivity.BALANCED)
        assert config.speech_threshold == 0.5
    except ImportError as e:
        pytest.skip(f"Module not built yet: {e}")


def test_rnnoise_config():
    """Test RNNoiseConfig class"""
    try:
        from ffvoice import RNNoiseConfig

        config = RNNoiseConfig()
        assert hasattr(config, "enable_vad")

        # Test modification
        config.enable_vad = True
        assert config.enable_vad == True
    except ImportError as e:
        pytest.skip(f"Module not built yet: {e}")


if __name__ == "__main__":
    pytest.main([__file__, "-v"])
