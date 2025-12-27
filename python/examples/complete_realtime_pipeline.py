#!/usr/bin/env python3
"""
Complete Real-time Speech Recognition Pipeline

This example demonstrates the full ffvoice pipeline:
1. AudioCapture - Capture audio from microphone
2. RNNoise - Reduce background noise and get VAD probability
3. VADSegmenter - Segment speech based on VAD
4. WhisperASR - Transcribe speech segments

Requirements:
- pip install numpy
- Microphone access
- Whisper model (downloaded automatically)
"""

import ffvoice
import numpy as np
import time
import sys


class RealtimeTranscriber:
    """Complete real-time transcription pipeline"""

    def __init__(self, model_type=ffvoice.WhisperModelType.TINY):
        self.sample_rate = 48000
        self.channels = 1
        self.frames_per_buffer = 256

        # Initialize components
        print("Initializing components...")

        # 1. RNNoise for noise reduction
        rnnoise_config = ffvoice.RNNoiseConfig()
        rnnoise_config.enable_vad = True
        self.rnnoise = ffvoice.RNNoise(rnnoise_config)
        self.rnnoise.initialize(self.sample_rate, self.channels)
        print("✓ RNNoise initialized")

        # 2. VAD Segmenter for intelligent speech segmentation
        vad_config = ffvoice.VADConfig.from_preset(ffvoice.VADSensitivity.BALANCED)
        self.vad = ffvoice.VADSegmenter(vad_config, self.sample_rate)
        print("✓ VADSegmenter initialized")

        # 3. Whisper ASR for transcription
        whisper_config = ffvoice.WhisperConfig()
        whisper_config.model_type = model_type
        whisper_config.language = "auto"
        self.asr = ffvoice.WhisperASR(whisper_config)
        print(f"✓ Loading Whisper {ffvoice.WhisperASR.get_model_type_name(model_type)} model...")
        if not self.asr.initialize():
            print(f"Error: {self.asr.get_last_error()}")
            sys.exit(1)
        print("✓ Whisper ASR initialized")

        # 4. Audio Capture
        ffvoice.AudioCapture.initialize()
        self.capture = ffvoice.AudioCapture()
        print("✓ AudioCapture initialized")

        # Statistics
        self.total_frames = 0
        self.total_segments = 0
        self.start_time = time.time()

    def list_devices(self):
        """List available audio devices"""
        print("\nAvailable audio devices:")
        devices = ffvoice.AudioCapture.get_devices()
        for device in devices:
            default_marker = " [DEFAULT]" if device.is_default else ""
            print(f"  {device.id}: {device.name}{default_marker}")
            print(f"      Input channels: {device.max_input_channels}")
            print(f"      Sample rates: {device.supported_sample_rates[:3]}...")
        print()

    def segment_callback(self, segment_array):
        """Called when VAD detects a complete speech segment"""
        self.total_segments += 1

        # Get VAD statistics
        stats = self.vad.get_statistics()
        avg_vad = stats['avg_vad_prob']
        speech_ratio = stats['speech_ratio']

        print(f"\n[Segment {self.total_segments}] {len(segment_array)} samples")
        print(f"  VAD: {avg_vad:.2f}, Speech ratio: {speech_ratio:.1%}")

        # Transcribe segment
        print("  Transcribing...", end=" ", flush=True)
        start = time.time()

        try:
            segments = self.asr.transcribe_buffer(segment_array)
            inference_time = self.asr.get_last_inference_time_ms()

            if segments:
                for seg in segments:
                    print(f"\n  → \"{seg.text}\"")
                    print(f"    [{seg.start_ms}ms - {seg.end_ms}ms, confidence: {seg.confidence:.2f}]")
                print(f"    Inference: {inference_time}ms")
            else:
                print("(no speech detected)")

        except Exception as e:
            print(f"Error: {e}")

    def audio_callback(self, audio_array):
        """Called for each audio frame from microphone"""
        self.total_frames += 1

        # Step 1: Noise reduction (in-place)
        self.rnnoise.process(audio_array)

        # Step 2: Get VAD probability
        vad_prob = self.rnnoise.get_vad_probability()

        # Step 3: Feed to VAD segmenter
        # The segmenter will call segment_callback when a complete segment is ready
        self.vad.process_frame(audio_array, vad_prob, self.segment_callback)

        # Print status every 100 frames (~0.5s at 256 samples/frame @ 48kHz)
        if self.total_frames % 100 == 0:
            elapsed = time.time() - self.start_time
            is_speech = "🎤 SPEECH" if self.vad.is_in_speech() else "🔇 silence"
            print(f"\rFrames: {self.total_frames}, VAD: {vad_prob:.2f}, {is_speech}, "
                  f"Buffer: {self.vad.get_buffer_size()} samples, "
                  f"Time: {elapsed:.1f}s", end="", flush=True)

    def start(self, device_index=-1):
        """Start real-time transcription"""
        print(f"\nOpening audio device (device_index={device_index})...")
        self.capture.open(
            sample_rate=self.sample_rate,
            channels=self.channels,
            frames_per_buffer=self.frames_per_buffer,
            device_index=device_index
        )

        print(f"✓ Device opened: {self.sample_rate}Hz, {self.channels} channel(s)")
        print("\nStarting real-time transcription...")
        print("Speak into your microphone! (Press Ctrl+C to stop)\n")

        # Start capture with callback
        self.capture.start(self.audio_callback)

        try:
            # Keep running until interrupted
            while True:
                time.sleep(0.1)

        except KeyboardInterrupt:
            print("\n\nStopping...")

    def stop(self):
        """Stop transcription and cleanup"""
        # Flush any remaining audio in VAD buffer
        print("Flushing VAD buffer...")
        self.vad.flush(self.segment_callback)

        # Stop capture
        self.capture.stop()
        self.capture.close()
        ffvoice.AudioCapture.terminate()

        # Print statistics
        elapsed = time.time() - self.start_time
        print(f"\n\n{'='*60}")
        print(f"Session Statistics:")
        print(f"  Duration: {elapsed:.1f}s")
        print(f"  Total frames: {self.total_frames}")
        print(f"  Total segments: {self.total_segments}")
        print(f"  Avg frames/segment: {self.total_frames/max(1,self.total_segments):.1f}")
        print(f"{'='*60}")


def main():
    """Main entry point"""
    print("="*60)
    print("ffvoice Real-time Speech Recognition")
    print("="*60)

    # Parse command line arguments
    device_index = -1  # Use default device
    model_type = ffvoice.WhisperModelType.TINY  # Use tiny model for speed

    if len(sys.argv) > 1:
        if sys.argv[1] == "--list-devices":
            ffvoice.AudioCapture.initialize()
            transcriber = RealtimeTranscriber(model_type)
            transcriber.list_devices()
            ffvoice.AudioCapture.terminate()
            return

        try:
            device_index = int(sys.argv[1])
        except ValueError:
            print(f"Usage: {sys.argv[0]} [device_index] [model_type]")
            print(f"       {sys.argv[0]} --list-devices")
            print(f"\nModel types: TINY, BASE, SMALL, MEDIUM, LARGE")
            sys.exit(1)

    if len(sys.argv) > 2:
        model_name = sys.argv[2].upper()
        model_type = getattr(ffvoice.WhisperModelType, model_name, ffvoice.WhisperModelType.TINY)

    # Create and run transcriber
    transcriber = RealtimeTranscriber(model_type)

    try:
        transcriber.start(device_index)
    finally:
        transcriber.stop()


if __name__ == "__main__":
    main()
