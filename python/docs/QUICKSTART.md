# ffvoice Quick Start Guide

This guide will help you get started with ffvoice Python bindings in 5 minutes.

## Installation

### Prerequisites

**macOS:**
```bash
brew install ffmpeg portaudio flac cmake
```

**Linux (Ubuntu/Debian):**
```bash
sudo apt-get install libavcodec-dev libavformat-dev libavutil-dev \
                     libswresample-dev portaudio19-dev libflac-dev cmake
```

### Install ffvoice

```bash
# Clone and install
git clone https://github.com/chicogong/ffvoice-engine.git
cd ffvoice-engine
pip install .

# Optional: Install NumPy for advanced features
pip install numpy
```

## 5-Minute Tutorial

### 1. Transcribe an Audio File

The simplest use case - transcribe a WAV file:

```python
import ffvoice

# Initialize Whisper ASR
config = ffvoice.WhisperConfig()
config.model_type = ffvoice.WhisperModelType.TINY  # Fastest model
asr = ffvoice.WhisperASR(config)
asr.initialize()

# Transcribe
segments = asr.transcribe_file("your_audio.wav")

# Print results
for seg in segments:
    print(f"[{seg.start_ms/1000:.1f}s - {seg.end_ms/1000:.1f}s]")
    print(f"  {seg.text}")
```

**Output:**
```
[0.0s - 2.5s]
  Hello, this is a test recording.
[2.8s - 5.1s]
  The weather is nice today.
```

### 2. Process Audio with NumPy

Work with audio data as NumPy arrays:

```python
import ffvoice
import numpy as np
import wave

# Read WAV file into NumPy array
with wave.open("audio.wav", "rb") as wf:
    sample_rate = wf.getframerate()
    audio_data = wf.readframes(wf.getnframes())
    audio_array = np.frombuffer(audio_data, dtype=np.int16)

# Transcribe from array
config = ffvoice.WhisperConfig()
config.model_type = ffvoice.WhisperModelType.TINY
asr = ffvoice.WhisperASR(config)
asr.initialize()

segments = asr.transcribe_buffer(audio_array)
for seg in segments:
    print(seg.text)
```

### 3. Reduce Background Noise

Clean up noisy audio with RNNoise:

```python
import ffvoice
import numpy as np

# Load noisy audio
audio = np.fromfile("noisy_audio.raw", dtype=np.int16)

# Initialize RNNoise
config = ffvoice.RNNoiseConfig()
config.enable_vad = True
rnnoise = ffvoice.RNNoise(config)
rnnoise.initialize(sample_rate=48000, channels=1)

# Process in chunks (RNNoise works on 256-sample frames at 48kHz)
chunk_size = 256
for i in range(0, len(audio), chunk_size):
    chunk = audio[i:i+chunk_size]
    if len(chunk) == chunk_size:
        rnnoise.process(chunk)  # Modifies chunk in-place
        vad_prob = rnnoise.get_vad_probability()
        if vad_prob > 0.5:
            print(f"Speech detected at frame {i//chunk_size}")

# Save cleaned audio
audio.tofile("clean_audio.raw")
```

### 4. Detect Voice Activity

Use VAD to find when someone is speaking:

```python
import ffvoice
import numpy as np

# Create VAD config
config = ffvoice.VADConfig.from_preset(ffvoice.VADSensitivity.BALANCED)
vad = ffvoice.VADSegmenter(config, sample_rate=48000)

# Callback for complete speech segments
def on_segment(segment_array):
    duration_ms = len(segment_array) / 48000 * 1000
    print(f"Speech segment: {duration_ms:.0f}ms ({len(segment_array)} samples)")
    # Save or process segment here...

# Process audio
audio = np.fromfile("audio.raw", dtype=np.int16)
chunk_size = 256
vad_prob = 0.8  # From RNNoise or other VAD

for i in range(0, len(audio), chunk_size):
    chunk = audio[i:i+chunk_size]
    if len(chunk) == chunk_size:
        vad.process_frame(chunk, vad_prob, on_segment)

# Don't forget to flush remaining audio
vad.flush(on_segment)
```

### 5. Real-time Microphone Capture

Capture and process audio from your microphone:

