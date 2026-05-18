"""
LiveCaptionSession — fixed-duration live captioning pipeline helper.

This module is intentionally free of any ``mcp`` imports so it can be
unit-tested in isolation without an MCP server context.

The helper drives a ``LiveCaptioner`` (which owns its own worker thread and
VAD/Whisper pipeline) for a caller-specified wall-clock duration, accumulates
every ``CaptionEvent`` that arrives via the callback, then returns a summary
dict.  Audio is captured from the microphone via ``AudioCapture`` and fed into
the captioner's lock-free ring buffer on the capture callback thread.
"""

from __future__ import annotations

import threading
import time
from typing import Any, Dict, List


class LiveCaptionSession:
    """
    Run a fixed-duration live captioning session.

    The caller is responsible for constructing the ffvoice objects
    (``AudioCapture`` and ``LiveCaptioner``) and passing them in.  This keeps
    the class fully unit-testable via mocks without any compiled extension or
    audio hardware.

    Usage::

        config = ffvoice.LiveCaptionerConfig()
        config.whisper.model_path = "/path/to/ggml-tiny.bin"
        config.partial_interval_ms = 500

        captioner = ffvoice.LiveCaptioner(config)
        capture = ffvoice.AudioCapture()

        session = LiveCaptionSession(
            capture=capture,
            captioner=captioner,
            sample_rate=48000,
        )
        session.run(duration_seconds=10.0, device_id=-1)
        result = session.get_result()
    """

    def __init__(
        self,
        capture: Any,
        captioner: Any,
        sample_rate: int = 48000,
    ) -> None:
        self._capture = capture
        self._captioner = captioner
        self._sample_rate = sample_rate

        self._events: List[Dict[str, Any]] = []
        self._utterances_completed: int = 0
        self._lock = threading.Lock()
        self._capture_start: float = 0.0

    # ------------------------------------------------------------------
    # Internal callback
    # ------------------------------------------------------------------

    def _caption_callback(self, event: Any) -> None:
        """
        Invoked by ``LiveCaptioner`` from its worker thread on each
        ``CaptionEvent`` (Partial or Final).

        Thread-safe: appends to ``_events`` under ``_lock``.
        """
        event_type_str = "Final" if _is_final(event) else "Partial"
        entry: Dict[str, Any] = {
            "type": event_type_str,
            "utterance_id": int(event.utterance_id),
            "text": str(event.text),
            "utterance_start_ms": int(event.utterance_start_ms),
            "utterance_end_ms": int(event.utterance_end_ms),
            "confidence": float(event.confidence),
        }
        with self._lock:
            self._events.append(entry)
            if event_type_str == "Final":
                self._utterances_completed += 1

    def _audio_callback(self, audio_array: Any) -> None:
        """
        Called for each captured audio frame from the microphone.

        Feeds samples directly into the ``LiveCaptioner`` ring buffer.
        The call is non-blocking (lock-free ring buffer write); samples
        are dropped if the buffer is full.
        """
        try:
            self._captioner.feed_audio(audio_array)
        except Exception:
            pass

    # ------------------------------------------------------------------
    # Public API
    # ------------------------------------------------------------------

    def run(self, duration_seconds: float, device_id: int = -1) -> None:
        """
        Initialise the captioner, open the audio device, capture for
        *duration_seconds*, then stop everything cleanly.

        Raises ``RuntimeError`` if the captioner fails to initialise, the
        audio device cannot be opened, or capture cannot start.
        """
        # Initialise captioner and register callback
        if not self._captioner.initialize():
            raise RuntimeError(
                "LiveCaptioner.initialize() failed: " + self._captioner.get_last_error()
            )
        self._captioner.set_callback(self._caption_callback)

        # Start the captioner worker thread
        if not self._captioner.start():
            raise RuntimeError("LiveCaptioner.start() failed")

        # Open the audio capture device
        if not self._capture.open(
            device_id=device_id,
            sample_rate=self._sample_rate,
            channels=1,
            frames_per_buffer=256,
        ):
            self._captioner.stop()
            raise RuntimeError("Failed to open audio device")

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
            # stop() flushes buffered audio and emits any pending Final event
            self._captioner.stop()

    def get_result(self) -> Dict[str, Any]:
        """Return the accumulated caption result dict."""
        with self._lock:
            events = list(self._events)
            utterances_completed = self._utterances_completed

        duration_recorded = round(time.time() - self._capture_start, 2)
        return {
            "events": events,
            "duration_recorded_s": duration_recorded,
            "utterances_completed": utterances_completed,
        }


# ---------------------------------------------------------------------------
# Internal helper
# ---------------------------------------------------------------------------


def _is_final(event: Any) -> bool:
    """
    Return True when *event* represents a Final caption.

    Works with both the real ``CaptionEventType`` enum (C++ binding) and the
    string-based mock used in tests.
    """
    ev_type = event.type

    # Fast path: plain string values used by the mock layer
    ev_type_str = str(ev_type)
    if ev_type_str in ("Final", "CaptionEventType.Final"):
        return True
    if ev_type_str in ("Partial", "CaptionEventType.Partial"):
        return False

    # Slow path: compare against the real CaptionEventType enum value
    try:
        import ffvoice

        if hasattr(ffvoice, "CaptionEventType"):
            return ev_type == ffvoice.CaptionEventType.Final
    except Exception:
        pass

    return False
