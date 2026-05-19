"""
Type stubs for _ffvoice — the compiled pybind11 extension module.

These stubs are generated from src/python/bindings.cpp and allow IDEs
and type checkers (mypy, pyright) to provide autocompletion and type
checking without the compiled binary.
"""

from __future__ import annotations

from typing import Callable, Optional
import numpy as np

# ---------------------------------------------------------------------------
# Enums
# ---------------------------------------------------------------------------

class WhisperModelType:
    """Whisper model size; larger models are slower but more accurate."""

    TINY: WhisperModelType  # ~39 MB, ~10x real-time
    BASE: WhisperModelType  # ~74 MB, ~7x real-time
    SMALL: WhisperModelType  # ~244 MB, ~3x real-time
    MEDIUM: WhisperModelType  # ~769 MB, ~1x real-time
    LARGE: WhisperModelType  # ~1550 MB, <1x real-time

class VADSensitivity:
    """Preset sensitivity levels for VADConfig.from_preset()."""

    VERY_SENSITIVE: VADSensitivity  # threshold=0.3
    SENSITIVE: VADSensitivity  # threshold=0.4
    BALANCED: VADSensitivity  # threshold=0.5 (default)
    CONSERVATIVE: VADSensitivity  # threshold=0.6
    VERY_CONSERVATIVE: VADSensitivity  # threshold=0.7

# ---------------------------------------------------------------------------
# Data classes
# ---------------------------------------------------------------------------

class Word:
    """Per-word timestamp within a TranscriptionSegment."""

    start_ms: int
    """Word start time in milliseconds."""
    end_ms: int
    """Word end time in milliseconds."""
    text: str
    """Word text."""
    probability: float
    """Mean token probability for this word (0.0 – 1.0)."""

    def __init__(
        self,
        start_ms: int,
        end_ms: int,
        text: str,
        probability: float,
    ) -> None: ...
    def __repr__(self) -> str: ...

class TranscriptionSegment:
    """A single speech recognition result with timestamps."""

    start_ms: int
    """Segment start time in milliseconds."""
    end_ms: int
    """Segment end time in milliseconds."""
    text: str
    """Transcribed text."""
    confidence: float
    """Confidence score (0.0 – 1.0)."""
    speaker_id: int
    """Speaker index, 0-based; -1 = unknown / diarization not run."""
    words: list[Word]
    """Per-word timestamps (empty unless WhisperConfig.word_timestamps was True)."""

    def __init__(
        self,
        start_ms: int,
        end_ms: int,
        text: str,
        confidence: float = 0.0,
    ) -> None: ...
    def __repr__(self) -> str: ...

class SpeakerSegment:
    """A diarization segment: a contiguous span attributed to one speaker."""

    start_ms: int
    """Segment start time in milliseconds."""
    end_ms: int
    """Segment end time in milliseconds."""
    speaker_id: int
    """Speaker index, 0-based; -1 = unknown."""

    def __init__(
        self,
        start_ms: int = ...,
        end_ms: int = ...,
        speaker_id: int = ...,
    ) -> None: ...
    def __repr__(self) -> str: ...

def merge_into_segments(
    segments: list[TranscriptionSegment],
    speakers: list[SpeakerSegment],
) -> list[TranscriptionSegment]:
    """
    Assign a speaker_id to each TranscriptionSegment via temporal overlap.

    For every segment, the SpeakerSegment with the largest millisecond overlap
    wins.  Returns the annotated list of segments.
    """
    ...

HAS_DIARIZATION: bool
"""True when the extension was built with ENABLE_DIARIZATION=ON."""

class AudioDeviceInfo:
    """Information about an audio device returned by AudioCapture.get_devices()."""

    id: int
    """Device ID."""
    name: str
    """Human-readable device name."""
    max_input_channels: int
    """Maximum number of input channels."""
    max_output_channels: int
    """Maximum number of output channels."""
    supported_sample_rates: list[int]
    """Sample rates supported by this device."""
    is_default: bool
    """True if this is the system default input device."""

# ---------------------------------------------------------------------------
# Configuration classes
# ---------------------------------------------------------------------------

class WhisperConfig:
    """Configuration for WhisperASR."""

    model_path: str
    """Path to the Whisper GGML model file."""
    language: str
    """Language code ('en', 'zh', 'auto', …)."""
    model_type: WhisperModelType
    """Model size."""
    n_threads: int
    """Number of CPU threads for inference."""
    translate: bool
    """Translate output to English."""
    print_progress: bool
    """Print inference progress to stdout."""
    print_timestamps: bool
    """Print timestamps together with text."""
    enable_performance_metrics: bool
    """Collect detailed timing metrics."""
    word_timestamps: bool
    """Populate TranscriptionSegment.words with per-word timing."""
    input_sample_rate: int
    """Sample rate (Hz) of audio passed to transcribe_buffer(). Default: 48000."""

    def __init__(self) -> None: ...

