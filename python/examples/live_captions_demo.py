"""
Live captions demo — real-time speech captioning via the Python API.

Uses ``ffvoice.LiveCaptioner`` to capture audio from the microphone and
produce partial (in-progress) and final (end-of-utterance) captions in
real time — entirely on-device, nothing transmitted externally.

Requirements
------------
- ffvoice built with ``ENABLE_WHISPER=ON``
- A Whisper model file (default: ggml-tiny.bin) either on
  ``FFVOICE_MODEL_PATH`` or specified via ``--model-path``

Usage
-----
    python live_captions_demo.py
    python live_captions_demo.py --model-path /path/to/ggml-tiny.bin
    python live_captions_demo.py --device 2 --duration 30
    python live_captions_demo.py --list-devices

Controls
--------
    Ctrl+C  — stop early and show the session summary

Output format
-------------
    [P] (id=1) hel...           <- Partial caption (text in progress)
    [F] (id=1) hello world      <- Final caption (utterance complete)
"""

from __future__ import annotations

import argparse
import signal
import sys
import time
from typing import Any

import ffvoice

# ---------------------------------------------------------------------------
# Guard: LiveCaptioner must be present in this build
# ---------------------------------------------------------------------------


def _check_live_captioner_available() -> None:
    if not getattr(ffvoice, "_HAS_LIVE_CAPTIONER", False):
        print(
            "Error: LiveCaptioner is not available in this build.\n"
            "Rebuild ffvoice-engine with -DENABLE_WHISPER=ON and re-install.",
            file=sys.stderr,
        )
        sys.exit(1)


# ---------------------------------------------------------------------------
# Device listing
# ---------------------------------------------------------------------------


def list_devices() -> None:
    """Print all available audio input devices to stdout."""
    ffvoice.AudioCapture.initialize()
    try:
        devices = ffvoice.AudioCapture.get_devices()
        print("\nAvailable audio input devices:")
        for dev in devices:
            if dev.max_input_channels > 0:
                marker = " [DEFAULT]" if dev.is_default else ""
                print(f"  {dev.id}: {dev.name}{marker}")
                print(f"       Max input channels : {dev.max_input_channels}")
                print(f"       Sample rates       : {list(dev.supported_sample_rates)}")
    finally:
        ffvoice.AudioCapture.terminate()


# ---------------------------------------------------------------------------
# Session state (shared between callbacks and main thread)
# ---------------------------------------------------------------------------


class _State:
    """Mutable session counters accessed from multiple threads."""

    def __init__(self) -> None:
        self.partials_received: int = 0
        self.finals_received: int = 0
        self.start_time: float = 0.0


# ---------------------------------------------------------------------------
# Main demo
# ---------------------------------------------------------------------------


