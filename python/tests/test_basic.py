"""
Basic tests for ffvoice Python bindings
"""

import pytest


def test_import():
    """Test that ffvoice module can be imported"""
    try:
        import ffvoice

        assert ffvoice.__version__ == "0.6.1"
    except ImportError as e:
        pytest.skip(f"Module not built yet: {e}")


def test_transcription_segment():
    """Test TranscriptionSegment class"""
    try:
        from ffvoice import TranscriptionSegment

        segment = TranscriptionSegment(0, 1000, "Hello world")
        assert segment.start_ms == 0
        assert segment.end_ms == 1000
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


def test_audio_capture():
    """Test AudioCapture class is importable and exposes expected methods"""
    try:
        from ffvoice import AudioCapture

        # AudioCapture can be constructed without touching hardware.
        capture = AudioCapture()

        # Check expected methods exist (do not call ones that touch hardware).
        for method in (
            "open",
            "start",
            "stop",
            "close",
            "is_open",
            "is_capturing",
            "get_sample_rate",
            "get_channels",
        ):
            assert hasattr(capture, method)

        for static_method in (
            "initialize",
            "terminate",
            "get_devices",
            "get_default_input_device",
        ):
            assert hasattr(AudioCapture, static_method)

        # A freshly constructed device is neither open nor capturing.
        assert capture.is_open() == False
        assert capture.is_capturing() == False
    except ImportError as e:
        pytest.skip(f"Module not built yet: {e}")


def test_audio_device_info():
    """Test AudioDeviceInfo class is importable and exposes expected fields"""
    try:
        from ffvoice import AudioDeviceInfo

        # AudioDeviceInfo carries device metadata; verify the attributes
        # are bound (instances are produced by AudioCapture.get_devices()).
        for field in (
            "id",
            "name",
            "max_input_channels",
            "max_output_channels",
            "supported_sample_rates",
            "is_default",
        ):
            assert hasattr(AudioDeviceInfo, field)
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
