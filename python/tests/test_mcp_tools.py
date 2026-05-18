"""
Unit tests for the ffvoice MCP server tools.

All ffvoice native bindings are mocked — no compiled extension or audio
hardware is required to run these tests.

Strategy
--------
The real ``ffvoice`` package lives at ``python/ffvoice/`` and depends on the
compiled ``_ffvoice`` extension.  We inject ``python/`` into ``sys.path`` so
that the package directory is discoverable, then replace ``ffvoice._ffvoice``
with a synthetic module containing all the mock classes.  This lets Python
resolve ``ffvoice.mcp`` and ``ffvoice.mcp.server`` from disk while still
exercising the code with no C++ binary.
"""

from __future__ import annotations

import importlib
import os
import pathlib
import sys
import types
from typing import Any, List
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
# Helper factories
# ---------------------------------------------------------------------------


def _make_segment(
    start_ms: int = 0,
    end_ms: int = 1000,
    text: str = "hello world",
    confidence: float = 0.9,
) -> MagicMock:
    seg = MagicMock()
    seg.start_ms = start_ms
    seg.end_ms = end_ms
    seg.text = text
    seg.confidence = confidence
    seg.words = []
    return seg


def _make_word(start_ms: int, end_ms: int, text: str, probability: float = 0.95) -> MagicMock:
    w = MagicMock()
    w.start_ms = start_ms
    w.end_ms = end_ms
    w.text = text
    w.probability = probability
    return w


def _make_device(
    id: int = 0,
    name: str = "Built-in Microphone",
    max_input_channels: int = 2,
    max_output_channels: int = 0,
    supported_sample_rates: List[int] = None,
    is_default: bool = True,
) -> MagicMock:
    dev = MagicMock()
    dev.id = id
    dev.name = name
    dev.max_input_channels = max_input_channels
    dev.max_output_channels = max_output_channels
    dev.supported_sample_rates = supported_sample_rates or [44100, 48000]
    dev.is_default = is_default
    return dev


# ---------------------------------------------------------------------------
# Fake _ffvoice extension classes
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


class _RNNoiseConfig:
    def __init__(self) -> None:
        self.enable_vad = False


class _WhisperASR:
    def __init__(self, config: Any = None) -> None:
        self._segments: List[Any] = []
        self._inf_ms: int = 42
        self._error: str = ""
        self._init_ok: bool = True

    def initialize(self) -> bool:
        return self._init_ok

    def transcribe_file(self, path: str) -> List[Any]:
        return self._segments

    def transcribe_buffer(self, audio: Any) -> List[Any]:
        return self._segments

    def get_last_inference_time_ms(self) -> int:
        return self._inf_ms

    def get_last_error(self) -> str:
        return self._error

    @staticmethod
    def get_model_type_name(model_type: Any) -> str:
        return str(model_type)


class _AudioCapture:
    _open: bool = False
    _capturing: bool = False

    def __init__(self) -> None:
        self._open = False
        self._capturing = False

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

    def get_sample_rate(self) -> int:
        return 48000

    def get_channels(self) -> int:
        return 1

    @staticmethod
    def initialize() -> bool:
        return True

    @staticmethod
    def terminate() -> None:
        pass

    @staticmethod
    def get_devices() -> List[Any]:
        return [_make_device()]

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

    def is_in_speech(self) -> bool:
        return False

    def get_buffer_size(self) -> int:
        return 0


class _RNNoise:
    def __init__(self, config: Any = None) -> None:
        pass

    def initialize(self, sample_rate: int, channels: int) -> bool:
        return True

    def process(self, audio: Any) -> None:
        pass

    def get_vad_probability(self) -> float:
        return 0.8


