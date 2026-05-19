# ffvoice — reference

## Install

```
pip install ffvoice                      # core: transcription, VAD, capture
pip install 'ffvoice[mcp]'               # + MCP server (ffvoice-mcp)
pip install 'ffvoice[mcp,diarization]'   # + speaker diarization
```

Platforms: macOS (Apple Silicon + Intel), Linux x86_64, Windows x86_64;
Python 3.10–3.14. The `diarization` extra works on all platforms — it pulls
the `sherpa-onnx` package, whose wheels bundle the ONNX Runtime.

## Models — automatic

Whisper weights and the diarization models download automatically on first
use to the per-user cache:

- `~/.cache/ffvoice/whisper/ggml-<size>.bin`
- `~/.cache/ffvoice/diarization/...`

No manual download or environment variable is needed. The first use of a
given model is slower while it fetches (Whisper tiny ≈ 39 MB; the two
diarization models ≈ 36 MB total).

### Overrides
- `FFVOICE_CACHE_DIR` — change the cache location.
- `FFVOICE_MODEL_PATH` — a directory holding pre-downloaded `ggml-*.bin`
  files; used in preference to the cache / download (for fully air-gapped
  setups).

## MCP server

Entry point: `ffvoice-mcp` (installed by the `mcp` extra). Register it with
an MCP client. Claude Desktop (`claude_desktop_config.json`):

```json
{"mcpServers": {"ffvoice": {"command": "ffvoice-mcp", "args": []}}}
```

Tools: `transcribe_file`, `transcribe_file_with_diarization`,
`capture_and_transcribe`, `capture_and_caption`, `list_audio_devices`.

## CLI flags

`ffvoice --help` lists every flag. Key ones:

| Flag | Purpose |
|------|---------|
| `--transcribe <file>` | Transcribe an audio file |
| `--format txt\|srt\|vtt\|json` | Output format |
| `-o, --output <file>` | Output path |
| `--language <code>` | Force a language (otherwise auto-detect) |
| `--diarize` | Add speaker labels (needs the diarization backend) |
| `--num-speakers <N>` | Expected speaker count (default: auto-detect) |
| `--record` / `-t <sec>` | Record from a microphone |
| `--live-captions` | Live captions while recording |
| `--list-devices` | List audio input devices |

## Speaker diarization — scope & tips

- The bundled speaker-embedding model is **English-tuned**. For other
  languages, point the diarizer at a language-matched embedding model
  (`DiarizerConfig.embedding_model_path`).
- Automatic speaker-count detection is unreliable with several speakers.
  When the count is known, pass `--num-speakers N` (CLI) or
  `num_speakers=N` (MCP / Python) for materially better results.
- Diarization quality is that of the underlying sherpa-onnx
  pyannote-segmentation-3.0 pipeline — ffvoice wraps it faithfully and
  does not change its accuracy.

## Privacy

100% offline. Audio is processed locally and never transmitted. The only
network access is the one-time model download — and that can be skipped
entirely by pre-staging models and pointing `FFVOICE_MODEL_PATH` at them.
