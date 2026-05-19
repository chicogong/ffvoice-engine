"""
Unit tests for the LiveCaptioner Python binding and the capture_and_caption
MCP tool.

All ffvoice native bindings are mocked — no compiled extension or audio
hardware is required to run these tests.

Strategy
--------
We reuse the same sys.modules injection pattern as ``test_mcp_tools.py``:
inject a synthetic ``ffvoice._ffvoice`` extension (carrying mock
``LiveCaptioner``, ``LiveCaptionerConfig``, ``CaptionEvent``, and
``CaptionEventType`` classes) before importing the real ``ffvoice`` package
and the MCP server.
"""

from __future__ import annotations

import importlib
import pathlib
import sys
import threading
import types
from typing import Any, Callable, List, Optional
from unittest.mock import MagicMock, patch

import numpy as np
import pytest

# ---------------------------------------------------------------------------
# Ensure python/ is on sys.path so the real ffvoice package is importable.
# ---------------------------------------------------------------------------
_PYTHON_DIR = str(pathlib.Path(__file__).parent.parent)
if _PYTHON_DIR not in sys.path:
    sys.path.insert(0, _PYTHON_DIR)


# ---------------------------------------------------------------------------
# Mock CaptionEventType / CaptionEvent
# ---------------------------------------------------------------------------


class _CaptionEventType:
    Partial = "Partial"
    Final = "Final"


class _CaptionEvent:
    """Minimal mock of the C++ CaptionEvent struct."""

    def __init__(
        self,
        event_type: str = "Final",
        text: str = "",
        utterance_id: int = 1,
        utterance_start_ms: int = 0,
        utterance_end_ms: int = 1000,
        confidence: float = 0.9,
    ) -> None:
        self.type = event_type
        self.text = text
        self.utterance_id = utterance_id
        self.utterance_start_ms = utterance_start_ms
        self.utterance_end_ms = utterance_end_ms
        self.confidence = confidence


def _make_final_event(
    text: str = "hello world",
    utterance_id: int = 1,
    start_ms: int = 0,
    end_ms: int = 2000,
    confidence: float = 0.88,
) -> _CaptionEvent:
    return _CaptionEvent(
        event_type=_CaptionEventType.Final,
        text=text,
        utterance_id=utterance_id,
        utterance_start_ms=start_ms,
        utterance_end_ms=end_ms,
        confidence=confidence,
    )


def _make_partial_event(
    text: str = "hel...",
    utterance_id: int = 1,
) -> _CaptionEvent:
    return _CaptionEvent(
        event_type=_CaptionEventType.Partial,
        text=text,
        utterance_id=utterance_id,
        utterance_start_ms=0,
        utterance_end_ms=500,
        confidence=0.0,
    )


# ---------------------------------------------------------------------------
# Mock LiveCaptionerConfig / LiveCaptioner
# ---------------------------------------------------------------------------


class _LiveCaptionerConfig:
    def __init__(self) -> None:
        self.partial_interval_ms: int = 500
        self.min_samples_for_partial: int = 16000
        self.ring_buffer_capacity: int = 144000
        self.sample_rate: int = 48000
        self.channels: int = 1
        self.suppress_whisper_progress: bool = True
        self.whisper = _WhisperConfigStub()
        self.vad = MagicMock()


class _WhisperConfigStub:
    def __init__(self) -> None:
        self.model_path: str = ""
        self.language: str = "auto"
        self.model_type: str = "TINY"
        self.print_progress: bool = False