def _build_fake_ffvoice_extension() -> types.ModuleType:
    """Return a synthetic ``_ffvoice`` module carrying the mock classes."""
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
    mod.RNNoiseConfig = _RNNoiseConfig  # type: ignore[attr-defined]
    # Extra stubs expected by ffvoice.__init__'s star-import
    for name in (
        "Word",
        "TranscriptionSegment",
        "AudioDeviceInfo",
        "WAVWriter",
        "FLACWriter",
        "AudioMixer",
        "RingBuffer",
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

    We rebuild the whole module hierarchy from scratch each time to avoid
    cross-test pollution.
    """
    # Remove any previously cached modules
    _clear_modules()

    fake_ext = _build_fake_ffvoice_extension()

    # Register the fake extension *before* importing ffvoice so that
    # ``from ._ffvoice import *`` inside ffvoice/__init__.py picks it up.
    sys.modules["ffvoice._ffvoice"] = fake_ext

    # Now import the real ffvoice package (which reads our fake extension)
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

    yield _ffvoice_pkg

    _clear_modules()


def _clear_modules() -> None:
    for key in list(sys.modules.keys()):
        if key == "ffvoice" or key.startswith("ffvoice."):
            del sys.modules[key]


# ---------------------------------------------------------------------------
# Import helper
# ---------------------------------------------------------------------------


def _import_server() -> Any:
    """Import (or re-import) ffvoice.mcp.server with the mock in place."""
    # server.py does ``import ffvoice`` at module level — the mock is already
    # installed in sys.modules by the fixture, so a fresh importlib call picks
    # it up correctly.
    for key in list(sys.modules.keys()):
        if key.startswith("ffvoice.mcp"):
            del sys.modules[key]
    return importlib.import_module("ffvoice.mcp.server")


# ---------------------------------------------------------------------------
# Tests: transcribe_file
# ---------------------------------------------------------------------------


class TestTranscribeFile:
    def test_file_not_found(self, tmp_path: Any) -> None:
        srv = _import_server()
        result = srv.transcribe_file(str(tmp_path / "nonexistent.wav"))
        assert "error" in result
        assert "not found" in result["error"].lower()
        assert result["segments"] == []

    def test_unknown_model(self, tmp_path: Any) -> None:
        audio_file = tmp_path / "audio.wav"
        audio_file.write_bytes(b"\x00" * 100)
        srv = _import_server()
        result = srv.transcribe_file(str(audio_file), model="supermodel")
        assert "error" in result
        assert "supermodel" in result["error"]

    def test_successful_transcription(self, tmp_path: Any, mock_ffvoice_module: Any) -> None:
        audio_file = tmp_path / "speech.wav"
        audio_file.write_bytes(b"\x00" * 100)

        seg = _make_segment(start_ms=0, end_ms=2000, text="hello world", confidence=0.95)

        class PatchedASR(_WhisperASR):
            def transcribe_file(self, path: str) -> List[Any]:
                return [seg]

            def get_last_inference_time_ms(self) -> int:
                return 123

        mock_ffvoice_module.WhisperASR = PatchedASR

        srv = _import_server()
        result = srv.transcribe_file(str(audio_file), model="tiny", language="en")

        assert "error" not in result
        assert len(result["segments"]) == 1
        assert result["segments"][0]["text"] == "hello world"
        assert result["segments"][0]["confidence"] == 0.95
        assert result["inference_ms"] == 123
        assert result["model_used"] == "tiny"
        assert result["language"] == "en"

    def test_word_timestamps_included(self, tmp_path: Any, mock_ffvoice_module: Any) -> None:
        audio_file = tmp_path / "speech.wav"
        audio_file.write_bytes(b"\x00" * 100)

        word = _make_word(0, 400, "hello")
        seg = _make_segment()
        seg.words = [word]

        class PatchedASR(_WhisperASR):
            def transcribe_file(self, path: str) -> List[Any]:
                return [seg]

        mock_ffvoice_module.WhisperASR = PatchedASR

        srv = _import_server()
        result = srv.transcribe_file(str(audio_file), model="base", word_timestamps=True)

        assert "error" not in result
        assert len(result["segments"]) == 1
        words = result["segments"][0].get("words", [])
        assert len(words) == 1
        assert words[0]["text"] == "hello"

    def test_asr_init_failure(self, tmp_path: Any, mock_ffvoice_module: Any) -> None:
        audio_file = tmp_path / "speech.wav"
        audio_file.write_bytes(b"\x00" * 100)

        class FailASR(_WhisperASR):
            def initialize(self) -> bool:
                return False

            def get_last_error(self) -> str:
                return "model not found"

        mock_ffvoice_module.WhisperASR = FailASR

        srv = _import_server()
        result = srv.transcribe_file(str(audio_file))
        assert "error" in result
        assert "model not found" in result["error"]

    def test_transcription_runtime_error(self, tmp_path: Any, mock_ffvoice_module: Any) -> None:
        audio_file = tmp_path / "speech.wav"
        audio_file.write_bytes(b"\x00" * 100)

        class ErrorASR(_WhisperASR):
            def transcribe_file(self, path: str) -> List[Any]:
                raise RuntimeError("transcription exploded")

        mock_ffvoice_module.WhisperASR = ErrorASR

        srv = _import_server()
        result = srv.transcribe_file(str(audio_file))
        assert "error" in result
        assert "transcription exploded" in result["error"]

    def test_model_path_env_var(self, tmp_path: Any, mock_ffvoice_module: Any) -> None:
        """FFVOICE_MODEL_PATH should be honoured."""
        audio_file = tmp_path / "speech.wav"
        audio_file.write_bytes(b"\x00" * 100)

        captured_configs: list = []

        class CapturingASR(_WhisperASR):
            def __init__(self, config: Any = None) -> None:
                super().__init__(config)
                captured_configs.append(config)

        mock_ffvoice_module.WhisperASR = CapturingASR
        model_dir = str(tmp_path)

        with patch.dict(os.environ, {"FFVOICE_MODEL_PATH": model_dir}):
            srv = _import_server()
            srv.transcribe_file(str(audio_file), model="tiny")

        assert len(captured_configs) == 1
        assert captured_configs[0].model_path == os.path.join(model_dir, "ggml-tiny.bin")


# ---------------------------------------------------------------------------
# Tests: list_audio_devices
# ---------------------------------------------------------------------------


class TestListAudioDevices:
    def test_returns_device_list(self, mock_ffvoice_module: Any) -> None:
        devices = [
            _make_device(id=0, name="Built-in Mic", is_default=True),
            _make_device(id=1, name="USB Headset", max_input_channels=1, is_default=False),
        ]
        mock_ffvoice_module.AudioCapture.get_devices = staticmethod(lambda: devices)
        mock_ffvoice_module.AudioCapture.get_default_input_device = staticmethod(lambda: 0)

        srv = _import_server()
        result = srv.list_audio_devices()

        assert "error" not in result
        assert len(result["devices"]) == 2
        assert result["devices"][0]["name"] == "Built-in Mic"
        assert result["devices"][0]["is_default"] is True
        assert result["default_input_device_id"] == 0

    def test_device_fields_present(self, mock_ffvoice_module: Any) -> None:
        dev = _make_device(
            id=3,
            name="Test Device",
            max_input_channels=2,
            max_output_channels=2,
            supported_sample_rates=[44100, 48000],
            is_default=False,
        )
        mock_ffvoice_module.AudioCapture.get_devices = staticmethod(lambda: [dev])

        srv = _import_server()
        result = srv.list_audio_devices()

        d = result["devices"][0]
        for field in (
            "id",
            "name",
            "max_input_channels",
            "max_output_channels",
            "supported_sample_rates",
            "is_default",
        ):
            assert field in d, f"Missing field: {field}"
        assert d["supported_sample_rates"] == [44100, 48000]

    def test_initialize_failure(self, mock_ffvoice_module: Any) -> None:
        def _bad_init() -> bool:
            raise RuntimeError("PortAudio exploded")

        mock_ffvoice_module.AudioCapture.initialize = staticmethod(_bad_init)

        srv = _import_server()
        result = srv.list_audio_devices()
        assert "error" in result

    def test_terminate_called_even_on_error(self, mock_ffvoice_module: Any) -> None:
        terminate_called: List[bool] = []

        def _ok_init() -> bool:
            return True

        def _bad_get_devices() -> List[Any]:
            raise RuntimeError("oops")

        def _terminate() -> None:
            terminate_called.append(True)

        mock_ffvoice_module.AudioCapture.initialize = staticmethod(_ok_init)
        mock_ffvoice_module.AudioCapture.get_devices = staticmethod(_bad_get_devices)
        mock_ffvoice_module.AudioCapture.terminate = staticmethod(_terminate)

        srv = _import_server()
        try:
            srv.list_audio_devices()
        except Exception:
            pass

        assert terminate_called, "AudioCapture.terminate() must be called even on error"


# ---------------------------------------------------------------------------
# Tests: capture_and_transcribe
# ---------------------------------------------------------------------------


class TestCaptureAndTranscribe:
    def test_duration_validation_too_short(self) -> None:
        srv = _import_server()
        with pytest.raises(ValueError, match="duration_seconds"):
            srv.capture_and_transcribe(duration_seconds=0.5)

    def test_duration_validation_too_long(self) -> None:
        srv = _import_server()
        with pytest.raises(ValueError, match="duration_seconds"):
            srv.capture_and_transcribe(duration_seconds=200.0)

    def test_duration_boundary_valid(self, mock_ffvoice_module: Any) -> None:
        """duration=1 and duration=120 are both valid."""
        srv = _import_server()
        with patch("time.sleep"):
            r1 = srv.capture_and_transcribe(duration_seconds=1.0)
            r2 = srv.capture_and_transcribe(duration_seconds=120.0)
        assert "error" not in r1
        assert "error" not in r2

    def test_successful_capture(self, mock_ffvoice_module: Any) -> None:
        srv = _import_server()
        with patch("time.sleep"):
            result = srv.capture_and_transcribe(duration_seconds=5.0)

        assert "error" not in result
        assert "segments" in result
        assert "duration_recorded_s" in result
        assert "total_inference_ms" in result
        assert "denoise_applied" in result
        assert "speech_segments_found" in result

    def test_denoise_false_skips_rnnoise(self, mock_ffvoice_module: Any) -> None:
        srv = _import_server()
        with patch("time.sleep"):
            result = srv.capture_and_transcribe(duration_seconds=2.0, denoise=False)

        assert result.get("denoise_applied") is False

    def test_rnnoise_unavailable_falls_back(self, mock_ffvoice_module: Any) -> None:
        mock_ffvoice_module._HAS_RNNOISE = False

        srv = _import_server()
        with patch("time.sleep"):
            result = srv.capture_and_transcribe(duration_seconds=2.0, denoise=True)

        assert result.get("denoise_applied") is False

    def test_asr_init_failure_returns_error(self, mock_ffvoice_module: Any) -> None:
        class FailASR(_WhisperASR):
            def initialize(self) -> bool:
                return False

            def get_last_error(self) -> str:
                return "no model"

        mock_ffvoice_module.WhisperASR = FailASR

        srv = _import_server()
        result = srv.capture_and_transcribe(duration_seconds=3.0)
        assert "error" in result
        assert "no model" in result["error"]

    def test_open_device_failure_returns_error(self, mock_ffvoice_module: Any) -> None:
        class BadCapture(_AudioCapture):
            def open(self, **kwargs: Any) -> bool:
                return False

        mock_ffvoice_module.AudioCapture = BadCapture

        srv = _import_server()
        result = srv.capture_and_transcribe(duration_seconds=3.0)
        assert "error" in result

    def test_speech_segments_found_count(self, mock_ffvoice_module: Any) -> None:
        """VAD flush delivers two segments; both should appear in the result."""
        seg1 = _make_segment(text="hello")
        seg2 = _make_segment(text="world")
        call_count = [0]

        class TriggeringVAD(_VADSegmenter):
            def flush(self, callback: Any) -> None:
                # Simulate two speech segments arriving at flush time
                audio = np.zeros(480, dtype=np.int16)
                callback(audio)
                callback(audio)

        class MultiASR(_WhisperASR):
            def transcribe_buffer(self, audio: Any) -> List[Any]:
                call_count[0] += 1
                return [seg1] if call_count[0] == 1 else [seg2]

        mock_ffvoice_module.VADSegmenter = TriggeringVAD
        mock_ffvoice_module.WhisperASR = MultiASR

        srv = _import_server()
        with patch("time.sleep"):
            result = srv.capture_and_transcribe(duration_seconds=3.0)

        assert result["speech_segments_found"] == 2
        assert len(result["segments"]) == 2


# ---------------------------------------------------------------------------
# Tests: _pipeline.CaptureSession (standalone, no mcp dependency)
# ---------------------------------------------------------------------------


class TestCaptureSession:
    """Test CaptureSession directly without importing the MCP server."""

    def _make_session(self, rnnoise: Any = None) -> Any:
        from ffvoice.mcp._pipeline import CaptureSession

        capture = MagicMock()
        capture.open.return_value = True
        capture.start.return_value = True
        capture.is_open.return_value = True
        capture.is_capturing.return_value = True

        asr = MagicMock()
        asr.transcribe_buffer.return_value = []
        asr.get_last_inference_time_ms.return_value = 10

        vad = MagicMock()
        vad.flush = MagicMock()

        return CaptureSession(capture=capture, vad=vad, asr=asr, rnnoise=rnnoise)

    def test_result_shape(self) -> None:
        session = self._make_session()
        with patch("time.sleep"):
            session.run(duration_seconds=1.0)
        result = session.get_result()
        assert "segments" in result
        assert "speech_segments_found" in result
        assert "total_inference_ms" in result
        assert "denoise_applied" in result

    def test_open_failure_raises(self) -> None:
        from ffvoice.mcp._pipeline import CaptureSession

        capture = MagicMock()
        capture.open.return_value = False

        session = CaptureSession(
            capture=capture,
            vad=MagicMock(),
            asr=MagicMock(),
            rnnoise=None,
        )
        with pytest.raises(RuntimeError, match="Failed to open"):
            session.run(duration_seconds=1.0)

    def test_flush_called_after_stop(self) -> None:
        session = self._make_session()
        with patch("time.sleep"):
            session.run(duration_seconds=1.0)
        session._vad.flush.assert_called_once()

    def test_no_rnnoise_uses_energy_vad(self) -> None:
        session = self._make_session(rnnoise=None)
        audio = np.zeros(256, dtype=np.int16)
        # Should not raise even without rnnoise
        session._audio_callback(audio)

    def test_segment_callback_accumulates(self) -> None:
        from ffvoice.mcp._pipeline import CaptureSession

        seg = _make_segment(text="test segment")

        capture = MagicMock()
        capture.open.return_value = True
        capture.start.return_value = True
        capture.is_open.return_value = True
        capture.is_capturing.return_value = True

        asr = MagicMock()
        asr.transcribe_buffer.return_value = [seg]
        asr.get_last_inference_time_ms.return_value = 50

        vad = MagicMock()

        session = CaptureSession(capture=capture, vad=vad, asr=asr, rnnoise=None)

        audio = np.zeros(480, dtype=np.int16)
        session._segment_callback(audio)

        result = session.get_result()
        assert result["speech_segments_found"] == 1
        assert result["segments"][0]["text"] == "test segment"
        assert result["total_inference_ms"] == 50


# ---------------------------------------------------------------------------
# Tests: module-level import guard
# ---------------------------------------------------------------------------


class TestImportGuards:
    def test_mcp_package_check(self) -> None:
        """ffvoice.mcp.__init__ must raise ImportError with helpful message
        when the 'mcp' SDK is absent."""
        # Remove any cached mcp.mcp.server modules
        for key in list(sys.modules.keys()):
            if key.startswith("ffvoice.mcp"):
                del sys.modules[key]

        # Block the mcp package by setting it to None in sys.modules
        saved: dict = {}
        for key in list(sys.modules.keys()):
            if key == "mcp" or key.startswith("mcp."):
                saved[key] = sys.modules.pop(key)

        sys.modules["mcp"] = None  # type: ignore[assignment]

        try:
            with pytest.raises(ImportError, match="pip install"):
                importlib.import_module("ffvoice.mcp")
        finally:
            # Restore mcp modules
            sys.modules.pop("mcp", None)
            sys.modules.update(saved)
            # Clean up ffvoice.mcp cache so next test starts fresh
            for key in list(sys.modules.keys()):
                if key.startswith("ffvoice.mcp"):
                    del sys.modules[key]
