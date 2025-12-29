"""
ffvoice - High-performance offline speech recognition library for Python

A Python binding for ffvoice-engine, providing:
- Real-time audio capture and recording
- AI-powered noise reduction (RNNoise)
- Voice Activity Detection (VAD)
- Offline speech recognition (Whisper ASR)
- Real-time transcription with intelligent segmentation

Example:
    >>> from ffvoice import AudioCapture, WhisperASR
    >>> capture = AudioCapture()
    >>> asr = WhisperASR(model="tiny")
    >>> transcripts = asr.transcribe_file("audio.wav")
    >>> for segment in transcripts:
    ...     print(segment.text)
"""

__version__ = "0.5.0"
__author__ = "ffvoice-engine contributors"
__license__ = "MIT"

# Import native module (will be built by pybind11)
try:
    from ._ffvoice import *
except ImportError as e:
    raise ImportError(
        "Failed to import ffvoice native module. "
        "Please ensure ffvoice is properly installed. "
        f"Error: {e}"
    )

# Public API
__all__ = [
    # Core classes (will be populated from C++ bindings)
    "__version__",
]
