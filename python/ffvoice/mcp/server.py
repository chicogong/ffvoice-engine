"""
ffvoice MCP server — exposes offline speech-recognition tools to AI agents.

All processing is fully on-device; no audio or transcription data is
ever transmitted to any external service.
"""

from __future__ import annotations

import os
from typing import Any, Dict, List, Optional

from mcp.server.fastmcp import FastMCP

import ffvoice
from ffvoice.mcp._pipeline import CaptureSession

# ---------------------------------------------------------------------------
# Server instance
# ---------------------------------------------------------------------------

mcp: FastMCP = FastMCP(
    name="ffvoice",
    instructions=(
        "Offline speech recognition tools powered by ffvoice-engine. "
        "All audio processing happens locally on this device — nothing is "
        "transmitted to any external server."
    ),
)

# ---------------------------------------------------------------------------
# Helpers
# ---------------------------------------------------------------------------

_MODEL_NAME_MAP: Dict[str, Any] = {
    "tiny": ffvoice.WhisperModelType.TINY,
    "base": ffvoice.WhisperModelType.BASE,
    "small": ffvoice.WhisperModelType.SMALL,
    "medium": ffvoice.WhisperModelType.MEDIUM,
    "large": ffvoice.WhisperModelType.LARGE,
}


def _resolve_model_path(model_name: str) -> str:
    """
    Return an explicit model path when FFVOICE_MODEL_PATH is set.

    If the env-var is not set, return an empty string so WhisperConfig
    leaves model_path blank and whisper.cpp auto-locates the model.
    """
    model_dir = os.environ.get("FFVOICE_MODEL_PATH", "")
    if model_dir:
        return os.path.join(model_dir, f"ggml-{model_name}.bin")
    return ""


def _build_asr(model_name: str, language: str, word_timestamps: bool) -> ffvoice.WhisperASR:
    """Construct and initialise a WhisperASR instance; raise on failure."""
    model_type = _MODEL_NAME_MAP.get(model_name.lower(), ffvoice.WhisperModelType.BASE)
    config = ffvoice.WhisperConfig()
    config.model_type = model_type
    config.language = language
    config.print_progress = False
    config.word_timestamps = word_timestamps
    model_path = _resolve_model_path(model_name.lower())
    if model_path:
        config.model_path = model_path

    asr = ffvoice.WhisperASR(config)
    if not asr.initialize():
        raise RuntimeError(f"WhisperASR initialisation failed: {asr.get_last_error()}")
    return asr


def _segment_to_dict(seg: Any, include_words: bool) -> Dict[str, Any]:
    """Serialise a TranscriptionSegment to a plain dict."""
    d: Dict[str, Any] = {
        "start_ms": seg.start_ms,
        "end_ms": seg.end_ms,
        "text": seg.text,
        "confidence": seg.confidence,
    }
    if include_words and seg.words:
        d["words"] = [
            {
                "start_ms": w.start_ms,
                "end_ms": w.end_ms,
                "text": w.text,
                "probability": w.probability,
            }
            for w in seg.words
        ]
    return d


# ---------------------------------------------------------------------------
# Tool 1: transcribe_file
# ---------------------------------------------------------------------------


@mcp.tool()
def transcribe_file(
    path: str,
    language: str = "auto",
    model: str = "base",
    word_timestamps: bool = False,
) -> Dict[str, Any]:
    """
    Transcribe a local audio file to text using offline Whisper ASR.

    Processing is fully on-device — no audio is sent anywhere.
    Supports WAV, FLAC, and any format that whisper.cpp can read.

    Args:
        path: Absolute or relative path to the audio file.
        language: BCP-47 language code ('en', 'zh', 'fr', …) or 'auto' for
            automatic language detection.
        model: Whisper model size — one of 'tiny', 'base', 'small', 'medium',
            'large'.  Larger models are more accurate but slower.
        word_timestamps: When True, each segment includes per-word timing in
            the 'words' field.

    Returns:
        On success::

            {
                "segments": [
                    {
                        "start_ms": int,
                        "end_ms": int,
                        "text": str,
                        "confidence": float,
                        "words": [...]   # only when word_timestamps=True
                    },
                    ...
                ],
                "inference_ms": int,
                "model_used": str,
                "language": str
            }

        On failure::

            {"error": "<message>", "segments": []}
    """
    if not os.path.exists(path):
        return {"error": f"File not found: {path}", "segments": []}

    model_lower = model.lower()
    if model_lower not in _MODEL_NAME_MAP:
        return {
            "error": f"Unknown model '{model}'. Valid values: {list(_MODEL_NAME_MAP.keys())}",
            "segments": [],
        }

    try:
        asr = _build_asr(model_lower, language, word_timestamps)
    except RuntimeError as exc:
        return {"error": str(exc), "segments": []}

    try:
        raw_segments = asr.transcribe_file(path)
    except RuntimeError as exc:
        return {"error": str(exc), "segments": []}

    inference_ms = asr.get_last_inference_time_ms()
    segments: List[Dict[str, Any]] = [
        _segment_to_dict(seg, word_timestamps) for seg in raw_segments
    ]

    return {
        "segments": segments,
        "inference_ms": inference_ms,
        "model_used": model_lower,
        "language": language,
    }


# ---------------------------------------------------------------------------
# Tool 2: list_audio_devices
# ---------------------------------------------------------------------------


