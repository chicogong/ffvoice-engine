"""
Model file resolution with on-demand download + caching.

ffvoice's PyPI wheels do not ship the Whisper ``ggml-*.bin`` weights or the
ONNX diarization models — they are large and licence-encumbered to varying
degrees.  This module makes them "just work" for a plain ``pip install`` by
resolving each model file to a local path, downloading it to a per-user cache
directory on first use.

The module is deliberately stdlib-only (``urllib``, ``tarfile``, ``pathlib``)
so it adds no new hard dependency.

Cache layout::

    <cache_dir>/whisper/ggml-<name>.bin
    <cache_dir>/diarization/sherpa-onnx-pyannote-segmentation-3-0/model.onnx
    <cache_dir>/diarization/3dspeaker_speech_campplus_sv_en_voxceleb_16k.onnx

Downloads are atomic (written to a ``.part`` file, then renamed) so an
interrupted download never leaves a corrupt model in place.
"""

from __future__ import annotations

import os
import sys
import tarfile
import urllib.error
import urllib.request
from pathlib import Path
from typing import Tuple

# ---------------------------------------------------------------------------
# Constants — URLs mirror CMakeLists.txt's Whisper / diarization blocks.
# ---------------------------------------------------------------------------

# Whisper model weights (whisper.cpp ggml format), hosted on Hugging Face.
_WHISPER_BASE_URL = "https://huggingface.co/ggerganov/whisper.cpp/resolve/main"
_WHISPER_MODEL_NAMES = ("tiny", "base", "small", "medium", "large")

# Pyannote speaker-segmentation model — distributed as a .tar.bz2 archive that
# extracts to a directory containing ``model.onnx``.
_DIARIZATION_SEG_URL = (
    "https://github.com/k2-fsa/sherpa-onnx/releases/download/"
    "speaker-segmentation-models/sherpa-onnx-pyannote-segmentation-3-0.tar.bz2"
)
_DIARIZATION_SEG_DIRNAME = "sherpa-onnx-pyannote-segmentation-3-0"

# 3D-Speaker speaker-embedding extractors — plain .onnx files, keyed by the
# `embedding` argument of ensure_diarization_models().
_DIARIZATION_EMB_RELEASE = (
    "https://github.com/k2-fsa/sherpa-onnx/releases/download/" "speaker-recongition-models/"
)
_DIARIZATION_EMB_MODELS = {
    # English-tuned (VoxCeleb) — the default, smallest footprint.
    "en": "3dspeaker_speech_campplus_sv_en_voxceleb_16k.onnx",
    # Bilingual Chinese + English (3D-Speaker "advanced" common model) —
    # use this for Chinese or mixed-language audio.
    "multilingual": "3dspeaker_speech_campplus_sv_zh_en_16k-common_advanced.onnx",
}


# ---------------------------------------------------------------------------
# Cache directory
# ---------------------------------------------------------------------------


def cache_dir() -> Path:
    """
    Return the ffvoice model cache directory, creating it if needed.

    Resolution order:

    1. ``$FFVOICE_CACHE_DIR`` if set.
    2. ``$XDG_CACHE_HOME/ffvoice`` if ``$XDG_CACHE_HOME`` is set.
    3. ``~/.cache/ffvoice``.
    """
    override = os.environ.get("FFVOICE_CACHE_DIR")
    if override:
        path = Path(override)
    else:
        xdg = os.environ.get("XDG_CACHE_HOME")
        if xdg:
            path = Path(xdg) / "ffvoice"
        else:
            path = Path.home() / ".cache" / "ffvoice"
    path.mkdir(parents=True, exist_ok=True)
    return path


# ---------------------------------------------------------------------------
# Download helper
# ---------------------------------------------------------------------------


def _download(url: str, dest: Path) -> None:
    """
    Download *url* to *dest* atomically.

    The body is streamed to ``<dest>.part`` and renamed to *dest* only on a
    fully successful download, so a partial/failed download never leaves a
    corrupt file in the cache.

    Raises:
        RuntimeError: On any HTTP / network error, with a clear message.
    """
    dest.parent.mkdir(parents=True, exist_ok=True)
    part = dest.with_name(dest.name + ".part")
    print(f"Downloading {url} ...", file=sys.stderr, flush=True)
    try:
        with urllib.request.urlopen(url) as response:  # noqa: S310 (trusted URLs)
            with open(part, "wb") as fh:
                while True:
                    chunk = response.read(1 << 16)
                    if not chunk:
                        break
                    fh.write(chunk)
    except urllib.error.HTTPError as exc:
        part.unlink(missing_ok=True)
        raise RuntimeError(f"Failed to download {url}: HTTP {exc.code} {exc.reason}") from exc
    except urllib.error.URLError as exc:
        part.unlink(missing_ok=True)
        raise RuntimeError(f"Failed to download {url}: {exc.reason}") from exc
    except OSError as exc:
        part.unlink(missing_ok=True)
        raise RuntimeError(f"Failed to download {url}: {exc}") from exc
    part.replace(dest)


