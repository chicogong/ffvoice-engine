# ffvoice — examples

## CLI

```
# Plain transcript
ffvoice --transcribe interview.wav -o interview.txt

# SRT subtitles
ffvoice --transcribe lecture.flac --format srt -o lecture.srt

# JSON with segment + word timestamps
ffvoice --transcribe podcast.wav --format json -o podcast.json

# Force a language (Chinese)
ffvoice --transcribe meeting.wav --language zh --format srt -o meeting.srt

# Transcribe a meeting and label the speakers
ffvoice --transcribe meeting.wav --diarize --format json -o meeting.json

# Two known speakers
ffvoice --transcribe call.wav --diarize --num-speakers 2 --format srt -o call.srt

# Live captions from the microphone for 60 seconds
ffvoice --record -o talk.wav --live-captions -t 60

# List input devices
ffvoice --list-devices
```

## MCP (agent) usage

Once the `ffvoice` MCP server is connected, an agent can call:

- `transcribe_file(path="/audio/interview.wav", language="auto", model="base")`
  — file → text + timestamped segments.
- `transcribe_file_with_diarization(path="/audio/meeting.wav", num_speakers=-1)`
  — file → segments each carrying a `speaker_id`, plus a `speaker_segments`
  timeline. Pass `num_speakers` when the count is known, or `-1` to
  auto-detect.
- `list_audio_devices()` — enumerate microphones; then call
  `capture_and_transcribe` (record-and-transcribe) or `capture_and_caption`
  (live captions) with the chosen device index and a duration.

## Python API

```python
import ffvoice

asr = ffvoice.WhisperASR(ffvoice.WhisperConfig())
asr.initialize()
for seg in asr.transcribe_file("audio.wav"):
    print(f"[{seg.start_ms}-{seg.end_ms}ms] speaker {seg.speaker_id}: {seg.text}")
```

For diarization from Python, see `ffvoice.mcp._diarization_pipeline`
(`make_diarizer()` + `DiarizationPipeline`).