@mcp.tool()
def list_audio_devices() -> Dict[str, Any]:
    """
    List all audio input/output devices available on this machine.

    Uses PortAudio under the hood; no data leaves the device.

    Returns::

        {
            "devices": [
                {
                    "id": int,
                    "name": str,
                    "max_input_channels": int,
                    "max_output_channels": int,
                    "supported_sample_rates": [int, ...],
                    "is_default": bool
                },
                ...
            ],
            "default_input_device_id": int
        }
    """
    try:
        ffvoice.AudioCapture.initialize()
    except Exception as exc:
        return {"error": f"AudioCapture.initialize() failed: {exc}", "devices": []}

    try:
        raw_devices = ffvoice.AudioCapture.get_devices()
        default_id: Optional[int] = None
        try:
            default_id = ffvoice.AudioCapture.get_default_input_device()
        except Exception:
            pass

        devices: List[Dict[str, Any]] = [
            {
                "id": dev.id,
                "name": dev.name,
                "max_input_channels": dev.max_input_channels,
                "max_output_channels": dev.max_output_channels,
                "supported_sample_rates": list(dev.supported_sample_rates),
                "is_default": dev.is_default,
            }
            for dev in raw_devices
        ]
    finally:
        ffvoice.AudioCapture.terminate()

    return {
        "devices": devices,
        "default_input_device_id": default_id,
    }


# ---------------------------------------------------------------------------
# Tool 3: capture_and_transcribe
# ---------------------------------------------------------------------------


@mcp.tool()
def capture_and_transcribe(
    duration_seconds: float,
    device_id: int = -1,
    denoise: bool = True,
) -> Dict[str, Any]:
    """
    Capture live microphone audio for a given duration and transcribe it.

    Everything runs locally on this device — no audio is transmitted anywhere.
    VAD (Voice Activity Detection) segments the stream into speech chunks which
    are each transcribed individually by Whisper ASR.

    Args:
        duration_seconds: How many seconds to record (must be between 1 and 120).
        device_id: PortAudio device ID to use.  Pass -1 (default) for the
            system default input device.
        denoise: Apply RNNoise AI denoising before VAD/ASR when True.
            If RNNoise is not available in this build the tool automatically
            falls back to energy-based VAD and sets denoise_applied=False in
            the response.

    Returns::

        {
            "segments": [
                {"start_ms": int, "end_ms": int, "text": str, "confidence": float},
                ...
            ],
            "duration_recorded_s": float,
            "total_inference_ms": int,
            "denoise_applied": bool,
            "speech_segments_found": int
        }

    Raises:
        ValueError: If duration_seconds is outside [1, 120].
    """
    if not (1 <= duration_seconds <= 120):
        raise ValueError(f"duration_seconds must be between 1 and 120, got {duration_seconds}")

    # Build VAD
    vad_config = ffvoice.VADConfig.from_preset(ffvoice.VADSensitivity.BALANCED)
    vad_config.enable_adaptive_threshold = True
    vad = ffvoice.VADSegmenter(vad_config)

    # Build ASR (TINY model for real-time use; model path honoured via env var)
    config = ffvoice.WhisperConfig()
    config.model_type = ffvoice.WhisperModelType.TINY
    config.language = "auto"
    config.print_progress = False
    model_path = _resolve_model_path("tiny")
    if model_path:
        config.model_path = model_path
    asr = ffvoice.WhisperASR(config)
    if not asr.initialize():
        return {
            "error": f"WhisperASR initialisation failed: {asr.get_last_error()}",
            "segments": [],
            "duration_recorded_s": 0.0,
            "total_inference_ms": 0,
            "denoise_applied": False,
            "speech_segments_found": 0,
        }

    # Build RNNoise (optional)
    rnnoise = None
    if denoise and ffvoice._HAS_RNNOISE:
        try:
            rn_cfg = ffvoice.RNNoiseConfig()
            rn_cfg.enable_vad = True
            rnnoise = ffvoice.RNNoise(rn_cfg)
            if not rnnoise.initialize(48000, 1):
                rnnoise = None
        except Exception:
            rnnoise = None

    # PortAudio init
    try:
        ffvoice.AudioCapture.initialize()
    except Exception as exc:
        return {
            "error": f"AudioCapture.initialize() failed: {exc}",
            "segments": [],
            "duration_recorded_s": 0.0,
            "total_inference_ms": 0,
            "denoise_applied": False,
            "speech_segments_found": 0,
        }

    capture = ffvoice.AudioCapture()
    session = CaptureSession(
        capture=capture,
        vad=vad,
        asr=asr,
        rnnoise=rnnoise,
        sample_rate=48000,
    )

    import time

    t_start = time.time()
    try:
        session.run(duration_seconds=duration_seconds, device_id=device_id)
    except RuntimeError as exc:
        return {
            "error": str(exc),
            "segments": [],
            "duration_recorded_s": round(time.time() - t_start, 2),
            "total_inference_ms": 0,
            "denoise_applied": False,
            "speech_segments_found": 0,
        }
    finally:
        ffvoice.AudioCapture.terminate()

    duration_recorded = round(time.time() - t_start, 2)
    result = session.get_result()

    return {
        "segments": result["segments"],
        "duration_recorded_s": duration_recorded,
        "total_inference_ms": result["total_inference_ms"],
        "denoise_applied": result["denoise_applied"],
        "speech_segments_found": result["speech_segments_found"],
    }


# ---------------------------------------------------------------------------
# Entry point
# ---------------------------------------------------------------------------


def main() -> None:
    """Run the ffvoice MCP server over stdio transport."""
    mcp.run(transport="stdio")


if __name__ == "__main__":
    main()
