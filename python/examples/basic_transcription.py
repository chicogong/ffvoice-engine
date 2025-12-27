"""
Basic example of using ffvoice for audio transcription

This example demonstrates:
1. Loading and initializing Whisper ASR
2. Transcribing an audio file
3. Printing transcription results with timestamps
"""

import ffvoice


def main():
    # Configure Whisper ASR
    config = ffvoice.WhisperConfig()
    config.model_type = ffvoice.WhisperModelType.TINY  # Use tiny model for speed
    config.language = "auto"  # Auto-detect language
    config.n_threads = 4  # Number of CPU threads
    config.print_progress = True
    config.enable_performance_metrics = True

    # Initialize Whisper processor
    print("Initializing Whisper ASR...")
    asr = ffvoice.WhisperASR(config)

    if not asr.initialize():
        print(f"Failed to initialize Whisper: {asr.get_last_error()}")
        return

    print("Whisper ASR initialized successfully!")

    # Transcribe an audio file
    audio_file = "test_audio.wav"  # Replace with your audio file

    try:
        print(f"\nTranscribing: {audio_file}")
        segments = asr.transcribe_file(audio_file)

        # Print results
        print(f"\n{'='*60}")
        print(f"Transcription Results ({len(segments)} segments)")
        print(f"{'='*60}\n")

        for i, segment in enumerate(segments, 1):
            start_sec = segment.start_time_ms / 1000.0
            end_sec = segment.end_time_ms / 1000.0
            duration = end_sec - start_sec

            print(f"Segment {i}:")
            print(f"  Time: [{start_sec:.2f}s -> {end_sec:.2f}s] (duration: {duration:.2f}s)")
            print(f"  Text: {segment.text}")
            print()

        # Print performance metrics
        inference_time_ms = asr.get_last_inference_time_ms()
        print(f"Inference time: {inference_time_ms:.1f} ms")

    except Exception as e:
        print(f"Transcription failed: {e}")


if __name__ == "__main__":
    main()