class _LiveCaptioner:
    """
    Mock LiveCaptioner.

    By default, ``initialize()`` and ``start()`` succeed.  A test can inject
    ``_injected_events`` to simulate caption callbacks during ``start()``.
    """

    def __init__(self, config: Any = None) -> None:
        self._config = config
        self._callback: Optional[Callable] = None
        self._running: bool = False
        self._init_ok: bool = True
        self._start_ok: bool = True
        self._error: str = ""
        # Tests can populate this list to drive the callback
        self._injected_events: List[_CaptionEvent] = []

    def initialize(self) -> bool:
        return self._init_ok

    def set_callback(self, callback: Callable) -> None:
        self._callback = callback

    def start(self) -> bool:
        if not self._start_ok:
            return False
        self._running = True
        # Fire any pre-injected events synchronously so tests are deterministic
        if self._callback is not None:
            for ev in self._injected_events:
                self._callback(ev)
        return True

    def stop(self) -> None:
        self._running = False

    def feed_audio(self, audio_array: Any) -> int:
        return int(len(audio_array)) if hasattr(audio_array, "__len__") else 0

    def is_running(self) -> bool:
        return self._running

    def get_last_error(self) -> str:
        return self._error


# ---------------------------------------------------------------------------
# Reuse mock helpers from test_mcp_tools (duplicated here to be self-contained)
# ---------------------------------------------------------------------------


class _WhisperModelType:
    TINY = "TINY"
    BASE = "BASE"
    SMALL = "SMALL"
    MEDIUM = "MEDIUM"
    LARGE = "LARGE"


class _VADSensitivity:
    BALANCED = "BALANCED"


class _WhisperConfig:
    def __init__(self) -> None:
        self.model_type = _WhisperModelType.BASE
        self.language = "auto"
        self.print_progress = False
        self.word_timestamps = False
        self.model_path = ""


class _VADConfig:
    def __init__(self) -> None:
        self.speech_threshold = 0.5
        self.enable_adaptive_threshold = False

    @staticmethod
    def from_preset(sensitivity: Any) -> "_VADConfig":
        return _VADConfig()


class _WhisperASR:
    def __init__(self, config: Any = None) -> None:
        self._init_ok: bool = True
        self._error: str = ""

    def initialize(self) -> bool:
        return self._init_ok

    def transcribe_file(self, path: str) -> List[Any]:
        return []

    def transcribe_buffer(self, audio: Any) -> List[Any]:
        return []

    def get_last_inference_time_ms(self) -> int:
        return 0

    def get_last_error(self) -> str:
        return self._error


class _AudioCapture:
    def __init__(self) -> None:
        self._open: bool = False
        self._capturing: bool = False

    def open(self, device_id: int = -1, **kwargs: Any) -> bool:
        self._open = True
        return True

    def start(self, callback: Any) -> bool:
        self._capturing = True
        return True

    def stop(self) -> None:
        self._capturing = False

    def close(self) -> None:
        self._open = False

    def is_open(self) -> bool:
        return self._open

    def is_capturing(self) -> bool:
        return self._capturing

    @staticmethod
    def initialize() -> bool:
        return True

    @staticmethod
    def terminate() -> None:
        pass

    @staticmethod
    def get_devices() -> List[Any]:
        return []

    @staticmethod
    def get_default_input_device() -> int:
        return 0


class _VADSegmenter:
    def __init__(self, config: Any = None) -> None:
        pass

    def process_frame(self, audio: Any, vad_prob: float, callback: Any) -> None:
        pass

    def flush(self, callback: Any) -> None:
        pass


class _RNNoiseConfig:
    def __init__(self) -> None:
        self.enable_vad = False


class _RNNoise:
    def __init__(self, config: Any = None) -> None:
        pass

    def initialize(self, sample_rate: int, channels: int) -> bool:
        return True

    def process(self, audio: Any) -> None:
        pass

    def get_vad_probability(self) -> float:
        return 0.8


# ---------------------------------------------------------------------------
# Build the fake _ffvoice extension module
# ---------------------------------------------------------------------------


