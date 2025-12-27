"""
Real-time audio capture and transcription example

This example demonstrates:
1. Capturing audio from microphone
2. Applying noise reduction with RNNoise
3. VAD-based intelligent segmentation
4. Real-time transcription with Whisper
"""

import ffvoice
import time


def main():
    print("=== Real-time Audio Transcription Demo ===\n")

    # 1. Configure audio capture
    capture_config = ffvoice.AudioCaptureConfig()
    capture_config.sample_rate = 48000
    capture_config.channels = 1
    capture_config.frames_per_buffer = 256

    # 2. Configure RNNoise (noise reduction)
    rnnoise_config = ffvoice.RNNoiseConfig()
    rnnoise_config.enable_vad = True

    # 3. Configure VAD segmenter
    vad_config = ffvoice.VADConfig.from_preset(ffvoice.VADSensitivity.BALANCED)
    vad_config.enable_adaptive_threshold = True

    # 4. Configure Whisper ASR
    whisper_config = ffvoice.WhisperConfig()
    whisper_config.model_type = ffvoice.WhisperModelType.TINY
    whisper_config.language = "auto"
    whisper_config.enable_performance_metrics = True

    # Initialize components
    print("Initializing audio capture...")
    capture = ffvoice.AudioCapture(capture_config)
    if not capture.initialize():
        print(f"Failed to initialize audio capture: {capture.get_last_error()}")
        return

    print("Initializing RNNoise...")
    rnnoise = ffvoice.RNNoise(rnnoise_config)
    if not rnnoise.initialize(48000, 1):
        print("Failed to initialize RNNoise")
        return

    print("Initializing VAD segmenter...")
    vad = ffvoice.VADSegmenter(vad_config)

    print("Initializing Whisper ASR...")
    asr = ffvoice.WhisperASR(whisper_config)
    if not asr.initialize():
        print(f"Failed to initialize Whisper: {asr.get_last_error()}")
        return

    print("\nAll components initialized successfully!")
    print("\nAvailable audio devices:")
    ffvoice.AudioCapture.list_devices()

    print("\n" + "="*60)
    print("Ready for real-time transcription!")
    print("Press Ctrl+C to stop...")
    print("="*60 + "\n")

    # Start recording
    if not capture.start_recording():
        print(f"Failed to start recording: {capture.get_last_error()}")
        return

    try:
        segment_count = 0

        while True:
            # Check if we have a complete segment
            if vad.has_complete_segment():
                segment_count += 1

                print(f"\n[Segment {segment_count}] Transcribing...")

                # Get VAD statistics
                avg_vad_prob, speech_ratio = vad.get_statistics()
                print(f"  VAD stats: avg_prob={avg_vad_prob:.2f}, "
                      f"speech_ratio={speech_ratio:.2%}, "
                      f"threshold={vad.get_current_threshold():.2f}")

                # In a real implementation, you would:
                # 1. Get the audio segment from VAD
                # 2. Transcribe it with Whisper
                # For now, this is just a demonstration structure

                print(f"  (Transcription would happen here)")

            # Small delay to avoid busy-waiting
            time.sleep(0.01)

    except KeyboardInterrupt:
        print("\n\nStopping...")

    finally:
        # Stop recording
        if capture.is_recording():
            capture.stop_recording()

        print("\nSession statistics:")
        print(f"  Total segments processed: {segment_count}")


if __name__ == "__main__":
    main()
