"""
Unit tests for ``ffvoice.models`` — model file resolution + caching.

All network access is mocked; no real download ever happens.  These tests
exercise cache-dir resolution, the ``FFVOICE_MODEL_PATH`` override, the
"already cached" fast path, and the ``.tar.bz2`` extraction path.
"""

from __future__ import annotations

import io
import os
import pathlib
import sys
import tarfile
from typing import Any
from unittest.mock import patch

import pytest

# Ensure python/ is importable so `import ffvoice.models` resolves from disk.
_PYTHON_DIR = str(pathlib.Path(__file__).parent.parent)
if _PYTHON_DIR not in sys.path:
    sys.path.insert(0, _PYTHON_DIR)

from ffvoice import models  # noqa: E402

# ---------------------------------------------------------------------------
# A fake urlopen response usable as a context manager.
# ---------------------------------------------------------------------------


class _FakeResponse:
    def __init__(self, data: bytes) -> None:
        self._buf = io.BytesIO(data)

    def __enter__(self) -> "_FakeResponse":
        return self

    def __exit__(self, *exc: Any) -> None:
        return None

    def read(self, size: int = -1) -> bytes:
        return self._buf.read(size)


def _fake_urlopen(payload: bytes):
    def _open(url: str, *args: Any, **kwargs: Any) -> _FakeResponse:
        _open.calls.append(url)  # type: ignore[attr-defined]
        return _FakeResponse(payload)

    _open.calls = []  # type: ignore[attr-defined]
    return _open


# ---------------------------------------------------------------------------
# cache_dir()
# ---------------------------------------------------------------------------


class TestCacheDir:
    def test_ffvoice_cache_dir_env_wins(self, tmp_path: Any) -> None:
        target = tmp_path / "explicit"
        with patch.dict(os.environ, {"FFVOICE_CACHE_DIR": str(target)}, clear=False):
            result = models.cache_dir()
        assert result == target
        assert result.is_dir()

    def test_xdg_cache_home_used(self, tmp_path: Any) -> None:
        env = {"XDG_CACHE_HOME": str(tmp_path)}
        with patch.dict(os.environ, env, clear=False):
            os.environ.pop("FFVOICE_CACHE_DIR", None)
            result = models.cache_dir()
        assert result == tmp_path / "ffvoice"
        assert result.is_dir()

    def test_home_cache_fallback(self, tmp_path: Any) -> None:
        with patch.dict(os.environ, {}, clear=False):
            os.environ.pop("FFVOICE_CACHE_DIR", None)
            os.environ.pop("XDG_CACHE_HOME", None)
            with patch.object(models.Path, "home", return_value=tmp_path):
                result = models.cache_dir()
        assert result == tmp_path / ".cache" / "ffvoice"
        assert result.is_dir()


# ---------------------------------------------------------------------------
# ensure_whisper_model()
# ---------------------------------------------------------------------------


class TestEnsureWhisperModel:
    def test_invalid_name_raises(self) -> None:
        with pytest.raises(ValueError, match="Unknown Whisper model"):
            models.ensure_whisper_model("supermodel")

    def test_model_path_override_returned(self, tmp_path: Any) -> None:
        """FFVOICE_MODEL_PATH override is used when the file exists there."""
        model_file = tmp_path / "ggml-tiny.bin"
        model_file.write_bytes(b"weights")
        with patch.dict(os.environ, {"FFVOICE_MODEL_PATH": str(tmp_path)}, clear=False):
            result = models.ensure_whisper_model("tiny")
        assert result == str(model_file)

    def test_cached_file_returned_without_download(self, tmp_path: Any) -> None:
        """An already-cached model is returned with no network call."""
        cache = tmp_path / "cache"
        cached = cache / "whisper" / "ggml-base.bin"
        cached.parent.mkdir(parents=True)
        cached.write_bytes(b"cached-weights")

        opener = _fake_urlopen(b"should-not-be-used")
        with patch.dict(os.environ, {"FFVOICE_CACHE_DIR": str(cache)}, clear=False):
            os.environ.pop("FFVOICE_MODEL_PATH", None)
            with patch("urllib.request.urlopen", opener):
                result = models.ensure_whisper_model("base")

        assert result == str(cached)
        assert opener.calls == []  # type: ignore[attr-defined]

    def test_download_when_missing(self, tmp_path: Any) -> None:
        """A missing model is downloaded atomically into the cache."""
        cache = tmp_path / "cache"
        opener = _fake_urlopen(b"downloaded-weights")
        with patch.dict(os.environ, {"FFVOICE_CACHE_DIR": str(cache)}, clear=False):
            os.environ.pop("FFVOICE_MODEL_PATH", None)
            with patch("urllib.request.urlopen", opener):
                result = models.ensure_whisper_model("tiny")

        expected = cache / "whisper" / "ggml-tiny.bin"
        assert result == str(expected)
        assert expected.read_bytes() == b"downloaded-weights"
        assert len(opener.calls) == 1  # type: ignore[attr-defined]
        assert opener.calls[0].endswith("ggml-tiny.bin")  # type: ignore[attr-defined]
        # No leftover .part file
        assert not expected.with_name(expected.name + ".part").exists()


