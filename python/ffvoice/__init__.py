"""
ffvoice - High-performance offline speech recognition library for Python

A Python binding for ffvoice-engine, providing:
- Real-time audio capture and recording
- AI-powered noise reduction (RNNoise)
- Voice Activity Detection (VAD)
- Offline speech recognition (Whisper ASR)
- Real-time transcription with intelligent segmentation

Example::

    import ffvoice

    config = ffvoice.WhisperConfig()
    config.model_type = ffvoice.WhisperModelType.TINY
    asr = ffvoice.WhisperASR(config)
    asr.initialize()

    for segment in asr.transcribe_file("audio.wav"):
        print(f"[{segment.start_ms}ms -> {segment.end_ms}ms] {segment.text}")
"""

__version__ = "0.6.1"
__author__ = "ffvoice-engine contributors"
__license__ = "MIT"

# Import native module (built by pybind11 / CMake)
try:
    from ._ffvoice import *  # noqa: F401, F403
    from ._ffvoice import (  # explicit re-exports for type checkers
        # Core data types
        Word,
        TranscriptionSegment,
        AudioDeviceInfo,
        # Enums
        WhisperModelType,
        VADSensitivity,
        # Configuration classes
        WhisperConfig,
        VADConfig,
        # Main processing classes
        WhisperASR,
        AudioCapture,
        VADSegmenter,
        WAVWriter,
        FLACWriter,
        AudioMixer,
        RingBuffer,
    )
except ImportError as e:
    raise ImportError(
        "Failed to import ffvoice native module. "
        "Please ensure ffvoice is properly installed. "
        f"Error: {e}"
    ) from e

# RNNoise is only present when the library was built with ENABLE_RNNOISE=ON.
# Import it conditionally so the package can still be used without it.
try:
    from ._ffvoice import RNNoise, RNNoiseConfig  # noqa: F401

    _HAS_RNNOISE = True
except ImportError:
    _HAS_RNNOISE = False

__all__ = [
    # Package metadata
    "__version__",
    "__author__",
    "__license__",
    # Core data types
    "Word",
    "TranscriptionSegment",
    "AudioDeviceInfo",
    # Enums
    "WhisperModelType",
    "VADSensitivity",
    # Configuration classes
    "WhisperConfig",
    "VADConfig",
    # Main processing classes
    "WhisperASR",
    "AudioCapture",
    "VADSegmenter",
    "WAVWriter",
    "FLACWriter",
    "AudioMixer",
    "RingBuffer",
    # RNNoise (conditionally available)
    "RNNoise",
    "RNNoiseConfig",
]
