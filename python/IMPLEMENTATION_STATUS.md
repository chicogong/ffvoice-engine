# ffvoice Python Bindings - Implementation Status

**Last Updated**: 2026-05-18
**Current Version**: v0.6.1

## Status Summary

All planned phases (1–8) are complete. The Python bindings are published to PyPI and support Linux x86_64, macOS ARM64, and Windows x86_64 with prebuilt wheels.

---

## Completed Phases

### Phase 1: pybind11 Integration
- pybind11 integrated via CMake FetchContent
- Python package structure: `python/ffvoice/__init__.py`, `python/tests/`, `python/examples/`, `src/python/`
- `setup.py` and `pyproject.toml` for `pip install` support
- `CMakeLists.txt` updated with `BUILD_PYTHON` option

### Phase 2–5: Core Class Bindings
All 13 original core C++ classes are fully bound:

| Class | Description |
|-------|-------------|
| `TranscriptionSegment` | Transcription result with `start_ms`, `end_ms`, `text`, `confidence` |
| `AudioDeviceInfo` | Audio device info (`id`, `name`, channels, sample rates, `is_default`) |
| `AudioCapture` | Real-time audio capture (PortAudio wrapper) |
| `WAVWriter` | WAV file writer |
| `FLACWriter` | FLAC lossless writer with `HasError()` status query (v0.6.1) |
| `RNNoise` | AI noise reduction (disabled on Windows) |
| `RNNoiseConfig` | RNNoise configuration |
| `VADSegmenter` | Voice activity detection segmenter |
| `VADConfig` | VAD configuration |
| `VADSensitivity` | Enum: VERY_SENSITIVE / SENSITIVE / BALANCED / CONSERVATIVE / VERY_CONSERVATIVE |
| `WhisperASR` | Speech recognition (wraps whisper.cpp) |
| `WhisperConfig` | Whisper config including `word_timestamps`, `input_sample_rate` |
| `WhisperModelType` | Enum: TINY / BASE / SMALL / MEDIUM / LARGE |

### Phase 6: NumPy Integration and Callback Support
- `WhisperASR.transcribe_buffer(numpy.ndarray)` — transcribe from in-memory audio
- `RNNoise.process(numpy.ndarray)` — in-place noise reduction
- `WAVWriter.write_samples_array(numpy.ndarray)` / `FLACWriter.write_samples_array(numpy.ndarray)`
- Python callbacks for `VADSegmenter.process_frame()`, `VADSegmenter.flush()`, `AudioCapture.start()`
- Proper GIL handling for thread-safe real-time audio callbacks

### Phase 7: Packaging and PyPI Release
- Prebuilt wheels published to PyPI for:
  - Linux x86_64 (manylinux_2_39): Python 3.9–3.12
  - macOS ARM64 (macosx_11_0): Python 3.9–3.12
  - Windows x86_64 (win_amd64): Python 3.9–3.12 (RNNoise disabled on Windows)
- Source distribution (sdist) available for other platforms
- GitHub Actions CI/CD: `ci.yml`, `release.yml`, `pr.yml`

### Phase 8: Documentation and Examples
- `python/README.md` — full API reference
- `python/docs/QUICKSTART.md` — quick start guide
- `python/docs/tutorials/ffvoice_tutorial.ipynb` — Jupyter notebook tutorial
- Examples: `basic_transcription.py`, `realtime_transcription.py`, `complete_realtime_pipeline.py`

---

## New API in v0.6.1 (additional bindings)

### `ffvoice.AudioMixer` — Multi-track Mixer
- `initialize(sample_rate, channels)`
- `add_track(gain, pan)` → track id
- `set_gain(track_id, gain)` / `set_pan(track_id, pan)` / `set_mute(track_id, muted)`
- `set_master_gain(gain)`
- `mix_block({track_id: numpy.int16_array, ...})` → mixed numpy.int16 array

### `ffvoice.RingBuffer` — Lock-free SPSC Ring Buffer
- `RingBuffer(capacity)` — capacity in int16 samples
- `push(sample)` / `pop()` — single sample
- `push_bulk(numpy.int16_array)` / `pop_bulk(n)` — bulk transfer
- `size()` / `capacity()` / `clear()`

### Word-level Timestamps
- `WhisperConfig.word_timestamps = True`
- `ffvoice.Word` — fields: `start_ms`, `end_ms`, `text`, `probability`
- `TranscriptionSegment.words` — list of `Word` objects

---

## Exported API Summary

### Classes (16 total, up from 13 in v0.4.0)
Original 13 classes + 3 new in v0.6.1:
- `AudioMixer` — multi-track audio mixer
- `RingBuffer` — lock-free SPSC ring buffer
- `Word` — per-word timestamp result

### Enums
- `WhisperModelType`: TINY, BASE, SMALL, MEDIUM, LARGE
- `VADSensitivity`: VERY_SENSITIVE, SENSITIVE, BALANCED, CONSERVATIVE, VERY_CONSERVATIVE

---

## Platform Support

| Platform | Prebuilt Wheel | Notes |
|----------|---------------|-------|
| Linux x86_64 | Yes | manylinux_2_39, Python 3.9–3.12 |
| macOS ARM64 (Apple Silicon) | Yes | macosx_11_0, Python 3.9–3.12 |
| Windows x86_64 | Yes | win_amd64, Python 3.9–3.12; RNNoise disabled |
| macOS Intel (x86_64) | No | Build from source (sdist) |
| Other | sdist | Requires C++ toolchain and system dependencies |

---

## Known Limitations

1. **Windows**: RNNoise noise reduction is disabled (MSVC does not support C99 VLA, which RNNoise requires). All other features work.
2. **macOS Intel**: No prebuilt wheel; Intel Mac users must build from source.
3. **RNNoise on Windows**: `ffvoice.RNNoise` and `ffvoice.RNNoiseConfig` are not available on Windows wheels.

---

## Build Reference

```bash
# Build with Python bindings (Linux/macOS)
cmake .. -DBUILD_PYTHON=ON -DENABLE_RNNOISE=ON -DENABLE_WHISPER=ON
cmake --build . --target _ffvoice

# Install from source
pip install .

# Verify
python -c "import ffvoice; print(ffvoice.__version__)"
```

---

**Status**: Phases 1–8 complete. Published on PyPI as `ffvoice`. Current version: **v0.6.1**.