def _build_fake_ffvoice_extension() -> types.ModuleType:
    """Return a synthetic ``_ffvoice`` module carrying all mock classes."""
    mod = types.ModuleType("ffvoice._ffvoice")
    mod.WhisperModelType = _WhisperModelType  # type: ignore[attr-defined]
    mod.VADSensitivity = _VADSensitivity  # type: ignore[attr-defined]
    mod.WhisperConfig = _WhisperConfig  # type: ignore[attr-defined]
    mod.VADConfig = _VADConfig  # type: ignore[attr-defined]
    mod.RNNoiseConfig = _RNNoiseConfig  # type: ignore[attr-defined]
    mod.WhisperASR = _WhisperASR  # type: ignore[attr-defined]
    mod.AudioCapture = _AudioCapture  # type: ignore[attr-defined]
    mod.VADSegmenter = _VADSegmenter  # type: ignore[attr-defined]
    mod.RNNoise = _RNNoise  # type: ignore[attr-defined]
    # LiveCaptioner symbols
    mod.LiveCaptioner = _LiveCaptioner  # type: ignore[attr-defined]
    mod.LiveCaptionerConfig = _LiveCaptionerConfig  # type: ignore[attr-defined]
    mod.CaptionEvent = _CaptionEvent  # type: ignore[attr-defined]
    mod.CaptionEventType = _CaptionEventType  # type: ignore[attr-defined]
    # Extra stubs expected by ffvoice.__init__'s star-import
    for name in (
        "Word",
        "TranscriptionSegment",
        "SpeakerSegment",
        "AudioDeviceInfo",
        "WAVWriter",
        "FLACWriter",
        "AudioMixer",
        "RingBuffer",
        "merge_into_segments",
    ):
        setattr(mod, name, MagicMock(name=name))
    return mod


# ---------------------------------------------------------------------------
# Fixture
# ---------------------------------------------------------------------------


@pytest.fixture(autouse=True)
def mock_ffvoice_module():
    """
    Inject a synthetic ``ffvoice._ffvoice`` extension so the real
    ``ffvoice`` package (and its ``mcp`` sub-package) can be imported
    without the compiled .so file.
    """
    _clear_modules()

    fake_ext = _build_fake_ffvoice_extension()
    sys.modules["ffvoice._ffvoice"] = fake_ext

    import ffvoice as _ffvoice_pkg

    # Expose convenience attrs so tests can mutate them directly
    _ffvoice_pkg.WhisperASR = _WhisperASR  # type: ignore[attr-defined]
    _ffvoice_pkg.AudioCapture = _AudioCapture  # type: ignore[attr-defined]
    _ffvoice_pkg.VADSegmenter = _VADSegmenter  # type: ignore[attr-defined]
    _ffvoice_pkg.RNNoise = _RNNoise  # type: ignore[attr-defined]
    _ffvoice_pkg.RNNoiseConfig = _RNNoiseConfig  # type: ignore[attr-defined]
    _ffvoice_pkg.WhisperConfig = _WhisperConfig  # type: ignore[attr-defined]
    _ffvoice_pkg.VADConfig = _VADConfig  # type: ignore[attr-defined]
    _ffvoice_pkg.WhisperModelType = _WhisperModelType  # type: ignore[attr-defined]
    _ffvoice_pkg.VADSensitivity = _VADSensitivity  # type: ignore[attr-defined]
    _ffvoice_pkg._HAS_RNNOISE = True  # type: ignore[attr-defined]
    _ffvoice_pkg._HAS_LIVE_CAPTIONER = True  # type: ignore[attr-defined]
    _ffvoice_pkg.LiveCaptioner = _LiveCaptioner  # type: ignore[attr-defined]
    _ffvoice_pkg.LiveCaptionerConfig = _LiveCaptionerConfig  # type: ignore[attr-defined]
    _ffvoice_pkg.CaptionEvent = _CaptionEvent  # type: ignore[attr-defined]
    _ffvoice_pkg.CaptionEventType = _CaptionEventType  # type: ignore[attr-defined]

    yield _ffvoice_pkg

    _clear_modules()


def _clear_modules() -> None:
    for key in list(sys.modules.keys()):
        if key == "ffvoice" or key.startswith("ffvoice."):
            del sys.modules[key]