def run(
    model_path: str = "",
    device_id: int = -1,
    duration: int = 0,
    partial_interval_ms: int = 500,
    language: str = "auto",
) -> None:
    """
    Start a live captioning session.

    Args:
        model_path: Path to a ggml Whisper model file.  Empty string lets
            whisper.cpp auto-locate the model.
        device_id: PortAudio device ID (-1 = system default).
        duration: Maximum session duration in seconds.  0 means run until
            Ctrl+C is pressed.
        partial_interval_ms: How often (ms) partial captions are emitted
            while speech is ongoing.
        language: BCP-47 language code or ``'auto'`` for detection.
    """
    _check_live_captioner_available()

    state = _State()

    # ------------------------------------------------------------------
    # Caption callback — called from LiveCaptioner's worker thread
    # ------------------------------------------------------------------

    def on_caption(event: Any) -> None:
        is_final = event.type == ffvoice.CaptionEventType.Final
        tag = "[F]" if is_final else "[P]"
        uid = event.utterance_id

        if is_final:
            state.finals_received += 1
            conf_str = f"  (confidence={event.confidence:.2f})"
        else:
            state.partials_received += 1
            conf_str = ""

        print(f"{tag} (id={uid}) {event.text}{conf_str}", flush=True)

    # ------------------------------------------------------------------
    # Build LiveCaptionerConfig
    # ------------------------------------------------------------------

    config = ffvoice.LiveCaptionerConfig()
    config.sample_rate = 48000
    config.channels = 1
    config.partial_interval_ms = partial_interval_ms
    config.suppress_whisper_progress = True
    config.whisper.language = language
    config.whisper.print_progress = False
    if model_path:
        config.whisper.model_path = model_path

    # ------------------------------------------------------------------
    # Initialise captioner
    # ------------------------------------------------------------------

    captioner = ffvoice.LiveCaptioner(config)
    captioner.set_callback(on_caption)

    print("Initialising Whisper model (this may take a few seconds)…")
    if not captioner.initialize():
        print(f"Error: {captioner.get_last_error()}", file=sys.stderr)
        sys.exit(1)
    print("Model loaded.\n")

    # ------------------------------------------------------------------
    # Open audio device
    # ------------------------------------------------------------------

    ffvoice.AudioCapture.initialize()
    capture = ffvoice.AudioCapture()

    print(f"Opening audio device (device_id={device_id})…")
    if not capture.open(
        device_id=device_id,
        sample_rate=48000,
        channels=1,
        frames_per_buffer=256,
    ):
        print("Error: failed to open audio device.", file=sys.stderr)
        ffvoice.AudioCapture.terminate()
        sys.exit(1)
    print("Device opened.\n")

    # ------------------------------------------------------------------
    # Audio callback — feed raw PCM into the captioner's ring buffer
    # ------------------------------------------------------------------

    def on_audio(audio_array: Any) -> None:
        captioner.feed_audio(audio_array)

    # ------------------------------------------------------------------
    # Start everything
    # ------------------------------------------------------------------

    captioner.start()
    capture.start(on_audio)

    state.start_time = time.time()

    if duration > 0:
        print(f"Listening for {duration} seconds… (Ctrl+C to stop early)\n")
    else:
        print("Listening… (Ctrl+C to stop)\n")

    # Graceful Ctrl+C handling
    stop_requested = [False]

    def _handle_sigint(signum: int, frame: Any) -> None:
        stop_requested[0] = True

    signal.signal(signal.SIGINT, _handle_sigint)

    try:
        while not stop_requested[0]:
            if duration > 0 and (time.time() - state.start_time) >= duration:
                break
            time.sleep(0.1)
    finally:
        print("\nStopping…")

        if capture.is_capturing():
            capture.stop()
        if capture.is_open():
            capture.close()
        ffvoice.AudioCapture.terminate()

        # stop() flushes buffered audio and emits the final caption
        captioner.stop()

        elapsed = round(time.time() - state.start_time, 1)
        print(f"\n{'=' * 60}")
        print("Session summary")
        print(f"  Duration        : {elapsed} s")
        print(f"  Final captions  : {state.finals_received}")
        print(f"  Partial captions: {state.partials_received}")
        print(f"{'=' * 60}")


# ---------------------------------------------------------------------------
# CLI entry point
# ---------------------------------------------------------------------------


def main() -> None:
    parser = argparse.ArgumentParser(
        description="Live speech captioning demo using ffvoice.LiveCaptioner",
        formatter_class=argparse.RawDescriptionHelpFormatter,
        epilog=__doc__,
    )
    parser.add_argument(
        "--list-devices",
        action="store_true",
        help="List available audio devices and exit",
    )
    parser.add_argument(
        "--model-path",
        default="",
        metavar="PATH",
        help="Path to ggml Whisper model file (e.g. ggml-tiny.bin)",
    )
    parser.add_argument(
        "--device",
        type=int,
        default=-1,
        metavar="ID",
        help="PortAudio device ID (-1 = system default)",
    )
    parser.add_argument(
        "--duration",
        type=int,
        default=0,
        metavar="SECONDS",
        help="Maximum recording duration in seconds (0 = run until Ctrl+C)",
    )
    parser.add_argument(
        "--partial-interval",
        type=int,
        default=500,
        metavar="MS",
        help="Interval between partial caption attempts in ms (default 500)",
    )
    parser.add_argument(
        "--language",
        default="auto",
        metavar="LANG",
        help="Language code, e.g. 'en', 'zh', or 'auto' for detection",
    )

    args = parser.parse_args()

    if args.list_devices:
        list_devices()
        return

    run(
        model_path=args.model_path,
        device_id=args.device,
        duration=args.duration,
        partial_interval_ms=args.partial_interval,
        language=args.language,
    )


if __name__ == "__main__":
    main()
