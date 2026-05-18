"""
Real-time audio capture and transcription example

This example demonstrates:
1. Listing audio devices and opening the microphone
2. Applying RNNoise noise reduction per frame
3. Using VADSegmenter callbacks to detect complete speech segments
4. Transcribing each segment with Whisper ASR

Usage:
    python realtime_transcription.py [device_id]
    python realtime_transcription.py --list-devices

Note: RNNoise is only available when the library was built with ENABLE_RNNOISE=ON.
      This example detects its presence at runtime and skips denoising gracefully.
"""

import sys
import time
import numpy as np
import ffvoice


def list_devices() -> None:
    """Print all available audio input devices."""
    ffvoice.AudioCapture.initialize()
    devices = ffvoice.AudioCapture.get_devices()
    print("\nAvailable audio devices:")
    for device in devices:
        if device.max_input_channels > 0:
            default_marker = " [DEFAULT]" if device.is_default else ""
            print(f"  {device.id}: {device.name}{default_marker}")
            print(f"      Input channels: {device.max_input_channels}")
    ffvoice.AudioCapture.terminate()


def main() -> None:
    # --- argument handling -----------------------------------------------
    if len(sys.argv) > 1 and sys.argv[1] == "--list-devices":
        list_devices()
        return

    device_id = -1  # -1 = PortAudio default
    if len(sys.argv) > 1:
        try:
            device_id = int(sys.argv[1])
        except ValueError:
            print(f"Usage: {sys.argv[0]} [device_id | --list-devices]")
            sys.exit(1)

    print("=" * 60)
    print("ffvoice Real-time Transcription Example")
    print("=" * 60)

    # --- pipeline configuration ------------------------------------------
    SAMPLE_RATE = 48000
    CHANNELS = 1
    FRAMES_PER_BUFFER = 256

    # RNNoise (optional — only available when built with ENABLE_RNNOISE=ON)
    rnnoise = None
    if hasattr(ffvoice, "RNNoise"):
        rnnoise_config = ffvoice.RNNoiseConfig()
        rnnoise_config.enable_vad = True
        rnnoise = ffvoice.RNNoise(rnnoise_config)
        if rnnoise.initialize(SAMPLE_RATE, CHANNELS):
            print("RNNoise initialized")
        else:
            print("RNNoise initialization failed — skipping denoising")
            rnnoise = None
    else:
        print("RNNoise not available in this build")

    # VAD Segmenter
    vad_config = ffvoice.VADConfig.from_preset(ffvoice.VADSensitivity.BALANCED)
    vad_config.enable_adaptive_threshold = True
    vad = ffvoice.VADSegmenter(vad_config)  # note: no sample_rate arg

    # Whisper ASR
    whisper_config = ffvoice.WhisperConfig()
    whisper_config.model_type = ffvoice.WhisperModelType.TINY
    whisper_config.language = "auto"
    whisper_config.enable_performance_metrics = True

    asr = ffvoice.WhisperASR(whisper_config)
    print("Loading Whisper TINY model...")
    if not asr.initialize():
        print(f"Failed to initialize Whisper: {asr.get_last_error()}")
        return
    print("Whisper ASR ready")

    # --- state shared between callbacks ----------------------------------
    state = {"total_frames": 0, "total_segments": 0, "start_time": time.time()}

    def segment_callback(segment_array: np.ndarray) -> None:
        """Called by VADSegmenter when a complete speech segment is detected."""
        state["total_segments"] += 1
        seg_n = state["total_segments"]

        # get_statistics() returns a tuple (avg_vad_prob, speech_ratio)
        avg_vad_prob, speech_ratio = vad.get_statistics()

        print(f"\n[Segment {seg_n}] {len(segment_array)} samples")
        print(f"  VAD: avg={avg_vad_prob:.2f}, speech_ratio={speech_ratio:.1%}")

        # Transcribe the speech segment
        print("  Transcribing...", end=" ", flush=True)
        try:
            segments = asr.transcribe_buffer(segment_array)
            inference_ms = asr.get_last_inference_time_ms()
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

    def audio_callback(audio_array: np.ndarray) -> None:
        """Called for each audio frame captured from the microphone."""
        state["total_frames"] += 1

        # Step 1: In-place noise reduction (modifies audio_array)
        if rnnoise is not None:
            rnnoise.process(audio_array)
            vad_prob = rnnoise.get_vad_probability()
        else:
            # Rough energy-based VAD fallback when RNNoise is absent
            energy = float(np.abs(audio_array).mean())
            vad_prob = min(1.0, energy / 3000.0)

        # Step 2: Feed frame to VAD; segment_callback fires on complete segments
        vad.process_frame(audio_array, vad_prob, segment_callback)

        # Print running status every 100 frames
        if state["total_frames"] % 100 == 0:
            elapsed = time.time() - state["start_time"]
            is_speech = "SPEECH" if vad.is_in_speech() else "silence"
            print(
                f"\rFrames: {state['total_frames']:>6}, "
                f"VAD: {vad_prob:.2f}, "
                f"{is_speech}, "
                f"buf={vad.get_buffer_size()} samples, "
                f"{elapsed:.1f}s elapsed",
                end="",
                flush=True,
            )

    # --- open device and start capture -----------------------------------
    ffvoice.AudioCapture.initialize()
    capture = ffvoice.AudioCapture()

    print(f"\nOpening audio device (device_id={device_id})...")
    # Note: the parameter is `device_id`, not `device_index`
    if not capture.open(
        device_id=device_id,
        sample_rate=SAMPLE_RATE,
        channels=CHANNELS,
        frames_per_buffer=FRAMES_PER_BUFFER,
    ):
        print("Failed to open audio device")
        ffvoice.AudioCapture.terminate()
        return

    print(f"Device opened: {SAMPLE_RATE} Hz, {CHANNELS} channel(s)")
    print("\nSpeak into your microphone! (Press Ctrl+C to stop)\n")

    # start() takes a Python callback; audio runs on a background C++ thread
    capture.start(audio_callback)

    try:
        while True:
            time.sleep(0.1)
    except KeyboardInterrupt:
        print("\n\nStopping...")
    finally:
        # Flush any remaining audio still buffered in the VAD segmenter
        print("Flushing VAD buffer...")
        vad.flush(segment_callback)

        capture.stop()
        capture.close()
        ffvoice.AudioCapture.terminate()

        # Summary
        elapsed = time.time() - state["start_time"]
        print(f"\n{'='*60}")
        print("Session Summary:")
        print(f"  Duration:        {elapsed:.1f} s")
        print(f"  Total frames:    {state['total_frames']}")
        print(f"  Total segments:  {state['total_segments']}")
        print(f"{'='*60}")


if __name__ == "__main__":
    main()