def _import_server() -> Any:
    """Import (or re-import) ffvoice.mcp.server with the mock in place."""
    for key in list(sys.modules.keys()):
        if key.startswith("ffvoice.mcp"):
            del sys.modules[key]
    return importlib.import_module("ffvoice.mcp.server")


# ---------------------------------------------------------------------------
# Tests: LiveCaptionSession (pipeline, no MCP dependency)
# ---------------------------------------------------------------------------


class TestLiveCaptionSession:
    """Test LiveCaptionSession directly without importing the MCP server."""

    def _make_session(self, captioner: Optional[_LiveCaptioner] = None) -> Any:
        from ffvoice.mcp._live_caption_pipeline import LiveCaptionSession

        capture = _AudioCapture()
        if captioner is None:
            captioner = _LiveCaptioner()
        return LiveCaptionSession(capture=capture, captioner=captioner, sample_rate=48000)

    def test_result_shape(self) -> None:
        session = self._make_session()
        with patch("time.sleep"):
            session.run(duration_seconds=1.0)
        result = session.get_result()
        assert "events" in result
        assert "duration_recorded_s" in result
        assert "utterances_completed" in result

    def test_empty_session_has_zero_events(self) -> None:
        session = self._make_session()
        with patch("time.sleep"):
            session.run(duration_seconds=1.0)
        result = session.get_result()
        assert result["events"] == []
        assert result["utterances_completed"] == 0

    def test_final_event_increments_utterances_completed(self) -> None:
        captioner = _LiveCaptioner()
        captioner._injected_events = [
            _make_final_event("hello", utterance_id=1),
            _make_final_event("world", utterance_id=2),
        ]
        session = self._make_session(captioner=captioner)
        with patch("time.sleep"):
            session.run(duration_seconds=2.0)
        result = session.get_result()
        assert result["utterances_completed"] == 2
        assert len(result["events"]) == 2

    def test_partial_event_does_not_increment_utterances_completed(self) -> None:
        captioner = _LiveCaptioner()
        captioner._injected_events = [
            _make_partial_event("hel..."),
            _make_final_event("hello"),
        ]
        session = self._make_session(captioner=captioner)
        with patch("time.sleep"):
            session.run(duration_seconds=2.0)
        result = session.get_result()
        assert result["utterances_completed"] == 1
        assert len(result["events"]) == 2
        assert result["events"][0]["type"] == "Partial"
        assert result["events"][1]["type"] == "Final"

    def test_event_fields_present(self) -> None:
        captioner = _LiveCaptioner()
        captioner._injected_events = [_make_final_event("test", utterance_id=5)]
        session = self._make_session(captioner=captioner)
        with patch("time.sleep"):
            session.run(duration_seconds=1.0)
        result = session.get_result()
        assert len(result["events"]) == 1
        ev = result["events"][0]
        for field in (
            "type",
            "utterance_id",
            "text",
            "utterance_start_ms",
            "utterance_end_ms",
            "confidence",
        ):
            assert field in ev, f"Missing field: {field}"
        assert ev["utterance_id"] == 5
        assert ev["text"] == "test"
        assert ev["type"] == "Final"

    def test_captioner_initialize_failure_raises(self) -> None:
        from ffvoice.mcp._live_caption_pipeline import LiveCaptionSession

        captioner = _LiveCaptioner()
        captioner._init_ok = False
        captioner._error = "model not found"
        capture = _AudioCapture()
        session = LiveCaptionSession(capture=capture, captioner=captioner)
        with pytest.raises(RuntimeError, match="LiveCaptioner.initialize"):
            session.run(duration_seconds=1.0)

    def test_captioner_start_failure_raises(self) -> None:
        from ffvoice.mcp._live_caption_pipeline import LiveCaptionSession

        captioner = _LiveCaptioner()
        captioner._start_ok = False
        capture = _AudioCapture()
        session = LiveCaptionSession(capture=capture, captioner=captioner)
        with pytest.raises(RuntimeError, match="LiveCaptioner.start"):
            session.run(duration_seconds=1.0)

    def test_device_open_failure_raises(self) -> None:
        from ffvoice.mcp._live_caption_pipeline import LiveCaptionSession

        class BadCapture(_AudioCapture):
            def open(self, **kwargs: Any) -> bool:
                return False

        session = LiveCaptionSession(
            capture=BadCapture(), captioner=_LiveCaptioner(), sample_rate=48000
        )
        with pytest.raises(RuntimeError, match="Failed to open"):
            session.run(duration_seconds=1.0)

    def test_captioner_stop_called_even_on_capture_error(self) -> None:
        """stop() must be called to flush even if capture.start() fails."""
        from ffvoice.mcp._live_caption_pipeline import LiveCaptionSession

        stop_calls: List[bool] = []

        class TrackingCaptioner(_LiveCaptioner):
            def stop(self) -> None:
                stop_calls.append(True)
                super().stop()

        class BadStartCapture(_AudioCapture):
            def start(self, callback: Any) -> bool:
                return False

        session = LiveCaptionSession(
            capture=BadStartCapture(),
            captioner=TrackingCaptioner(),
        )
        with pytest.raises(RuntimeError, match="Failed to start"):
            session.run(duration_seconds=1.0)

        assert stop_calls, "captioner.stop() must be called even when capture.start() fails"

    def test_feed_audio_called_from_audio_callback(self) -> None:
        """_audio_callback must forward data to captioner.feed_audio()."""
        from ffvoice.mcp._live_caption_pipeline import LiveCaptionSession

        feed_calls: List[int] = []

        class TrackingCaptioner(_LiveCaptioner):
            def feed_audio(self, audio_array: Any) -> int:
                feed_calls.append(len(audio_array))
                return len(audio_array)

        session = LiveCaptionSession(
            capture=_AudioCapture(),
            captioner=TrackingCaptioner(),
        )
        audio = np.zeros(256, dtype=np.int16)
        session._audio_callback(audio)
        assert feed_calls == [256]

    def test_caption_callback_thread_safety(self) -> None:
        """Events accumulated from multiple threads must all appear in get_result()."""
        from ffvoice.mcp._live_caption_pipeline import LiveCaptionSession

        session = LiveCaptionSession(
            capture=_AudioCapture(),
            captioner=_LiveCaptioner(),
        )

        barrier = threading.Barrier(5)

        def _fire(idx: int) -> None:
            barrier.wait()
            ev = _make_final_event(f"utterance {idx}", utterance_id=idx)
            session._caption_callback(ev)

        threads = [threading.Thread(target=_fire, args=(i,)) for i in range(5)]
        for t in threads:
            t.start()
        for t in threads:
            t.join()

        result = session.get_result()
        assert result["utterances_completed"] == 5
        assert len(result["events"]) == 5


