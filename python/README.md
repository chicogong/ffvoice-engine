# ffvoice - Python Bindings

High-performance offline speech recognition library for Python, powered by C++ ffvoice-engine.

## Features

⚡ **High Performance**
- 3-10x faster than pure Python solutions
- C++ core with Python ease of use
- ~272MB memory footprint (vs 1-2GB for pure Python)

🔒 **Privacy First**
- 100% offline operation
- No data uploaded to cloud
- GDPR/HIPAA compliant

🎙️ **Complete Audio Pipeline**
- Real-time audio capture
- AI-powered noise reduction (RNNoise)
- Voice Activity Detection (VAD)
- Offline speech recognition (Whisper)
- Intelligent audio segmentation

🛠️ **Easy to Use**
- Simple Python API
- One-line installation (coming soon)
- Comprehensive examples

## Installation

### Prerequisites

**System Dependencies:**

macOS:
```bash
brew install ffmpeg portaudio flac cmake
```

Linux (Ubuntu/Debian):
```bash
sudo apt-get install libavcodec-dev libavformat-dev libavutil-dev \
                     libswresample-dev portaudio19-dev libflac-dev cmake
```

**Python Requirements:**
- Python 3.7 or later
- pip

### Install from Source

```bash
# Clone repository
git clone https://github.com/chicogong/ffvoice-engine.git
cd ffvoice-engine

# Install Python package
pip install .

# Or install in development mode
pip install -e .
```

### Build Options

The installation automatically enables:
- RNNoise noise suppression
- Whisper ASR

To customize the build:
```bash
# Set environment variables before pip install
export CMAKE_ARGS="-DENABLE_RNNOISE=ON -DENABLE_WHISPER=ON"
pip install .
```

## Quick Start

### Basic Transcription

```python
import ffvoice

# Configure Whisper ASR
config = ffvoice.WhisperConfig()
config.model_type = ffvoice.WhisperModelType.TINY
config.language = "auto"  # Auto-detect language

# Initialize ASR
asr = ffvoice.WhisperASR(config)
asr.initialize()

# Transcribe audio file
segments = asr.transcribe_file("audio.wav")

# Print results
for segment in segments:
    print(f"[{segment.start_ms}ms -> {segment.end_ms}ms]")
    print(f"  {segment.text} (confidence: {segment.confidence:.2f})")
```

### NumPy Array Support

```python
import ffvoice
import numpy as np

# Load audio as NumPy array
audio = np.zeros(48000, dtype=np.int16)  # 1 second at 48kHz

# Transcribe from NumPy array
config = ffvoice.WhisperConfig()
config.model_type = ffvoice.WhisperModelType.TINY
asr = ffvoice.WhisperASR(config)
asr.initialize()

segments = asr.transcribe_buffer(audio)
for segment in segments:
    print(segment.text)
```

### Real-time Audio Capture with Callback

```python
import ffvoice
import numpy as np

# Initialize audio capture
ffvoice.AudioCapture.initialize()

# List available devices
devices = ffvoice.AudioCapture.get_devices()
for device in devices:
    print(f"{device.id}: {device.name} (channels: {device.max_input_channels})")

# Create capture instance
capture = ffvoice.AudioCapture()
capture.open(sample_rate=48000, channels=1, frames_per_buffer=256)

# Define callback to process audio
def audio_callback(audio_array):
    """Receives NumPy array with audio samples"""
    print(f"Received {len(audio_array)} samples")
    # Process audio here...

# Start capture with callback
capture.start(audio_callback)

# ... capture runs in background ...

# Stop capture
capture.stop()
capture.close()
ffvoice.AudioCapture.terminate()
```

### Noise Reduction with NumPy

```python
import ffvoice
import numpy as np

# Configure RNNoise
config = ffvoice.RNNoiseConfig()
config.enable_vad = True

# Initialize noise reduction
rnnoise = ffvoice.RNNoise(config)
rnnoise.initialize(sample_rate=48000, channels=1)

# Process audio from NumPy array (in-place modification)
audio = np.random.randint(-1000, 1000, 256, dtype=np.int16)
rnnoise.process(audio)  # Audio array is modified in-place

# Get VAD probability
vad_prob = rnnoise.get_vad_probability()
print(f"Voice activity: {vad_prob:.2%}")
```

### Voice Activity Detection with Callbacks