# ---------------------------------------------------------------------------
# ensure_diarization_models()
# ---------------------------------------------------------------------------


def _make_seg_archive() -> bytes:
    """Build a tiny synthetic .tar.bz2 mirroring the real segmentation archive."""
    buf = io.BytesIO()
    with tarfile.open(fileobj=buf, mode="w:bz2") as tar:
        payload = b"fake-onnx-segmentation-model"
        info = tarfile.TarInfo(name="sherpa-onnx-pyannote-segmentation-3-0/model.onnx")
        info.size = len(payload)
        tar.addfile(info, io.BytesIO(payload))
    return buf.getvalue()


class TestEnsureDiarizationModels:
    def test_download_and_extract(self, tmp_path: Any) -> None:
        """Segmentation .tar.bz2 is extracted; embedding .onnx is downloaded."""
        cache = tmp_path / "cache"
        archive_bytes = _make_seg_archive()

        def _opener(url: str, *args: Any, **kwargs: Any) -> _FakeResponse:
            _opener.calls.append(url)  # type: ignore[attr-defined]
            if url.endswith(".tar.bz2"):
                return _FakeResponse(archive_bytes)
            return _FakeResponse(b"fake-embedding-model")

        _opener.calls = []  # type: ignore[attr-defined]

        with patch.dict(os.environ, {"FFVOICE_CACHE_DIR": str(cache)}, clear=False):
            with patch("urllib.request.urlopen", _opener):
                seg, emb = models.ensure_diarization_models()

        assert pathlib.Path(seg).is_file()
        assert seg.endswith("model.onnx")
        assert pathlib.Path(seg).read_bytes() == b"fake-onnx-segmentation-model"
        assert pathlib.Path(emb).is_file()
        assert pathlib.Path(emb).read_bytes() == b"fake-embedding-model"
        # Archive itself should have been cleaned up after extraction.
        assert not (
            cache / "diarization" / "sherpa-onnx-pyannote-segmentation-3-0.tar.bz2"
        ).exists()
        assert len(_opener.calls) == 2  # type: ignore[attr-defined]

    def test_cached_models_skip_download(self, tmp_path: Any) -> None:
        """When both diarization models are already cached, no download runs."""
        cache = tmp_path / "cache"
        diar = cache / "diarization"
        seg = diar / "sherpa-onnx-pyannote-segmentation-3-0" / "model.onnx"
        seg.parent.mkdir(parents=True)
        seg.write_bytes(b"seg")
        emb = diar / "3dspeaker_speech_campplus_sv_en_voxceleb_16k.onnx"
        emb.write_bytes(b"emb")

        opener = _fake_urlopen(b"unused")
        with patch.dict(os.environ, {"FFVOICE_CACHE_DIR": str(cache)}, clear=False):
            with patch("urllib.request.urlopen", opener):
                result_seg, result_emb = models.ensure_diarization_models()

        assert result_seg == str(seg)
        assert result_emb == str(emb)
        assert opener.calls == []  # type: ignore[attr-defined]

    def test_multilingual_embedding_downloads_bilingual_model(self, tmp_path: Any) -> None:
        """embedding='multilingual' fetches the bilingual zh+en .onnx file."""
        cache = tmp_path / "cache"
        archive_bytes = _make_seg_archive()

        def _opener(url: str, *args: Any, **kwargs: Any) -> _FakeResponse:
            _opener.calls.append(url)  # type: ignore[attr-defined]
            if url.endswith(".tar.bz2"):
                return _FakeResponse(archive_bytes)
            return _FakeResponse(b"fake-multilingual-embedding")

        _opener.calls = []  # type: ignore[attr-defined]

        with patch.dict(os.environ, {"FFVOICE_CACHE_DIR": str(cache)}, clear=False):
            with patch("urllib.request.urlopen", _opener):
                _seg, emb = models.ensure_diarization_models(embedding="multilingual")

        assert emb.endswith("3dspeaker_speech_campplus_sv_zh_en_16k-common_advanced.onnx")
        assert pathlib.Path(emb).read_bytes() == b"fake-multilingual-embedding"
        # The embedding download URL must be the bilingual model.
        assert any("zh_en" in url for url in _opener.calls)  # type: ignore[attr-defined]

    def test_unknown_embedding_raises(self) -> None:
        """An unrecognised embedding key is rejected before any download."""
        with pytest.raises(ValueError, match="Unknown embedding model"):
            models.ensure_diarization_models(embedding="bogus")
