#!/usr/bin/env python3
"""
Complete Real-time Speech Recognition Pipeline

This example demonstrates the full ffvoice pipeline:
  1. AudioCapture   - capture audio frames from the microphone
  2. RNNoise        - reduce background noise and get VAD probability
  3. VADSegmenter   - accumulate frames into complete speech segments (callback-based)
  4. WhisperASR     - offline transcription of each speech segment

Requirements:
    pip install numpy
    Microphone access
    A downloaded Whisper model (set model_path, or rely on the auto-locate default)

Usage:
    python complete_realtime_pipeline.py                  # default device, TINY model
    python complete_realtime_pipeline.py --list-devices   # list devices and exit
    python complete_realtime_pipeline.py 2                # use device id 2
    python complete_realtime_pipeline.py 2 SMALL          # device 2, SMALL model
"""

import sys
import time
import numpy as np
import ffvoice


class RealtimeTranscriber:
    """Complete real-time transcription pipeline."""

    def __init__(
        self, model_type: ffvoice.WhisperModelType = ffvoice.WhisperModelType.TINY
    ) -> None:
        self.sample_rate = 48000
        self.channels = 1
        self.frames_per_buffer = 256
        self.total_frames = 0
        self.total_segments = 0
        self.start_time = time.time()

        print("Initializing pipeline components...")

        # 1. RNNoise for noise reduction (optional — only in ENABLE_RNNOISE builds)
        self.rnnoise = None
        if hasattr(ffvoice, "RNNoise"):
            rnnoise_config = ffvoice.RNNoiseConfig()
            rnnoise_config.enable_vad = True
            self.rnnoise = ffvoice.RNNoise(rnnoise_config)
            if self.rnnoise.initialize(self.sample_rate, self.channels):
                print("  RNNoise initialized")
            else:
                print("  RNNoise initialization failed — skipping denoising")
                self.rnnoise = None
        else:
            print("  RNNoise not available in this build")

        # 2. VAD Segmenter
        #    Constructor signature: VADSegmenter(config)  — no sample_rate argument.
        vad_config = ffvoice.VADConfig.from_preset(ffvoice.VADSensitivity.BALANCED)
        vad_config.enable_adaptive_threshold = True
        self.vad = ffvoice.VADSegmenter(vad_config)
        print("  VADSegmenter initialized")

        # 3. Whisper ASR
        whisper_config = ffvoice.WhisperConfig()
        whisper_config.model_type = model_type
        whisper_config.language = "auto"
        whisper_config.enable_performance_metrics = True
        # word_timestamps adds per-word timing to each TranscriptionSegment.words
        whisper_config.word_timestamps = False

        model_name = ffvoice.WhisperASR.get_model_type_name(model_type)
        print(f"  Loading Whisper {model_name} model...")
        self.asr = ffvoice.WhisperASR(whisper_config)
        if not self.asr.initialize():
            raise RuntimeError(f"Whisper init failed: {self.asr.get_last_error()}")
        print("  Whisper ASR initialized")

        # 4. Audio Capture
        #    Must call AudioCapture.initialize() (static) before opening a device.
        ffvoice.AudioCapture.initialize()
        self.capture = ffvoice.AudioCapture()
        print("  AudioCapture ready")

    # ------------------------------------------------------------------
    # Device helpers
    # ------------------------------------------------------------------

    @staticmethod
    def list_devices() -> None:
        """Print all available audio input devices."""
        ffvoice.AudioCapture.initialize()
        devices = ffvoice.AudioCapture.get_devices()
        print("\nAvailable audio devices:")
        for device in devices:
            if device.max_input_channels > 0:
                default_marker = " [DEFAULT]" if device.is_default else ""
                print(f"  {device.id}: {device.name}{default_marker}")
                print(f"      Input channels:  {device.max_input_channels}")
                rates = device.supported_sample_rates
                print(f"      Sample rates:    {rates[:5]}")
        print()
        ffvoice.AudioCapture.terminate()

    # ------------------------------------------------------------------
    # Callbacks
    # ------------------------------------------------------------------

    def segment_callback(self, segment_array: np.ndarray) -> None:
        """Called by VADSegmenter when a complete speech segment is ready."""
        self.total_segments += 1

        # get_statistics() returns a tuple (avg_vad_prob, speech_ratio),
        # NOT a dict — unpack directly.
        avg_vad_prob, speech_ratio = self.vad.get_statistics()

        print(f"\n[Segment {self.total_segments}] {len(segment_array)} samples")
        print(f"  VAD: avg={avg_vad_prob:.2f}, speech_ratio={speech_ratio:.1%}")

        print("  Transcribing...", end=" ", flush=True)
        try:
            segments = self.asr.transcribe_buffer(segment_array)
            inference_ms = self.asr.get_last_inference_time_ms()

            if segments:
                for seg in segments:
                    print(f'\n  -> "{seg.text}"')
                    print(
                        f"     [{seg.start_ms}ms - {seg.end_ms}ms, "
                        f"confidence={seg.confidence:.2f}]"
                    )
                print(f"     Inference: {inference_ms} ms")
            else:
                print("(no speech detected)")
        except Exception as exc:
            print(f"Error: {exc}")

    def audio_callback(self, audio_array: np.ndarray) -> None:
        """Called for each captured audio frame from the microphone."""
        self.total_frames += 1

        # Step 1: In-place noise reduction
        if self.rnnoise is not None:
            self.rnnoise.process(audio_array)
            vad_prob = self.rnnoise.get_vad_probability()
        else:
            energy = float(np.abs(audio_array).mean())
            vad_prob = min(1.0, energy / 3000.0)

        # Step 2: Feed frame to VAD segmenter.
        #         segment_callback fires automatically on complete segments.
        self.vad.process_frame(audio_array, vad_prob, self.segment_callback)

        # Running status every 100 frames
        if self.total_frames % 100 == 0:
            elapsed = time.time() - self.start_time
            is_speech = "SPEECH" if self.vad.is_in_speech() else "silence"
            print(
                f"\rFrames: {self.total_frames:>6}, "
                f"VAD: {vad_prob:.2f}, {is_speech}, "
                f"buf={self.vad.get_buffer_size()} samples, "
                f"{elapsed:.1f}s",
                end="",
                flush=True,
            )

    # ------------------------------------------------------------------
    # Lifecycle
    # ------------------------------------------------------------------

    def start(self, device_id: int = -1) -> None:
        """Open the audio device and begin real-time transcription."""
        print(f"\nOpening audio device (device_id={device_id})...")

        # Parameter is `device_id` (int), NOT `device_index`
        if not self.capture.open(
            device_id=device_id,
            sample_rate=self.sample_rate,
            channels=self.channels,
            frames_per_buffer=self.frames_per_buffer,
        ):
            raise RuntimeError("Failed to open audio device")

        actual_rate = self.capture.get_sample_rate()
        actual_ch = self.capture.get_channels()
        print(f"Device opened: {actual_rate} Hz, {actual_ch} channel(s)")
        print("\nSpeak into your microphone! (Press Ctrl+C to stop)\n")

        # start(callback) — audio runs on a background C++ thread
        self.capture.start(self.audio_callback)

        try:
            while True:
                time.sleep(0.1)
        except KeyboardInterrupt:
            print("\n\nStopping...")

    def stop(self) -> None:
        """Flush remaining audio and shut down cleanly."""
        print("Flushing VAD buffer...")
        self.vad.flush(self.segment_callback)

        if self.capture.is_capturing():
            self.capture.stop()
        if self.capture.is_open():
            self.capture.close()
        ffvoice.AudioCapture.terminate()

        elapsed = time.time() - self.start_time
        print(f"\n{'='*60}")
        print("Session Statistics:")
        print(f"  Duration:        {elapsed:.1f} s")
        print(f"  Total frames:    {self.total_frames}")
        print(f"  Total segments:  {self.total_segments}")
        avg = self.total_frames / max(1, self.total_segments)
        print(f"  Avg frames/seg:  {avg:.1f}")
        print(f"{'='*60}")


# ----------------------------------------------------------------------
# Entry point
# ----------------------------------------------------------------------


def main() -> None:
    print("=" * 60)
    print("ffvoice Complete Real-time Pipeline")
    print("=" * 60)

    if len(sys.argv) > 1 and sys.argv[1] == "--list-devices":
        RealtimeTranscriber.list_devices()
        return

    device_id = -1
    model_type = ffvoice.WhisperModelType.TINY

    if len(sys.argv) > 1:
        try:
            device_id = int(sys.argv[1])
        except ValueError:
            print(f"Usage: {sys.argv[0]} [device_id] [MODEL_TYPE]")
            print(f"       {sys.argv[0]} --list-devices")
            print("Model types: TINY, BASE, SMALL, MEDIUM, LARGE")
            sys.exit(1)

    if len(sys.argv) > 2:
        model_name = sys.argv[2].upper()
        model_type = getattr(ffvoice.WhisperModelType, model_name, ffvoice.WhisperModelType.TINY)

    transcriber = RealtimeTranscriber(model_type)
    try:
        transcriber.start(device_id)
    finally:
        transcriber.stop()


if __name__ == "__main__":
    main()