# ---------------------------------------------------------------------------
# Tests: capture_and_caption MCP tool
# ---------------------------------------------------------------------------


class TestCaptureAndCaption:
    def test_duration_validation_too_short(self) -> None:
        srv = _import_server()
        with pytest.raises(ValueError, match="duration_seconds"):
            srv.capture_and_caption(duration_seconds=0)

    def test_duration_validation_too_long(self) -> None:
        srv = _import_server()
        with pytest.raises(ValueError, match="duration_seconds"):
            srv.capture_and_caption(duration_seconds=121)

    def test_duration_boundary_valid(self, mock_ffvoice_module: Any) -> None:
        srv = _import_server()
        with patch("time.sleep"):
            r1 = srv.capture_and_caption(duration_seconds=1)
            r2 = srv.capture_and_caption(duration_seconds=120)
        assert "error" not in r1
        assert "error" not in r2

    def test_successful_response_shape(self, mock_ffvoice_module: Any) -> None:
        srv = _import_server()
        with patch("time.sleep"):
            result = srv.capture_and_caption(duration_seconds=5)

        assert "error" not in result
        for field in ("events", "duration_recorded_s", "utterances_completed"):
            assert field in result, f"Missing field: {field}"
        assert isinstance(result["events"], list)
        assert isinstance(result["utterances_completed"], int)

    def test_live_captioner_not_available_returns_error(self, mock_ffvoice_module: Any) -> None:
        mock_ffvoice_module._HAS_LIVE_CAPTIONER = False
        srv = _import_server()
        result = srv.capture_and_caption(duration_seconds=5)
        assert "error" in result
        assert "ENABLE_WHISPER" in result["error"] or "LiveCaptioner" in result["error"]
        assert result["events"] == []

    def test_audio_initialize_failure_returns_error(self, mock_ffvoice_module: Any) -> None:
        # Replace the whole class with a subclass rather than mutating the
        # class-level staticmethod (which would pollute subsequent tests).
        class BadInitCapture(_AudioCapture):
            @staticmethod
            def initialize() -> None:
                raise RuntimeError("PortAudio exploded")

        mock_ffvoice_module.AudioCapture = BadInitCapture
        srv = _import_server()
        result = srv.capture_and_caption(duration_seconds=5)
        assert "error" in result
        assert result["events"] == []

    def test_captioner_init_failure_returns_error(self, mock_ffvoice_module: Any) -> None:
        class FailCaptioner(_LiveCaptioner):
            def initialize(self) -> bool:
                return False

            def get_last_error(self) -> str:
                return "model not found"

        mock_ffvoice_module.LiveCaptioner = FailCaptioner
        srv = _import_server()
        with patch("time.sleep"):
            result = srv.capture_and_caption(duration_seconds=3)
        assert "error" in result
        assert result["events"] == []

    def test_events_accumulate(self, mock_ffvoice_module: Any) -> None:
        """Injected Final events should appear in the tool response."""
        events_to_inject = [
            _make_final_event("hello", utterance_id=1),
            _make_final_event("world", utterance_id=2),
        ]

        class InjectingCaptioner(_LiveCaptioner):
            def __init__(self, config: Any = None) -> None:
                super().__init__(config)
                self._injected_events = events_to_inject

        mock_ffvoice_module.LiveCaptioner = InjectingCaptioner
        srv = _import_server()
        with patch("time.sleep"):
            result = srv.capture_and_caption(duration_seconds=5)

        assert "error" not in result
        assert result["utterances_completed"] == 2
        assert len(result["events"]) == 2
        texts = {ev["text"] for ev in result["events"]}
        assert texts == {"hello", "world"}

    def test_partial_interval_ms_forwarded(self, mock_ffvoice_module: Any) -> None:
        """The partial_interval_ms parameter must reach LiveCaptionerConfig."""
        captured_configs: List[_LiveCaptionerConfig] = []

        class CapturingCaptioner(_LiveCaptioner):
            def __init__(self, config: Any = None) -> None:
                super().__init__(config)
                captured_configs.append(config)

        mock_ffvoice_module.LiveCaptioner = CapturingCaptioner
        srv = _import_server()
        with patch("time.sleep"):
            srv.capture_and_caption(duration_seconds=2, partial_interval_ms=250)

        assert len(captured_configs) == 1
        assert captured_configs[0].partial_interval_ms == 250

    def test_model_path_env_var_forwarded(self, mock_ffvoice_module: Any, tmp_path: Any) -> None:
        """FFVOICE_MODEL_PATH must propagate into LiveCaptionerConfig.whisper.model_path."""
        import os

        captured_configs: List[_LiveCaptionerConfig] = []

        class CapturingCaptioner(_LiveCaptioner):
            def __init__(self, config: Any = None) -> None:
                super().__init__(config)
                captured_configs.append(config)

        mock_ffvoice_module.LiveCaptioner = CapturingCaptioner
        model_dir = str(tmp_path)
        with patch.dict(os.environ, {"FFVOICE_MODEL_PATH": model_dir}):
            srv = _import_server()
            with patch("time.sleep"):
                srv.capture_and_caption(duration_seconds=2)

        assert len(captured_configs) == 1
        expected_path = os.path.join(model_dir, "ggml-tiny.bin")
        assert captured_configs[0].whisper.model_path == expected_path

    def test_terminate_called_on_success(self, mock_ffvoice_module: Any) -> None:
        terminate_calls: List[bool] = []

        # Use a subclass so class-level mutation does not pollute other tests.
        class TrackingCapture(_AudioCapture):
            @staticmethod
            def terminate() -> None:
                terminate_calls.append(True)

        mock_ffvoice_module.AudioCapture = TrackingCapture
        srv = _import_server()
        with patch("time.sleep"):
            srv.capture_and_caption(duration_seconds=2)

        assert terminate_calls, "AudioCapture.terminate() must be called after capture"

    def test_terminate_called_on_captioner_init_failure(self, mock_ffvoice_module: Any) -> None:
        terminate_calls: List[bool] = []

        class TrackingCapture(_AudioCapture):
            @staticmethod
            def terminate() -> None:
                terminate_calls.append(True)

        class FailCaptioner(_LiveCaptioner):
            def initialize(self) -> bool:
                return False

        mock_ffvoice_module.AudioCapture = TrackingCapture
        mock_ffvoice_module.LiveCaptioner = FailCaptioner
        srv = _import_server()
        with patch("time.sleep"):
            srv.capture_and_caption(duration_seconds=3)

        assert terminate_calls, "AudioCapture.terminate() must be called even on failure"

    def test_device_id_forwarded(self, mock_ffvoice_module: Any) -> None:
        """device_id=-1 default and an explicit ID must both work without error."""
        srv = _import_server()
        with patch("time.sleep"):
            r1 = srv.capture_and_caption(duration_seconds=2, device_id=-1)
            r2 = srv.capture_and_caption(duration_seconds=2, device_id=3)
        assert "error" not in r1
        assert "error" not in r2


