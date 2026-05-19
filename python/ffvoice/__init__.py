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

__version__ = "0.8.3"
__author__ = "ffvoice-engine contributors"
__license__ = "MIT"

# Import native module (built by pybind11 / CMake)
try:
    from ._ffvoice import *  # noqa: F401, F403
    from ._ffvoice import (  # explicit re-exports for type checkers
        # Core data types
        Word,
        TranscriptionSegment,
        SpeakerSegment,
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
        # Free functions
        merge_into_segments,
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

# LiveCaptioner is only present when the library was built with ENABLE_WHISPER=ON.
# Import conditionally — the package remains fully usable without it.
try:
    from ._ffvoice import (  # noqa: F401
        CaptionEvent,
        CaptionEventType,
        LiveCaptioner,
        LiveCaptionerConfig,
    )

    _HAS_LIVE_CAPTIONER = True
except ImportError:
    _HAS_LIVE_CAPTIONER = False

# Diarizer / DiarizerConfig are only present when the library was built with
# ENABLE_DIARIZATION=ON.  SpeakerSegment and merge_into_segments above are
# always available regardless of the build configuration.
try:
    from ._ffvoice import DiarizerConfig, Diarizer  # noqa: F401

    _HAS_DIARIZATION = True
except ImportError:
    _HAS_DIARIZATION = False

# HAS_DIARIZATION is always defined by the extension; fall back to False if the
# native module itself failed to import.
try:
    from ._ffvoice import HAS_DIARIZATION  # noqa: F401
except ImportError:
    HAS_DIARIZATION = False

__all__ = [
    # Package metadata
    "__version__",
    "__author__",
    "__license__",
    # Core data types
    "Word",
    "TranscriptionSegment",
    "SpeakerSegment",
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
    # RNNoise (conditionally available — only when built with ENABLE_RNNOISE=ON)
    "RNNoise",
    "RNNoiseConfig",
    # LiveCaptioner (conditionally available — only when built with ENABLE_WHISPER=ON)
    "LiveCaptioner",
    "LiveCaptionerConfig",
    "CaptionEvent",
    "CaptionEventType",
    # Speaker diarization
    "merge_into_segments",
    "HAS_DIARIZATION",
    # Diarizer (conditionally available — only when built with ENABLE_DIARIZATION=ON)
    "Diarizer",
    "DiarizerConfig",
]