class VADConfig:
    """Configuration for VADSegmenter."""

    speech_threshold: float
    """VAD probability threshold for speech detection (0.0 – 1.0)."""
    min_speech_frames: int
    """Minimum consecutive frames to trigger speech start."""
    min_silence_frames: int
    """Minimum consecutive frames to trigger speech end."""
    max_segment_samples: int
    """Maximum segment length in samples."""
    enable_adaptive_threshold: bool
    """Enable adaptive threshold adjustment."""
    adaptive_factor: float
    """Adaptation speed (0.0 – 1.0)."""

    def __init__(self) -> None: ...
    @staticmethod
    def from_preset(sensitivity: VADSensitivity) -> VADConfig:
        """Create a VADConfig from a built-in sensitivity preset."""
        ...

class RNNoiseConfig:
    """Configuration for RNNoise (only available in ENABLE_RNNOISE builds)."""

    enable_vad: bool
    """Enable VAD probability output from RNNoise."""

    def __init__(self) -> None: ...

class DiarizerConfig:
    """Configuration for Diarizer (only available in ENABLE_DIARIZATION builds)."""

    segmentation_model_path: str
    """Path to the pyannote speaker-segmentation model (.onnx)."""
    embedding_model_path: str
    """Path to the speaker-embedding model (.onnx)."""
    num_speakers: int
    """Expected number of speakers; -1 = auto-detect via cluster_threshold."""
    cluster_threshold: float
    """Clustering distance threshold (used only when num_speakers <= 0)."""
    num_threads: int
    """Inference threads for the segmentation and embedding models."""

    def __init__(self) -> None: ...

# ---------------------------------------------------------------------------
# Processing classes
# ---------------------------------------------------------------------------

class WhisperASR:
    """Offline speech recognition using whisper.cpp."""

    def __init__(self, config: WhisperConfig = ...) -> None: ...
    def initialize(self) -> bool:
        """Load the Whisper model. Returns True on success."""
        ...

    def is_initialized(self) -> bool:
        """Return True if the model is loaded."""
        ...

    def transcribe_file(self, audio_file: str) -> list[TranscriptionSegment]:
        """Transcribe an audio file. Raises RuntimeError on failure."""
        ...

    def transcribe_buffer(self, audio_array: np.ndarray) -> list[TranscriptionSegment]:
        """
        Transcribe audio from a 1-D int16 NumPy array.

        The array must be dtype=np.int16 and 1-dimensional.
        Set WhisperConfig.input_sample_rate to match the array's sample rate.
        Raises RuntimeError on failure.
        """
        ...

    def get_last_error(self) -> str:
        """Return the last error message (empty string if none)."""
        ...

    def get_last_inference_time_ms(self) -> int:
        """Return inference time of the most recent call in milliseconds."""
        ...

    @staticmethod
    def get_model_type_name(model_type: WhisperModelType) -> str:
        """Return the human-readable name of a WhisperModelType."""
        ...

class AudioCapture:
    """Real-time audio capture from a microphone via PortAudio."""

    def __init__(self) -> None: ...
    def open(
        self,
        device_id: int = -1,
        sample_rate: int = 48000,
        channels: int = 1,
        frames_per_buffer: int = 256,
    ) -> bool:
        """
        Open an audio capture device.

        Args:
            device_id: PortAudio device ID (-1 = system default).
            sample_rate: Sample rate in Hz.
            channels: Number of input channels.
            frames_per_buffer: Frames per audio callback.

        Returns:
            True on success.
        """
        ...

    def start(self, callback: Callable[[np.ndarray], None]) -> bool:
        """
        Start capturing audio.

        The *callback* is called on a background C++ thread for each audio
        frame; the argument is a 1-D int16 NumPy array of length
        `frames_per_buffer`.  Acquire the GIL before touching Python objects
        inside the callback (the binding does this automatically).
        """
        ...

    def stop(self) -> None:
        """Stop capturing audio."""
        ...

    def close(self) -> None:
        """Close the device."""
        ...

    def is_open(self) -> bool:
        """Return True if the device is open."""
        ...

    def is_capturing(self) -> bool:
        """Return True if audio capture is active."""
        ...

    def get_sample_rate(self) -> int:
        """Return the configured sample rate."""
        ...

    def get_channels(self) -> int:
        """Return the configured channel count."""
        ...

    @staticmethod
    def initialize() -> bool:
        """Initialize the PortAudio library. Call once before any AudioCapture."""
        ...

    @staticmethod
    def terminate() -> None:
        """Terminate the PortAudio library. Call once when done."""
        ...

    @staticmethod
    def get_devices() -> list[AudioDeviceInfo]:
        """Return a list of all available audio devices."""
        ...

    @staticmethod
    def get_default_input_device() -> int:
        """Return the default input device ID."""
        ...

