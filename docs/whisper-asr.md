# Whisper ASR 语音识别集成

**状态**: ✅ 已完成集成（Milestone 3）

## 📋 目录

- [Whisper ASR 简介](#whisper-asr-简介)
- [技术架构](#技术架构)
- [编译与安装](#编译与安装)
- [使用指南](#使用指南)
- [性能测试](#性能测试)
- [技术细节](#技术细节)
- [故障排除](#故障排除)

---

## Whisper ASR 简介

### 什么是 Whisper？

Whisper 是由 OpenAI 开发的自动语音识别（ASR）系统，使用大规模多语言数据集训练。whisper.cpp 是 Whisper 模型的 C++ 移植版本，专为高性能推理和跨平台部署优化。

### 核心特点

- **多语言支持**：支持 99 种语言（包括中文、英文）
- **高准确率**：英文 ~8-10% WER，中文 ~12-15% WER
- **快速推理**：5-75x realtime（取决于音频长度和模型大小）
- **离线运行**：无需网络连接，完全本地化
- **多种模型**：tiny (39MB) / base (140MB) / small (466MB) / medium (1.5GB)
- **字幕生成**：支持纯文本、SRT、VTT 三种格式

### 应用场景

✅ **推荐使用**：
- 语音转文字（会议记录、访谈整理）
- 字幕生成（视频、播客）
- 语音笔记和日记
- 多语言翻译（转写 + 翻译）
- ASR 数据标注

⚠️ **限制**：
- 仅支持离线模式（Phase 1，实时模式在 Phase 2）
- tiny 模型准确率有限（建议使用 base 或更大模型提高准确率）
- 长音频可能需要分段处理

---

## 技术架构

### 整体流程

```
音频文件 (WAV/FLAC, 48kHz, int16, mono/stereo)
    ↓
AudioConverter 加载
    ↓
格式转换 (int16 → float, [-1, 1])
    ↓
立体声转单声道 (stereo → mono)
    ↓
重采样 (48kHz → 16kHz)
    ↓
Whisper 推理 (16kHz, float, mono)
    ↓
结果提取 (TranscriptionSegment)
    ↓
字幕生成 (纯文本/SRT/VTT)
```

### WhisperProcessor 类设计

```cpp
class WhisperProcessor {
public:
    struct WhisperConfig {
        std::string model_path = WHISPER_MODEL_PATH;  // tiny 模型路径
        std::string language = "auto";                // 语言（auto/zh/en）
        int n_threads = 4;                            // 推理线程数
        bool translate = false;                       // 是否翻译成英文
        bool print_progress = true;                   // 打印进度
    };

    explicit WhisperProcessor(const WhisperConfig& config);
    ~WhisperProcessor();

    bool Initialize();
    bool TranscribeFile(const std::string& audio_file,
                       std::vector<TranscriptionSegment>& segments);
    bool TranscribeBuffer(const int16_t* samples, size_t num_samples,
                         std::vector<TranscriptionSegment>& segments);

private:
    struct whisper_context* ctx_ = nullptr;
    bool LoadAudioFile(const std::string& filename,
                      std::vector<float>& pcm_data);
    void ExtractSegments(std::vector<TranscriptionSegment>& segments);
};
```

### TranscriptionSegment 结构

```cpp
struct TranscriptionSegment {
    int64_t start_ms;   // 开始时间（毫秒）
    int64_t end_ms;     // 结束时间（毫秒）
    std::string text;   // 文本内容
    float confidence;   // 置信度（0.0-1.0）
};
```

### AudioConverter 工具类

负责音频格式转换，将 WAV/FLAC 文件转换为 Whisper 所需的 16kHz float mono 格式。

```cpp
class AudioConverter {
public:
    // WAV/FLAC → 16kHz float mono
    static bool LoadAndConvert(const std::string& filename,
                              std::vector<float>& pcm_data,
                              int target_sample_rate = 16000);

    // int16 → float 归一化
    static void Int16ToFloat(const int16_t* input, size_t num_samples,
                            float* output);

    // 重采样（线性插值）
    static void Resample(const float* input, size_t input_size, int input_rate,
                        float* output, size_t output_size, int output_rate);

    // 立体声 → 单声道
    static void StereoToMono(const float* stereo, size_t num_frames,
                            float* mono);
};
```

### SubtitleGenerator 工具类

负责将转写结果生成不同格式的字幕文件。

```cpp
class SubtitleGenerator {
public:
    enum class Format {
        PlainText,  // 纯文本（无时间戳）
        SRT,        // SubRip 字幕
        VTT         // WebVTT 字幕
    };

    static bool Generate(const std::vector<TranscriptionSegment>& segments,
                        const std::string& output_file,
                        Format format);
};
```

---

## 编译与安装

### 方案：CMake FetchContent（推荐✅）

whisper.cpp 和 tiny 模型会**自动下载和编译**，无需手动安装。

```bash
# 1. 配置 CMake（启用 Whisper）
cd ffvoice-engine
mkdir build && cd build
cmake .. -DCMAKE_BUILD_TYPE=Release -DENABLE_WHISPER=ON

# 2. 编译（首次会自动下载 whisper.cpp 和 tiny 模型）
make -j$(nproc)

# 3. 验证
./ffvoice --help | grep transcribe
```

**输出示例**：
```
-- Fetching whisper.cpp from GitHub...
-- whisper.cpp configured successfully
--   Source dir: /path/to/build/_deps/whisper-src
--   Include dir: /path/to/build/_deps/whisper-src
-- Downloading whisper tiny model (39MB)...
-- Model downloaded successfully: /path/to/build/models/ggml-tiny.bin
[100%] Built target whisper
[100%] Built target ffvoice
```

### 编译选项

| 选项 | 说明 |
|------|------|
| `-DENABLE_WHISPER=ON` | 启用 Whisper ASR（推荐）|
| `-DENABLE_WHISPER=OFF` | 禁用 Whisper ASR（默认）|
| `-DENABLE_RNNOISE=ON` | 同时启用 RNNoise（推荐组合）|

### 系统要求

- **CMake**: 3.20+
- **C++20 编译器**: GCC 10+ / Clang 12+ / MSVC 2019+
- **FFmpeg**: 4.4+ (libavcodec, libavformat, libavutil, libswresample)
- **网络连接**: 首次编译需要下载 whisper.cpp 和模型文件（39MB）
- **磁盘空间**: ~150MB（whisper.cpp + tiny 模型）
- **内存**: 至少 512MB 可用内存

### 下载其他模型（可选）

默认使用 tiny 模型（39MB），如需更高准确率，可下载更大的模型：

```bash
cd build/models

# base 模型 (140MB, 更高准确率)
wget https://huggingface.co/ggerganov/whisper.cpp/resolve/main/ggml-base.bin

# small 模型 (466MB, 专业级准确率)
wget https://huggingface.co/ggerganov/whisper.cpp/resolve/main/ggml-small.bin

# medium 模型 (1.5GB, 最高准确率)
wget https://huggingface.co/ggerganov/whisper.cpp/resolve/main/ggml-medium.bin
```

**注意**：更大的模型推理速度更慢，但准确率更高。

---

## 使用指南

### 基础使用

#### 1. 转写音频为纯文本

```bash
./ffvoice --transcribe recording.wav -o transcript.txt
```

#### 2. 生成 SRT 字幕

```bash
./ffvoice --transcribe recording.wav --format srt -o subtitles.srt
```

**输出示例**：
```
1
00:00:00,000 --> 00:00:03,000
Hello, this is a test recording.

2
00:00:03,000 --> 00:00:07,200
Whisper is working correctly.
```

#### 3. 生成 VTT 字幕

```bash
./ffvoice --transcribe recording.wav --format vtt -o subtitles.vtt
```

**输出示例**：
```
WEBVTT

00:00:00.000 --> 00:00:03.000
Hello, this is a test recording.

00:00:03.000 --> 00:00:07.200
Whisper is working correctly.
```

### 高级配置

#### 指定语言

```bash
# 中文转写
./ffvoice --transcribe speech.wav --language zh -o transcript_zh.txt

# 英文转写
./ffvoice --transcribe speech.wav --language en -o transcript_en.txt

# 自动检测语言（默认）
./ffvoice --transcribe speech.wav --language auto -o transcript.txt
```

#### 转写 FLAC 文件

```bash
./ffvoice --transcribe recording.flac --format srt -o subtitles.srt
```

支持的输入格式：
- ✅ WAV (所有采样率)
- ✅ FLAC (所有采样率)

#### 完整工作流：录制 + 音频处理 + 转写

```bash
# 1. 高质量录音（音频处理）
./ffvoice --record -o speech.flac \
  --highpass 80 \
  --rnnoise \
  --normalize \
  -t 60

# 2. 转写为字幕
./ffvoice --transcribe speech.flac --format srt -o speech.srt

# 3. 查看结果
cat speech.srt
```

### 输出格式对比

| 格式 | 扩展名 | 时间戳 | 用途 |
|------|--------|--------|------|
| 纯文本 | `.txt` | ❌ | 文档整理、笔记 |
| SRT | `.srt` | ✅ (HH:MM:SS,mmm) | 视频字幕（通用） |
| VTT | `.vtt` | ✅ (HH:MM:SS.mmm) | Web 字幕、HTML5 |

---

## 性能测试

### 测试环境

- **硬件**: Apple M3 Pro (Rosetta 2)
- **模型**: Whisper Tiny (39MB)
- **采样率**: 48kHz → 16kHz
- **线程数**: 4

### 转写速度

| 音频长度 | 处理时间 | 实时倍率 | 说明 |
|----------|---------|----------|------|
| 3 秒     | 0.52 秒 | **5.8x** | 短音频 |
| 10 秒    | 0.59 秒 | **17x**  | 中等音频 |
| 30 秒    | 0.40 秒 | **75x**  | 长音频（最优） |

**结论**：音频越长，实时倍率越高（因为模型加载开销被均摊）。

### 内存占用

| 组件 | 大小 | 说明 |
|------|------|------|
| 模型加载 | 77.11 MB | Tiny 模型 |
| KV Cache | 17.48 MB | 键值缓存 |
| Compute Buffers | 177.12 MB | 计算缓冲区 |
| **总计** | **~272 MB** | 峰值内存 |

**结论**：tiny 模型内存占用较小，适合资源受限的设备。

### 准确率（Tiny 模型）

| 语言 | WER (Word Error Rate) | 说明 |
|------|----------------------|------|
| 英文 | ~8-10% | 清晰语音，无背景噪音 |
| 中文 | ~12-15% | 普通话，无背景噪音 |

**注意**：
- WER 取决于音频质量、口音、背景噪音等因素
- 使用 RNNoise 降噪可提高准确率
- 更大的模型（base/small/medium）可显著提高准确率

### 性能优化建议

1. **使用 RNNoise 预处理**（推荐✅）：
   ```bash
   ./ffvoice --record -o clean.flac --rnnoise -t 60
   ./ffvoice --transcribe clean.flac -o transcript.txt
   ```

2. **使用 FLAC 格式**（节省磁盘空间）：
   ```bash
   ./ffvoice --record -o speech.flac -t 60
   ./ffvoice --transcribe speech.flac -o transcript.txt
   ```

3. **调整线程数**（多核 CPU）：
   - 修改 `WhisperConfig::n_threads` 可提升推理速度
   - 默认 4 线程适合大多数场景

---

## 技术细节

### Whisper.cpp API

```c
// 加载模型
struct whisper_context* ctx = whisper_init_from_file("model.bin");

// 配置参数
struct whisper_full_params params = whisper_full_default_params(
    WHISPER_SAMPLING_GREEDY
);
params.language = "zh";           // 语言
params.n_threads = 4;             // 线程数
params.translate = false;         // 是否翻译
params.print_progress = true;     // 打印进度

// 运行推理（输入：16kHz float PCM）
int result = whisper_full(ctx, params, pcm_data, pcm_size);

// 提取结果
int n_segments = whisper_full_n_segments(ctx);
for (int i = 0; i < n_segments; ++i) {
    int64_t t0 = whisper_full_get_segment_t0(ctx, i);  // 开始时间（厘秒）
    int64_t t1 = whisper_full_get_segment_t1(ctx, i);  // 结束时间（厘秒）
    const char* text = whisper_full_get_segment_text(ctx, i);
}

// 清理
whisper_free(ctx);
```

### 音频格式转换流程

**Whisper 输入要求**：
- 采样率：**16000 Hz**
- 格式：**float32**
- 声道：**mono**
- 范围：**[-1.0, 1.0]**

**我们的录音格式**：
- 采样率：**48000 Hz**
- 格式：**int16**
- 声道：**mono/stereo**

**转换步骤**：

```
WAV/FLAC 文件 (48kHz, int16, stereo)
    ↓ LoadWAV/LoadFLAC
float PCM (48kHz, float, stereo)
    ↓ StereoToMono
float PCM (48kHz, float, mono)
    ↓ Resample (线性插值)
float PCM (16kHz, float, mono)  ← Whisper 输入
```

**重采样实现**（线性插值）：

```cpp
void AudioConverter::Resample(const float* input, size_t input_size, int input_rate,
                              float* output, size_t output_size, int output_rate) {
    float ratio = static_cast<float>(input_rate) / output_rate;

    for (size_t i = 0; i < output_size; ++i) {
        float pos = i * ratio;
        size_t index = static_cast<size_t>(pos);
        float frac = pos - index;

        if (index + 1 < input_size) {
            // 线性插值
            output[i] = input[index] * (1.0f - frac) + input[index + 1] * frac;
        } else {
            output[i] = input[index];
        }
    }
}
```

### 字幕格式生成

#### SRT 格式

```
1
00:00:00,000 --> 00:00:03,500
第一行字幕

2
00:00:03,500 --> 00:00:07,200
第二行字幕
```

**时间戳格式**：`HH:MM:SS,mmm`（逗号分隔毫秒）

#### VTT 格式

```
WEBVTT

00:00:00.000 --> 00:00:03.500
第一行字幕

00:00:03.500 --> 00:00:07.200
第二行字幕
```

**时间戳格式**：`HH:MM:SS.mmm`（句点分隔毫秒）
**头部**：必须以 `WEBVTT` 开头

#### 纯文本格式

```
第一行字幕
第二行字幕
```

**无时间戳**，仅保留文本内容。

---

## 故障排除

### 1. 编译错误：找不到 Whisper

**错误信息**：
```
CMake Error: whisper library not found
```

**解决方案**：
- 确保使用了 `-DENABLE_WHISPER=ON` 编译选项
- 检查网络连接（首次编译需要下载 whisper.cpp）
- 清除缓存重新编译：
  ```bash
  rm -rf build
  mkdir build && cd build
  cmake .. -DENABLE_WHISPER=ON
  make -j$(nproc)
  ```

### 2. 编译错误：LOG_* 宏未定义

**错误信息**：
```
error: use of undeclared identifier 'LOG_INFO'
error: use of undeclared identifier 'LOG_ERROR'
```

**解决方案**：
这是已知问题，已在最新版本修复。确保使用最新代码：
```bash
git pull origin master
cd build
cmake .. -DENABLE_WHISPER=ON
make -j$(nproc)
```

### 3. 运行时错误：无法加载模型

**错误信息**：
```
[ERROR] Failed to load whisper model: /path/to/model.bin
```

**解决方案**：
1. 检查模型文件是否存在：
   ```bash
   ls -lh build/models/ggml-tiny.bin
   ```

2. 如果不存在，手动下载：
   ```bash
   mkdir -p build/models
   cd build/models
   wget https://huggingface.co/ggerganov/whisper.cpp/resolve/main/ggml-tiny.bin
   ```

3. 检查模型路径配置（CMakeLists.txt）：
   ```cmake
   add_compile_definitions(WHISPER_MODEL_PATH="${WHISPER_TINY_MODEL}")
   ```

### 4. 转写错误：音频格式不支持

**错误信息**：
```
[ERROR] Failed to load audio file: unsupported format
```

**解决方案**：
- 确保使用 WAV 或 FLAC 格式
- 检查文件是否损坏：
  ```bash
  ffmpeg -i input.wav -f null -
  ```
- 转换格式：
  ```bash
  ffmpeg -i input.mp3 -ar 48000 -ac 1 output.wav
  ```

### 5. 转写结果为空

**可能原因**：
- 音频文件为静音或噪声
- 音频质量太差
- 语言检测失败

**解决方案**：
1. 检查音频内容：
   ```bash
   afplay recording.wav
   ```

2. 指定语言：
   ```bash
   ./ffvoice --transcribe recording.wav --language zh -o transcript.txt
   ```

3. 使用 RNNoise 预处理：
   ```bash
   ./ffvoice --record -o clean.wav --rnnoise -t 10
   ./ffvoice --transcribe clean.wav -o transcript.txt
   ```

### 6. 转写速度太慢

**可能原因**：
- 使用了大模型（base/small/medium）
- CPU 资源不足
- 线程数配置不当

**解决方案**：
1. 使用 tiny 模型（最快）
2. 增加线程数（修改 `WhisperConfig::n_threads`）
3. 关闭其他占用 CPU 的应用

### 7. CLI 选项不可用

**错误信息**：
```
(Whisper ASR not available - rebuild with -DENABLE_WHISPER=ON)
```

**解决方案**：
项目未启用 Whisper 编译。重新编译：
```bash
cd build
cmake .. -DENABLE_WHISPER=ON
make -j$(nproc)
```

### 8. Rosetta 2 编译错误（macOS Apple Silicon）

**错误信息**：
```
cc: error: unsupported option '-mavx' for target 'x86_64-apple-darwin'
```

**解决方案**：
这是已知问题，已在 CMakeLists.txt 中禁用 AVX 指令集：
```cmake
set(WHISPER_NO_AVX ON CACHE BOOL "" FORCE)
set(WHISPER_NO_AVX2 ON CACHE BOOL "" FORCE)
set(WHISPER_NO_FMA ON CACHE BOOL "" FORCE)
set(WHISPER_NO_F16C ON CACHE BOOL "" FORCE)
```

确保使用最新代码，然后重新编译。

---

## 参考资料

### 官方文档

- [whisper.cpp 官方仓库](https://github.com/ggerganov/whisper.cpp)
- [OpenAI Whisper](https://github.com/openai/whisper)
- [Whisper 模型下载](https://huggingface.co/ggerganov/whisper.cpp)

### 相关文档

- [audio-processing.md](./audio-processing.md) - 音频处理框架总览
- [rnnoise.md](./rnnoise.md) - RNNoise 降噪集成（推荐组合使用）
- [README.md](../README.md) - 项目主文档

### 技术论文

- [Robust Speech Recognition via Large-Scale Weak Supervision](https://cdn.openai.com/papers/whisper.pdf) - Whisper 论文

---

**最后更新**：2025-12-27
**作者**：ffvoice-engine 开发团队
**版本**：0.1.0