# ---------------------------------------------------------------------------
# Tests: _is_final helper
# ---------------------------------------------------------------------------


class TestIsFinal:
    def test_final_string(self) -> None:
        from ffvoice.mcp._live_caption_pipeline import _is_final

        ev = _CaptionEvent(event_type="Final")
        assert _is_final(ev) is True

    def test_partial_string(self) -> None:
        from ffvoice.mcp._live_caption_pipeline import _is_final

        ev = _CaptionEvent(event_type="Partial")
        assert _is_final(ev) is False

    def test_captiontype_final_string(self) -> None:
        from ffvoice.mcp._live_caption_pipeline import _is_final

        ev = _CaptionEvent(event_type="CaptionEventType.Final")
        assert _is_final(ev) is True

    def test_captiontype_partial_string(self) -> None:
        from ffvoice.mcp._live_caption_pipeline import _is_final

        ev = _CaptionEvent(event_type="CaptionEventType.Partial")
        assert _is_final(ev) is False


# ---------------------------------------------------------------------------
# Tests: ffvoice.__init__ exports for LiveCaptioner symbols
# ---------------------------------------------------------------------------


class TestInitExports:
    def test_live_captioner_in_all(self, mock_ffvoice_module: Any) -> None:
        import ffvoice

        for sym in ("LiveCaptioner", "LiveCaptionerConfig", "CaptionEvent", "CaptionEventType"):
            assert sym in ffvoice.__all__, f"{sym} must be in ffvoice.__all__"

    def test_has_live_captioner_flag(self, mock_ffvoice_module: Any) -> None:
        import ffvoice

        assert hasattr(ffvoice, "_HAS_LIVE_CAPTIONER")

    def test_live_captioner_accessible(self, mock_ffvoice_module: Any) -> None:
        import ffvoice

        assert ffvoice.LiveCaptioner is _LiveCaptioner
        assert ffvoice.LiveCaptionerConfig is _LiveCaptionerConfig

    def test_caption_event_type_accessible(self, mock_ffvoice_module: Any) -> None:
        import ffvoice

        assert ffvoice.CaptionEventType is _CaptionEventType