```python
import ffvoice
import time

# Initialize PortAudio
ffvoice.AudioCapture.initialize()

# List available devices
print("Available microphones:")
devices = ffvoice.AudioCapture.get_devices()
for dev in devices:
    if dev.max_input_channels > 0:
        marker = " [DEFAULT]" if dev.is_default else ""
        print(f"  {dev.id}: {dev.name}{marker}")

# Open default microphone
capture = ffvoice.AudioCapture()
capture.open(sample_rate=48000, channels=1, frames_per_buffer=256)

# Callback receives NumPy arrays
def audio_callback(audio_array):
    print(f"Received {len(audio_array)} samples")
    # Process audio here...

# Start capture
print("\nRecording... (press Ctrl+C to stop)")
capture.start(audio_callback)

try:
    time.sleep(5)  # Record for 5 seconds
except KeyboardInterrupt:
    print("Stopping...")

# Cleanup
capture.stop()
capture.close()
ffvoice.AudioCapture.terminate()
```

### 6. Write Audio to Files

Save NumPy arrays to WAV or FLAC files:

```python
import ffvoice
import numpy as np

# Generate or load audio
sample_rate = 48000
audio = np.random.randint(-1000, 1000, 48000, dtype=np.int16)  # 1 second

# Write WAV file
wav = ffvoice.WAVWriter()
wav.open("output.wav", sample_rate, channels=1)
wav.write_samples_array(audio)
wav.close()
print(f"Wrote WAV: {wav.total_samples} samples")

# Write FLAC file (compressed)
flac = ffvoice.FLACWriter()
flac.open("output.flac", sample_rate, channels=1, bits_per_sample=16, compression_level=5)
flac.write_samples_array(audio)
compression = flac.get_compression_ratio()
flac.close()
print(f"Wrote FLAC: {flac.total_samples} samples (compression: {compression:.1f}x)")
```

## Complete Pipeline Example

Putting it all together - real-time transcription:

```python
import ffvoice
import numpy as np

class SimpleTranscriber:
    def __init__(self):
        # Setup RNNoise
        self.rnnoise = ffvoice.RNNoise(ffvoice.RNNoiseConfig())
        self.rnnoise.initialize(48000, 1)

        # Setup VAD
        vad_config = ffvoice.VADConfig.from_preset(ffvoice.VADSensitivity.BALANCED)
        self.vad = ffvoice.VADSegmenter(vad_config, 48000)

        # Setup Whisper
        config = ffvoice.WhisperConfig()
        config.model_type = ffvoice.WhisperModelType.TINY
        self.asr = ffvoice.WhisperASR(config)
        self.asr.initialize()

        # Setup audio capture
        ffvoice.AudioCapture.initialize()
        self.capture = ffvoice.AudioCapture()
        self.capture.open(48000, 1, 256)

    def on_segment(self, segment_array):
        """Transcribe complete speech segments"""
        segments = self.asr.transcribe_buffer(segment_array)
        for seg in segments:
            print(f"→ {seg.text}")

    def on_audio(self, audio_array):
        """Process each audio frame"""
        # Denoise
        self.rnnoise.process(audio_array)

        # Get VAD and segment
        vad_prob = self.rnnoise.get_vad_probability()
        self.vad.process_frame(audio_array, vad_prob, self.on_segment)

    def run(self, duration=10):
        """Record and transcribe for specified duration"""
        import time

        print(f"Recording for {duration} seconds...")
        self.capture.start(self.on_audio)
        time.sleep(duration)
        self.capture.stop()

        # Flush remaining audio
        self.vad.flush(self.on_segment)

        # Cleanup
        self.capture.close()
        ffvoice.AudioCapture.terminate()

# Run it!
transcriber = SimpleTranscriber()
transcriber.run(duration=10)
```

## Next Steps

- Check out the [complete examples](../examples/) directory
- Read the [API Reference](../README.md#api-reference) for all available options
- See [Performance Benchmarks](../README.md#performance) for model comparison
- Try the [Jupyter notebook tutorial](tutorials/ffvoice_tutorial.ipynb) (coming soon)

## Troubleshooting

### "Model file not found"
The Whisper model is downloaded automatically on first use. Make sure you have:
- Internet connection for initial download
- ~40MB free disk space for TINY model
- Write permission in the models directory

### "No audio devices found"
Check that:
- Your microphone is connected
- System has microphone permissions enabled
- PortAudio is installed: `brew install portaudio` (macOS)

### "Import Error: Failed to import ffvoice"
Reinstall with force rebuild:
```bash
pip install --force-reinstall --no-cache-dir .
```

## Support

- GitHub Issues: https://github.com/chicogong/ffvoice-engine/issues
- Documentation: https://github.com/chicogong/ffvoice-engine/tree/master/docs
