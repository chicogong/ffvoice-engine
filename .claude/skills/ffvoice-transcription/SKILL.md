---
name: ffvoice-transcription
description: >-
  Offline speech-to-text and speaker diarization with the ffvoice engine.
  Use when the user wants to transcribe an audio file, generate subtitles
  (SRT/VTT/JSON), identify who spoke when (speaker diarization), caption or
  transcribe live microphone input, or list audio input devices — all fully
  on-device, with no cloud API and no API key.
when_to_use: >-
  Triggers: "transcribe this audio/recording", "what does this recording
  say", "generate subtitles/captions", "diarize" / "who said what" / "label
  the speakers", "live captions", "caption my microphone", "offline speech
  recognition", or any mention of transcribing .wav/.flac/.mp3/.m4a files.
allowed-tools: Bash(ffvoice *) Bash(pip show ffvoice*) Bash(pip install ffvoice*)
---

# ffvoice — offline speech transcription & diarization

`ffvoice` is an offline speech toolkit: speech-to-text (Whisper), speaker
diarization ("who spoke when"), and live captioning. Everything runs
locally — no audio leaves the machine, no API key.

## Preconditions

Check ffvoice is installed: `pip show ffvoice`.

- Not installed → `pip install 'ffvoice[mcp]'`. For speaker diarization add
  the extra: `pip install 'ffvoice[mcp,diarization]'`.
- Models (Whisper weights, diarization models) download automatically to
  `~/.cache/ffvoice/` on first use — no manual setup. The first run of a
  given model is slower while it downloads.

## Two ways to use it — pick one

1. **MCP server** — if an `ffvoice` MCP server is connected (tools named
   `transcribe_file`, `list_audio_devices`, etc. are available), call those
   tools directly. Preferred path for agent use.
2. **CLI** — otherwise drive the `ffvoice` command-line tool. Works for
   batch/scripted use without an MCP connection.

## MCP tools (when the ffvoice MCP server is connected)

| Tool | Use it for |
|------|-----------|
| `transcribe_file` | Transcribe one audio file → text + timestamped segments. Supports language, model size, word timestamps. |
| `transcribe_file_with_diarization` | Transcribe **and** label speakers — every segment gets a `speaker_id`. Use when the user asks "who said what". |
| `capture_and_transcribe` | Record from a microphone for N seconds and transcribe it. |
| `capture_and_caption` | Live streaming captions from the microphone (partial + final events). |
| `list_audio_devices` | Enumerate audio input devices. Call this FIRST whenever a microphone task needs a device index. |

Workflow:
- File transcription → confirm the path exists, then `transcribe_file`
  (or `transcribe_file_with_diarization` when the user wants speakers).
- Microphone → `list_audio_devices` first, then `capture_and_transcribe`
  or `capture_and_caption` with the chosen device.

## CLI usage (`ffvoice` command)

The CLI uses flag-style options (not subcommands). Confirm with
`ffvoice --help`. Common forms:

```
# transcribe a file to a transcript / subtitle
ffvoice --transcribe speech.wav --format srt -o speech.srt
ffvoice --transcribe speech.wav --format json -o speech.json   # segments + word timestamps
ffvoice --transcribe speech.wav --language zh -o out.txt        # force a language

# transcribe + speaker diarization
ffvoice --transcribe meeting.wav --diarize --format json -o meeting.json
ffvoice --transcribe meeting.wav --diarize --num-speakers 2 --format srt -o meeting.srt

# live captions while recording from the microphone
ffvoice --record -o talk.wav --live-captions -t 60

# list audio input devices
ffvoice --list-devices
```

`--format` accepts `txt`, `srt`, `vtt`, `json`. `--diarize` adds a
`speaker_id` to each segment (visible in JSON output).

See [examples.md](examples.md) for more invocations and
[reference.md](reference.md) for install options, the model cache, and
environment variables.

## Notes

- All processing is local; large files take time — tell the user it is
  working rather than going silent.
- `transcribe_file_with_diarization` (MCP) and `--diarize` (CLI) need the
  diarization backend: `pip install 'ffvoice[diarization]'`, or a source
  build with `-DENABLE_DIARIZATION=ON`. If it is unavailable the tool
  returns a clear error — relay it to the user, do not guess.