```python
import ffvoice
import numpy as np

# Create VAD config with preset
config = ffvoice.VADConfig.from_preset(ffvoice.VADSensitivity.BALANCED)

# Initialize VAD segmenter
vad = ffvoice.VADSegmenter(config, sample_rate=48000)

# Define callback for complete segments
def segment_callback(segment_array):
    """Called when a complete speech segment is detected"""
    print(f"Speech segment: {len(segment_array)} samples")
    # Process or save the segment...

# Process audio frames with VAD
audio_frame = np.zeros(256, dtype=np.int16)
vad_prob = 0.8  # From RNNoise

vad.process_frame(audio_frame, vad_prob, segment_callback)

# Flush remaining audio at the end
vad.flush(segment_callback)

# Get statistics
stats = vad.get_statistics()
print(f"Average VAD: {stats['avg_vad_prob']:.2f}")
print(f"Speech ratio: {stats['speech_ratio']:.2%}")
print(f"Is in speech: {vad.is_in_speech()}")
```

### Writing Audio Files from NumPy

```python
import ffvoice
import numpy as np

# Create audio data
sample_rate = 48000
audio = np.random.randint(-1000, 1000, 48000, dtype=np.int16)

# Write WAV file
wav_writer = ffvoice.WAVWriter()
wav_writer.open("output.wav", sample_rate, channels=1)
samples_written = wav_writer.write_samples_array(audio)
wav_writer.close()
print(f"Wrote {samples_written} samples to WAV")

# Write FLAC file (with compression)
flac_writer = ffvoice.FLACWriter()
flac_writer.open("output.flac", sample_rate, channels=1, bits_per_sample=16, compression_level=5)
samples_written = flac_writer.write_samples_array(audio)
compression_ratio = flac_writer.get_compression_ratio()
flac_writer.close()
print(f"Wrote {samples_written} samples to FLAC (compression: {compression_ratio:.2f}x)")
```

## API Reference

### Core Classes

#### `WhisperASR`
Offline speech recognition using Whisper models.

**Methods:**
- `initialize()` - Load Whisper model
- `transcribe_file(filename)` - Transcribe audio file, returns list of `TranscriptionSegment`
- `transcribe_buffer(audio_array)` - Transcribe from NumPy array (int16, 1D), returns list of `TranscriptionSegment`
- `get_last_error()` - Get last error message (string)
- `get_last_inference_time_ms()` - Get inference time in milliseconds (int)
- `is_initialized()` - Check if model is loaded (bool)

#### `AudioCapture`
Real-time audio capture from microphone with callback support.

**Methods:**
- `open(sample_rate, channels, frames_per_buffer, device_index=-1)` - Open audio device
- `start(callback)` - Start capture with Python callback receiving NumPy arrays
- `stop()` - Stop audio capture
- `close()` - Close audio device
- `is_open` - Property: check if device is open (bool)
- `is_capturing` - Property: check if currently capturing (bool)
- `sample_rate` - Property: get sample rate (int)
- `channels` - Property: get number of channels (int)

**Static Methods:**
- `initialize()` - Initialize PortAudio system
- `terminate()` - Terminate PortAudio system
- `get_devices()` - Get list of `AudioDeviceInfo` objects
- `get_default_input_device()` - Get default input device info

#### `RNNoise`
AI-powered noise reduction with NumPy support.

**Methods:**
- `initialize(sample_rate, channels)` - Initialize processor
- `process(audio_array)` - Process NumPy array in-place (modifies input)
- `reset()` - Reset internal state
- `get_vad_probability()` - Get VAD probability 0.0-1.0 (float)

#### `VADSegmenter`
Voice activity detection and intelligent segmentation with callbacks.

**Methods:**
- `process_frame(audio_array, vad_prob, callback)` - Process audio frame with callback for complete segments
- `flush(callback)` - Flush remaining audio with callback
- `reset()` - Reset segmenter state
- `is_in_speech()` - Check if currently in speech (bool)
- `get_buffer_size()` - Get current buffer size in samples (int)
- `get_current_threshold()` - Get adaptive threshold (float)
- `get_statistics()` - Get VAD statistics as dict: `{'avg_vad_prob': float, 'speech_ratio': float}`

#### `WAVWriter`
Write audio to WAV files with NumPy support.

**Methods:**
- `open(filename, sample_rate, channels)` - Open WAV file for writing
- `write_samples_array(audio_array)` - Write NumPy array to file, returns samples written (int)
- `close()` - Close file and finalize headers
- `is_open` - Property: check if file is open (bool)
- `total_samples` - Property: get total samples written (int)

#### `FLACWriter`
Write audio to FLAC files with compression and NumPy support.

**Methods:**
- `open(filename, sample_rate, channels, bits_per_sample, compression_level)` - Open FLAC file
  - `compression_level`: 0 (fastest) to 8 (best compression), default 5
