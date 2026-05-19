"""
DiarizationPipeline — file transcription + speaker diarization helper.

This module is intentionally free of any ``mcp`` imports so it can be
unit-tested in isolation without an MCP server context.

The pipeline transcribes an audio file with Whisper ASR, runs speaker
diarization over the same audio with a ``Diarizer``, then merges the speaker
labels onto the transcription segments via ``ffvoice.merge_into_segments``.

Audio loading is delegated to a ``load_fn`` seam (defaulting to
``_default_load_audio``) so tests can inject synthetic audio without touching
the filesystem or any audio-decoding library.
"""

from __future__ import annotations

from typing import Any, Callable, Dict, List, Tuple

import numpy as np

import ffvoice

# Sample rate (Hz) the diarization models expect.
_TARGET_SAMPLE_RATE = 16000


def _default_load_audio(path: str) -> Tuple[List[float], int]:
    """
    Load an audio file into mono float32 samples at 16 kHz.

    Tries ``soundfile`` first (handles WAV/FLAC/OGG/…); falls back to
    ``scipy.io.wavfile`` for plain WAV files.  Stereo audio is down-mixed to
    mono by averaging channels, and audio is resampled to 16 kHz when
    ``scipy`` is available.

    Args:
        path: Path to the audio file.

    Returns:
        A ``(samples, sample_rate)`` tuple — ``samples`` is a list of floats
        normalised to roughly [-1, 1], ``sample_rate`` is in Hz.

    Raises:
        ImportError: If neither ``soundfile`` nor ``scipy`` is importable.
    """
    samples: np.ndarray
    sample_rate: int

    # --- Preferred path: soundfile -------------------------------------
    try:
        import soundfile as sf  # type: ignore

        data, sample_rate = sf.read(path, dtype="float32", always_2d=False)
        samples = np.asarray(data, dtype=np.float32)
        if samples.ndim > 1:
            samples = samples.mean(axis=1).astype(np.float32)
    except ImportError:
        # --- Fallback path: scipy.io.wavfile ---------------------------
        try:
            from scipy.io import wavfile  # type: ignore
        except ImportError as exc:
            raise ImportError(
                "Audio loading requires either 'soundfile' or 'scipy'. "
                "Install one with: pip install soundfile  (or)  pip install scipy"
            ) from exc

        sample_rate, raw = wavfile.read(path)
        raw_arr = np.asarray(raw)
        if raw_arr.ndim > 1:
            raw_arr = raw_arr.mean(axis=1)
        if raw_arr.dtype == np.int16:
            samples = raw_arr.astype(np.float32) / 32768.0
        elif raw_arr.dtype == np.int32:
            samples = raw_arr.astype(np.float32) / 2147483648.0
        elif raw_arr.dtype == np.uint8:
            samples = (raw_arr.astype(np.float32) - 128.0) / 128.0
        else:
            samples = raw_arr.astype(np.float32)

    # --- Resample to 16 kHz when scipy is available --------------------
    if sample_rate != _TARGET_SAMPLE_RATE:
        try:
            from scipy.signal import resample_poly  # type: ignore

            samples = resample_poly(samples, _TARGET_SAMPLE_RATE, sample_rate).astype(np.float32)
            sample_rate = _TARGET_SAMPLE_RATE
        except ImportError:
            # scipy missing — return the audio at its native rate; Diarizer
            # will report a sample-rate mismatch if it cannot handle it.
            pass

    return samples.astype(np.float32).tolist(), int(sample_rate)


class DiarizationPipeline:
    """
    Transcribe an audio file and annotate each segment with a speaker.

    The caller constructs the ffvoice objects (an initialised ``WhisperASR``
    and an initialised ``Diarizer``) and passes them in.  This keeps the class
    fully unit-testable via mocks without any compiled extension.

    Usage::

        asr = ffvoice.WhisperASR(whisper_config)
        asr.initialize()

        diarizer = ffvoice.Diarizer(diarizer_config)
        diarizer.init()

        pipeline = DiarizationPipeline(asr=asr, diarizer=diarizer)
        result = pipeline.run("/path/to/audio.wav")
    """

    def __init__(
        self,
        asr: Any,
        diarizer: Any,
        load_fn: Callable[[str], Tuple[List[float], int]] | None = None,
    ) -> None:
        self._asr = asr
        self._diarizer = diarizer
        # load_fn is the test seam — defaults to the real audio loader.
        self._load_fn = load_fn if load_fn is not None else _default_load_audio

    def run(self, path: str, word_timestamps: bool = False) -> Dict[str, Any]:
        """
        Run transcription + diarization + merge for *path*.

        Args:
            path: Path to the audio file.
            word_timestamps: When True, include per-word timing in each
                serialised segment's ``words`` field.

        Returns:
            A dict with ``segments``, ``speaker_segments``, ``inference_ms``
            and ``num_speakers_detected``.

        Raises:
            RuntimeError: If any stage (transcription, audio loading,
                diarization, or merge) fails.
        """
        # --- Stage 1: transcription ------------------------------------
        try:
            segments = list(self._asr.transcribe_file(path))
        except Exception as exc:
            raise RuntimeError(f"Transcription failed: {exc}") from exc

        inference_ms = 0
        try:
            inference_ms = int(self._asr.get_last_inference_time_ms())
        except Exception:
            inference_ms = 0

        # --- Stage 2: audio loading ------------------------------------
        try:
            raw_samples, sample_rate = self._load_fn(path)
        except Exception as exc:
            raise RuntimeError(f"Audio loading failed: {exc}") from exc

        # --- Stage 3: diarization --------------------------------------
        try:
            audio_array = np.asarray(raw_samples, dtype=np.float32)
            speaker_segments = list(self._diarizer.diarize(audio_array, sample_rate))
        except Exception as exc:
            raise RuntimeError(f"Diarization failed: {exc}") from exc

        # --- Stage 4: merge --------------------------------------------
        try:
            merged = ffvoice.merge_into_segments(list(segments), list(speaker_segments))
        except Exception as exc:
            raise RuntimeError(f"Merging speaker labels failed: {exc}") from exc

        # --- Stage 5: serialise ----------------------------------------
        out_segments: List[Dict[str, Any]] = []
        for seg in merged:
            d: Dict[str, Any] = {
                "start_ms": seg.start_ms,
                "end_ms": seg.end_ms,
                "text": seg.text,
                "confidence": seg.confidence,
                "speaker_id": seg.speaker_id,
            }
            if word_timestamps and getattr(seg, "words", None):
                d["words"] = [
                    {
                        "start_ms": w.start_ms,
                        "end_ms": w.end_ms,
                        "text": w.text,
                        "probability": w.probability,
                    }
                    for w in seg.words
                ]
            out_segments.append(d)

        out_speakers: List[Dict[str, Any]] = [
            {
                "start_ms": sp.start_ms,
                "end_ms": sp.end_ms,
                "speaker_id": sp.speaker_id,
            }
            for sp in speaker_segments
        ]

        distinct_speakers = {sp["speaker_id"] for sp in out_speakers if sp["speaker_id"] >= 0}

        return {
            "segments": out_segments,
            "speaker_segments": out_speakers,
            "inference_ms": inference_ms,
            "num_speakers_detected": len(distinct_speakers),
        }
