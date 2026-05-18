"""
Basic example of using ffvoice for audio transcription

This example demonstrates:
1. Loading and initializing Whisper ASR
2. Transcribing an audio file
3. Printing transcription results with timestamps

Usage:
    python basic_transcription.py [audio_file.wav]

The example also shows how to:
- Transcribe from a NumPy array (int16, 1D)
- Enable word-level timestamps
"""

import sys
import numpy as np
import ffvoice


def transcribe_file(audio_file: str) -> None:
    """Transcribe an audio file and print results."""

    # Configure Whisper ASR
    config = ffvoice.WhisperConfig()
    config.model_type = ffvoice.WhisperModelType.TINY  # fastest model
    config.language = "auto"  # auto-detect language
    config.n_threads = 4  # CPU threads for inference
    config.print_progress = False  # suppress verbose progress output
    config.enable_performance_metrics = True
    config.word_timestamps = True  # get per-word timestamps in each segment

    # Initialize ASR processor
    print("Initializing Whisper ASR...")
    asr = ffvoice.WhisperASR(config)

    if not asr.initialize():
        print(f"Failed to initialize Whisper: {asr.get_last_error()}")
        return

    print("Whisper ASR initialized successfully!")

    # Transcribe the audio file
    print(f"\nTranscribing: {audio_file}")
    segments = asr.transcribe_file(audio_file)

    # Print results
    print(f"\n{'='*60}")
    print(f"Transcription Results ({len(segments)} segments)")
    print(f"{'='*60}\n")

    for i, segment in enumerate(segments, 1):
        # segment.start_ms / end_ms are the correct attribute names
        start_sec = segment.start_ms / 1000.0
        end_sec = segment.end_ms / 1000.0
        duration = end_sec - start_sec

        print(f"Segment {i}:")
        print(f"  Time:       [{start_sec:.2f}s -> {end_sec:.2f}s] ({duration:.2f}s)")
        print(f"  Text:       {segment.text}")
        print(f"  Confidence: {segment.confidence:.2f}")

        # Print per-word timestamps when word_timestamps=True
        if segment.words:
            print("  Words:")
            for word in segment.words:
                print(
                    f"    {word.start_ms:>6}ms - {word.end_ms:<6}ms  "
                    f"'{word.text}'  p={word.probability:.2f}"
                )
        print()

    inference_time_ms = asr.get_last_inference_time_ms()
    print(f"Inference time: {inference_time_ms:.1f} ms")


def transcribe_numpy_buffer() -> None:
    """Demonstrate transcribing audio from a NumPy int16 array."""

    # Configure ASR; set input_sample_rate to match your buffer's sample rate
    config = ffvoice.WhisperConfig()
    config.model_type = ffvoice.WhisperModelType.TINY
    config.language = "auto"
    config.input_sample_rate = 16000  # Hz of the buffer passed to transcribe_buffer()

    asr = ffvoice.WhisperASR(config)
    if not asr.initialize():
        print(f"Failed to initialize Whisper: {asr.get_last_error()}")
        return

    # Create a dummy 1-second silent buffer (replace with real audio data)
    audio = np.zeros(16000, dtype=np.int16)  # 1 s of silence at 16 kHz

    print("\nTranscribing from NumPy buffer (16000 samples @ 16 kHz)...")
    segments = asr.transcribe_buffer(audio)

    for segment in segments:
        print(f"  [{segment.start_ms}ms -> {segment.end_ms}ms] {segment.text}")

    print(f"Done. Inference time: {asr.get_last_inference_time_ms()} ms")


def main() -> None:
    audio_file = sys.argv[1] if len(sys.argv) > 1 else "test_audio.wav"

    print("=" * 60)
    print("ffvoice Basic Transcription Example")
    print("=" * 60)

    try:
        transcribe_file(audio_file)
    except Exception as e:
        print(f"Transcription failed: {e}")

    # Also show the NumPy buffer path (no real audio file required)
    print("\n--- NumPy Buffer Example ---")
    try:
        transcribe_numpy_buffer()
    except Exception as e:
        print(f"Buffer transcription failed: {e}")


if __name__ == "__main__":
    main()
