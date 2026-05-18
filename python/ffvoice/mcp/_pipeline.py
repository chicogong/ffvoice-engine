"""
CaptureSession — audio-capture + VAD + Whisper pipeline helper.

This module intentionally does NOT import anything from ``mcp`` so it can be
unit-tested in isolation without an MCP server context.
"""

from __future__ import annotations

import threading
import time
from typing import Any, Dict, List


class CaptureSession:
    """
    Run a fixed-duration microphone capture with VAD segmentation and ASR.

    The caller is responsible for constructing the ffvoice objects (AudioCapture,
    VADSegmenter, WhisperASR, and optionally RNNoise) and passing them in.
    This keeps the class fully unit-testable via mocks.

    Usage::

        session = CaptureSession(
            capture=capture,
            vad=vad,
            asr=asr,
            rnnoise=rnnoise_or_None,
            sample_rate=48000,
        )
        session.run(duration_seconds=10.0, device_id=-1)
        result = session.get_result()
    """

    def __init__(
        self,
        capture: Any,
        vad: Any,
        asr: Any,
        rnnoise: Any,  # may be None
        sample_rate: int = 48000,
    ) -> None:
        self._capture = capture
        self._vad = vad
        self._asr = asr
        self._rnnoise = rnnoise
        self._sample_rate = sample_rate

        self._segments: List[Dict[str, Any]] = []
        self._total_inference_ms: int = 0
        self._lock = threading.Lock()
        self._capture_start: float = 0.0
        self._denoise_applied: bool = False

    # ------------------------------------------------------------------
    # Internal callbacks
    # ------------------------------------------------------------------

    def _segment_callback(self, segment_array: Any) -> None:
        """Called by VADSegmenter when a complete speech segment is ready."""
        try:
            results = self._asr.transcribe_buffer(segment_array)
            inf_ms = self._asr.get_last_inference_time_ms()
        except Exception:
            return

        elapsed_ms = int((time.time() - self._capture_start) * 1000)
        with self._lock:
            self._total_inference_ms += inf_ms
            for seg in results:
                self._segments.append(
                    {
                        "start_ms": getattr(seg, "start_ms", 0),
                        "end_ms": getattr(seg, "end_ms", elapsed_ms),
                        "text": getattr(seg, "text", ""),
                        "confidence": getattr(seg, "confidence", 0.0),
                    }
                )

    def _audio_callback(self, audio_array: Any) -> None:
        """Called for each captured audio frame from the microphone."""
        if self._rnnoise is not None:
            try:
                self._rnnoise.process(audio_array)
                vad_prob = self._rnnoise.get_vad_probability()
            except Exception:
                import numpy as np

                energy = float(np.abs(audio_array).mean())
                vad_prob = min(1.0, energy / 3000.0)
        else:
            import numpy as np

            energy = float(np.abs(audio_array).mean())
            vad_prob = min(1.0, energy / 3000.0)

        self._vad.process_frame(audio_array, vad_prob, self._segment_callback)

    # ------------------------------------------------------------------
    # Public API
    # ------------------------------------------------------------------

    def run(self, duration_seconds: float, device_id: int = -1) -> None:
        """
        Open the audio device, capture for *duration_seconds*, then close.

        Raises RuntimeError if the device cannot be opened or capture cannot start.
        """
        import numpy as np  # confirm numpy available (already a hard dep)

        _ = np  # silence F401

        if not self._capture.open(
            device_id=device_id,
            sample_rate=self._sample_rate,
            channels=1,
            frames_per_buffer=256,
        ):
            raise RuntimeError("Failed to open audio device")

        self._denoise_applied = self._rnnoise is not None
        self._capture_start = time.time()

        try:
            if not self._capture.start(self._audio_callback):
                raise RuntimeError("Failed to start audio capture")
            time.sleep(duration_seconds)
        finally:
            if self._capture.is_capturing():
                self._capture.stop()
            if self._capture.is_open():
                self._capture.close()

        # Flush any remaining buffered speech
        self._vad.flush(self._segment_callback)

    def get_result(self) -> Dict[str, Any]:
        """Return the accumulated transcription result dict."""
        with self._lock:
            segs = list(self._segments)
            total_inf = self._total_inference_ms

        return {
            "segments": segs,
            "speech_segments_found": len(segs),
            "total_inference_ms": total_inf,
            "denoise_applied": self._denoise_applied,
        }