class RNNoise:
    """
    AI-powered noise reduction using RNNoise.

    Only available when the library was built with ENABLE_RNNOISE=ON.
    """

    def __init__(self, config: RNNoiseConfig = ...) -> None: ...
    def initialize(self, sample_rate: int, channels: int) -> bool:
        """Initialize with the given sample rate and channel count."""
        ...

    def process(self, audio_array: np.ndarray) -> None:
        """
        Denoise audio in-place.

        *audio_array* must be a writable 1-D int16 NumPy array.
        The array is modified in-place.
        """
        ...

    def reset(self) -> None:
        """Reset internal filter state."""
        ...

    def get_vad_probability(self) -> float:
        """Return VAD probability from the most recent process() call (0.0 – 1.0)."""
        ...

class Diarizer:
    """
    Offline speaker diarization engine.

    Only available when the library was built with ENABLE_DIARIZATION=ON.
    """

    def __init__(self, config: DiarizerConfig = ...) -> None: ...
    def init(self) -> bool:
        """Initialize the diarizer and load the models. Returns True on success."""
        ...

    def is_initialized(self) -> bool:
        """Return True once init() has completed successfully."""
        ...

    def diarize(
        self,
        audio_array: np.ndarray,
        sample_rate: int = 16000,
    ) -> list[SpeakerSegment]:
        """
        Run speaker diarization on a 1-D float32 NumPy array of mono PCM samples.

        Returns speaker segments sorted by start time (empty on failure).
        """
        ...

    def get_last_error(self) -> str:
        """Return the last error message (empty string if none)."""
        ...

    def get_expected_sample_rate(self) -> int:
        """Return the input sample rate the models expect (Hz)."""
        ...

class VADSegmenter:
    """Voice Activity Detection segmenter with callback-based output."""

    def __init__(self, config: VADConfig = ...) -> None: ...
    def process_frame(
        self,
        audio_array: np.ndarray,
        vad_prob: float,
        callback: Callable[[np.ndarray], None],
    ) -> None:
        """
        Process one audio frame.

        When the segmenter detects the end of a speech segment it calls
        *callback* with a 1-D int16 NumPy array containing the complete
        segment audio.

        Args:
            audio_array: 1-D int16 NumPy array of the current frame.
            vad_prob:     VAD probability for this frame (e.g. from RNNoise).
            callback:     Called with the complete segment array.
        """
        ...

    def flush(self, callback: Callable[[np.ndarray], None]) -> None:
        """
        Flush any remaining buffered audio via *callback*.

        Call this when you are done capturing to ensure the last partial
        segment is not lost.
        """
        ...

    def reset(self) -> None:
        """Reset segmenter state."""
        ...

    def get_buffer_size(self) -> int:
        """Return the number of samples currently in the internal buffer."""
        ...

    def is_in_speech(self) -> bool:
        """Return True if the segmenter is currently inside a speech segment."""
        ...

    def get_current_threshold(self) -> float:
        """Return the current (possibly adaptive) VAD threshold."""
        ...

    def get_statistics(self) -> tuple[float, float]:
        """
        Return VAD statistics as a 2-tuple.

        Returns:
            (avg_vad_prob, speech_ratio)
              avg_vad_prob  – mean VAD probability across all processed frames
              speech_ratio  – fraction of frames classified as speech
        """
        ...

class WAVWriter:
    """Write PCM audio to a WAV file."""

    def __init__(self) -> None: ...
    def open(
        self,
        filename: str,
        sample_rate: int,
        channels: int,
        bits_per_sample: int = 16,
    ) -> bool:
        """Open *filename* for writing. Returns True on success."""
        ...

    def write_samples(self, samples: list[int]) -> bool:
        """Write PCM samples from a Python list of int16 values."""
        ...

    def write_samples_array(self, audio_array: np.ndarray) -> int:
        """
        Write PCM samples from a 1-D int16 NumPy array.

        Returns the number of samples written.
        """
        ...

    def is_open(self) -> bool:
        """Return True if the file is open."""
        ...

    def get_total_samples(self) -> int:
        """Return the total number of samples written so far."""
        ...

    def close(self) -> None:
        """Close the file and finalise the WAV header."""
        ...