# ---------------------------------------------------------------------------
# Whisper
# ---------------------------------------------------------------------------


def ensure_whisper_model(name: str = "tiny") -> str:
    """
    Return the local path to the Whisper ``ggml-<name>.bin`` model.

    Resolution order:

    1. If ``$FFVOICE_MODEL_PATH`` is set and ``<that dir>/ggml-<name>.bin``
       exists, return it (preserves the existing override behaviour).
    2. If already cached under ``cache_dir()/whisper/``, return that.
    3. Otherwise download it from Hugging Face into the cache and return it.

    Args:
        name: Whisper model size — one of ``tiny``, ``base``, ``small``,
            ``medium``, ``large``.

    Raises:
        ValueError: If *name* is not a recognised model size.
        RuntimeError: If the download fails.
    """
    name = name.lower()
    if name not in _WHISPER_MODEL_NAMES:
        raise ValueError(
            f"Unknown Whisper model '{name}'. Valid names: {list(_WHISPER_MODEL_NAMES)}"
        )

    filename = f"ggml-{name}.bin"

    # (a) honour the FFVOICE_MODEL_PATH override when the file is present
    model_dir = os.environ.get("FFVOICE_MODEL_PATH", "")
    if model_dir:
        override_path = Path(model_dir) / filename
        if override_path.is_file():
            return str(override_path)

    # (b) cached copy
    cached = cache_dir() / "whisper" / filename
    if cached.is_file():
        return str(cached)

    # (c) download
    _download(f"{_WHISPER_BASE_URL}/{filename}", cached)
    return str(cached)


# ---------------------------------------------------------------------------
# Diarization
# ---------------------------------------------------------------------------


def ensure_diarization_models(embedding: str = "en") -> Tuple[str, str]:
    """
    Return ``(segmentation_onnx_path, embedding_onnx_path)`` for diarization.

    Downloads the pyannote segmentation model (a ``.tar.bz2`` archive that
    extracts to a directory containing ``model.onnx``) and a 3D-Speaker
    speaker-embedding model (a plain ``.onnx`` file) into
    ``cache_dir()/diarization/`` on first use.

    Args:
        embedding: Which speaker-embedding model to use — ``"en"`` (default,
            English-tuned VoxCeleb model) or ``"multilingual"`` (a bilingual
            Chinese + English model; use it for Chinese or mixed-language
            audio). Each model is cached independently.

    Raises:
        ValueError: If *embedding* is not a known model key.
        RuntimeError: If a download fails, or the segmentation archive does
            not contain the expected ``model.onnx``.
    """
    if embedding not in _DIARIZATION_EMB_MODELS:
        raise ValueError(
            f"Unknown embedding model '{embedding}'. "
            f"Valid values: {sorted(_DIARIZATION_EMB_MODELS)}"
        )
    diar_dir = cache_dir() / "diarization"
    diar_dir.mkdir(parents=True, exist_ok=True)

    # --- segmentation model (.tar.bz2 -> dir/model.onnx) ---------------
    seg_model = diar_dir / _DIARIZATION_SEG_DIRNAME / "model.onnx"
    if not seg_model.is_file():
        archive = diar_dir / "sherpa-onnx-pyannote-segmentation-3-0.tar.bz2"
        _download(_DIARIZATION_SEG_URL, archive)
        try:
            with tarfile.open(archive, "r:bz2") as tar:
                tar.extractall(diar_dir)  # noqa: S202 (trusted release archive)
        except (tarfile.TarError, OSError) as exc:
            archive.unlink(missing_ok=True)
            raise RuntimeError(
                f"Failed to extract diarization segmentation archive {archive}: {exc}"
            ) from exc
        archive.unlink(missing_ok=True)
        if not seg_model.is_file():
            raise RuntimeError(f"Segmentation archive did not contain expected model: {seg_model}")

    # --- embedding model (.onnx) ---------------------------------------
    emb_filename = _DIARIZATION_EMB_MODELS[embedding]
    emb_model = diar_dir / emb_filename
    if not emb_model.is_file():
        _download(_DIARIZATION_EMB_RELEASE + emb_filename, emb_model)

    return str(seg_model), str(emb_model)