- `write_samples_array(audio_array)` - Write NumPy array to file, returns samples written (int)
- `close()` - Close file and finalize
- `get_compression_ratio()` - Get compression ratio (float)
- `is_open` - Property: check if file is open (bool)
- `total_samples` - Property: get total samples written (int)

### Data Classes

#### `TranscriptionSegment`
Speech recognition result with timestamp.

**Fields:**
- `start_ms` - Start time in milliseconds (int)
- `end_ms` - End time in milliseconds (int)
- `text` - Transcribed text (string)
- `confidence` - Confidence score 0.0-1.0 (float)

#### `AudioDeviceInfo`
Audio device information.

**Fields:**
- `id` - Device ID (int)
- `name` - Device name (string)
- `max_input_channels` - Maximum input channels (int)
- `max_output_channels` - Maximum output channels (int)
- `supported_sample_rates` - List of supported sample rates (list of int)
- `is_default` - Is default device (bool)

### Configuration Classes

#### `WhisperConfig`
- `model_path` - Path to Whisper model file
- `language` - Language code ('en', 'zh', 'auto')
- `model_type` - Model size (TINY, BASE, SMALL, MEDIUM, LARGE)
- `n_threads` - Number of CPU threads
- `enable_performance_metrics` - Enable timing metrics

#### `AudioCaptureConfig`
- `sample_rate` - Sample rate in Hz (default: 48000)
- `channels` - Number of channels (default: 1)
- `frames_per_buffer` - Buffer size (default: 256)
- `device_index` - Audio device (-1 for default)

#### `VADConfig`
- `speech_threshold` - VAD probability threshold
- `min_speech_frames` - Min frames to start speech
- `min_silence_frames` - Min frames to end speech
- `enable_adaptive_threshold` - Enable adaptive adjustment
- `from_preset(sensitivity)` (static) - Create from preset

### Enums

#### `WhisperModelType`
- `TINY` - Fastest (~39MB, ~10x realtime)
- `BASE` - Balanced (~74MB, ~7x realtime)
- `SMALL` - Better accuracy (~244MB, ~3x realtime)
- `MEDIUM` - High accuracy (~769MB, ~1x realtime)
- `LARGE` - Best accuracy (~1550MB, <1x realtime)

#### `VADSensitivity`
- `VERY_SENSITIVE` - Detect very quiet speech (threshold=0.3)
- `SENSITIVE` - Detect quiet speech (threshold=0.4)
- `BALANCED` - Balanced detection (threshold=0.5)
- `CONSERVATIVE` - Avoid false positives (threshold=0.6)
- `VERY_CONSERVATIVE` - Only clear speech (threshold=0.7)

## Examples

See the [examples](examples/) directory for complete working examples:

- `basic_transcription.py` - Simple file transcription
- `realtime_transcription.py` - Real-time audio processing

## Performance

Benchmark on Apple M1 Pro (8 cores):

| Model | Speed (RTF) | Memory | Use Case |
|-------|-------------|--------|----------|
| TINY | ~10x | ~272MB | Real-time, fastest |
| BASE | ~7x | ~345MB | Balanced |
| SMALL | ~3x | ~512MB | Better accuracy |
| MEDIUM | ~1x | ~1.2GB | High quality |
| LARGE | <1x | ~2.1GB | Best quality |

*RTF = Real-time Factor (higher is faster)*

## Troubleshooting

### Import Error

If you get `ImportError: Failed to import ffvoice native module`:

1. Ensure all system dependencies are installed
2. Rebuild the package: `pip install --force-reinstall .`
3. Check CMake build logs for errors

### Audio Capture Issues

If audio capture fails:

1. List available devices: `ffvoice.AudioCapture.list_devices()`
2. Check device permissions (microphone access)
3. Try a different device index in `AudioCaptureConfig`

### Model Loading Issues

If Whisper model fails to load:

1. Check model file exists at the specified path
2. Ensure you have enough disk space
3. Verify model file is not corrupted

## Development

### Running Tests

```bash
# Install dev dependencies
pip install -e .[dev]

# Run tests
pytest python/tests -v
```

### Code Formatting

```bash
# Format code
black python/

# Check style
flake8 python/

# Type checking
mypy python/
```

## Contributing

Contributions are welcome! Please see [CONTRIBUTING.md](../CONTRIBUTING.md) for guidelines.

## License

MIT License - see [LICENSE](../LICENSE) for details.

## Links

- [GitHub Repository](https://github.com/chicogong/ffvoice-engine)
- [Documentation](https://github.com/chicogong/ffvoice-engine/tree/master/docs)
- [Issue Tracker](https://github.com/chicogong/ffvoice-engine/issues)
- [Market Research](../docs/market-research.md)