class FLACWriter:
    """Write PCM audio to a FLAC file with lossless compression."""

    def __init__(self) -> None: ...
    def open(
        self,
        filename: str,
        sample_rate: int,
        channels: int,
        bits_per_sample: int = 16,
        compression_level: int = 5,
    ) -> bool:
        """
        Open *filename* for writing.

        Args:
            compression_level: 0 (fastest) to 8 (best compression). Default 5.

        Returns:
            True on success.
        """
        ...

    def write_samples(self, samples: list[int]) -> bool:
        """Write PCM samples from a Python list of int16 values."""
        ...

    def write_samples_array(self, audio_array: np.ndarray) -> int:
        """
        Write PCM samples from a 1-D int16 NumPy array.

        Returns the number of samples written.
        """
        ...

    def is_open(self) -> bool:
        """Return True if the file is open."""
        ...

    def get_total_samples(self) -> int:
        """Return the total number of samples written so far."""
        ...

    def get_compression_ratio(self) -> float:
        """Return the current compression ratio (original_size / compressed_size)."""
        ...

    def close(self) -> None:
        """Close the file and finalise the FLAC stream."""
        ...

class AudioMixer:
    """Multi-track mixer that combines several int16 audio tracks into one output."""

    def __init__(self) -> None: ...
    def initialize(self, sample_rate: int, channels: int) -> bool:
        """
        Initialize the mixer.

        Args:
            channels: Must be 1 (mono) or 2 (stereo).

        Returns:
            True on success.
        """
        ...

    def is_initialized(self) -> bool: ...
    def get_sample_rate(self) -> int: ...
    def get_channels(self) -> int: ...
    def add_track(self, gain: float = 1.0, pan: float = 0.0) -> int:
        """
        Add a new track.

        Args:
            gain: Linear gain clamped to [0, 8]. Default 1.0.
            pan:  Stereo pan: -1=left, 0=centre, +1=right. Default 0.0.

        Returns:
            Track ID (non-negative int), or -1 if the mixer is not initialised.
        """
        ...

    def remove_track(self, track_id: int) -> bool:
        """Remove a track. Returns True if the track existed."""
        ...

    def has_track(self, track_id: int) -> bool: ...
    def get_track_count(self) -> int: ...
    def set_gain(self, track_id: int, gain: float) -> bool: ...
    def get_gain(self, track_id: int) -> float: ...
    def set_pan(self, track_id: int, pan: float) -> bool: ...
    def get_pan(self, track_id: int) -> float: ...
    def set_mute(self, track_id: int, muted: bool) -> bool: ...
    def is_muted(self, track_id: int) -> bool: ...
    def set_master_gain(self, gain: float) -> None: ...
    def get_master_gain(self) -> float: ...
    def reset(self) -> None:
        """Remove all tracks and reset master gain to 1.0."""
        ...

    def mix_block(self, tracks: dict[int, np.ndarray]) -> np.ndarray:
        """
        Mix a block of audio from multiple tracks.

        Args:
            tracks: Mapping of track_id -> 1-D int16 NumPy array.
                    All arrays must have the same length.

        Returns:
            Mixed output as a 1-D int16 NumPy array.

        Raises:
            RuntimeError: If the mixer is not initialised, or arrays have
                          different lengths, or any array is not 1-D.
        """
        ...

class RingBuffer:
    """Lock-free single-producer/single-consumer (SPSC) ring buffer for int16 audio."""

    def __init__(self, capacity: int) -> None:
        """
        Create a ring buffer.

        Args:
            capacity: Maximum number of int16 samples the buffer can hold.
        """
        ...

    def capacity(self) -> int:
        """Maximum number of elements."""
        ...

    def size(self) -> int:
        """Number of elements currently stored."""
        ...

    def empty(self) -> bool:
        """Return True if the buffer is empty."""
        ...

    def full(self) -> bool:
        """Return True if the buffer is full."""
        ...

    def available_read(self) -> int:
        """Number of elements available to pop."""
        ...

    def available_write(self) -> int:
        """Number of free slots available for pushing."""
        ...

    def push(self, value: int) -> bool:
        """Push one int16 value. Returns False if the buffer is full."""
        ...

    def pop(self) -> Optional[int]:
        """Pop one int16 value. Returns None if the buffer is empty."""
        ...

    def push_bulk(self, data: np.ndarray) -> int:
        """
        Push values from a 1-D int16 NumPy array.

        Returns the number of values actually pushed (may be less than len(data)
        if the buffer fills up).
        """
        ...

    def pop_bulk(self, count: int) -> np.ndarray:
        """
        Pop up to *count* values.

        Returns a 1-D int16 NumPy array (may be shorter than *count* if fewer
        elements are available).
        """
        ...

    def clear(self) -> None:
        """Reset the buffer to empty."""
        ...
