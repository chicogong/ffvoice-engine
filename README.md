# ffvoice-engine

<!-- Build & CI Status -->
[![CI](https://github.com/chicogong/ffvoice-engine/workflows/CI/badge.svg)](https://github.com/chicogong/ffvoice-engine/actions/workflows/ci.yml)
[![Release](https://github.com/chicogong/ffvoice-engine/workflows/Release/badge.svg)](https://github.com/chicogong/ffvoice-engine/actions/workflows/release.yml)
[![codecov](https://codecov.io/gh/chicogong/ffvoice-engine/branch/master/graph/badge.svg)](https://codecov.io/gh/chicogong/ffvoice-engine)

<!-- License & Language -->
[![License: MIT](https://img.shields.io/badge/License-MIT-yellow.svg)](https://opensource.org/licenses/MIT)
[![C++20](https://img.shields.io/badge/C%2B%2B-20-blue.svg)](https://en.cppreference.com/w/cpp/20)
[![CMake](https://img.shields.io/badge/CMake-3.20+-064F8C.svg?logo=cmake)](https://cmake.org/)

<!-- Platform Support -->
[![Platform](https://img.shields.io/badge/Platform-macOS%20|%20Linux%20|%20Windows-lightgrey.svg)]()
[![macOS](https://github.com/chicogong/ffvoice-engine/workflows/CI/badge.svg?label=macOS)](https://github.com/chicogong/ffvoice-engine/actions)
[![Linux](https://github.com/chicogong/ffvoice-engine/workflows/CI/badge.svg?label=Linux)](https://github.com/chicogong/ffvoice-engine/actions)
[![Windows](https://github.com/chicogong/ffvoice-engine/workflows/CI/badge.svg?label=Windows)](https://github.com/chicogong/ffvoice-engine/actions)

<!-- Version & Community -->
[![PyPI version](https://img.shields.io/pypi/v/ffvoice.svg)](https://pypi.org/project/ffvoice/)
[![Python versions](https://img.shields.io/pypi/pyversions/ffvoice.svg)](https://pypi.org/project/ffvoice/)
[![GitHub release](https://img.shields.io/github/release/chicogong/ffvoice-engine.svg)](https://github.com/chicogong/ffvoice-engine/releases)
[![GitHub stars](https://img.shields.io/github/stars/chicogong/ffvoice-engine?style=social)](https://github.com/chicogong/ffvoice-engine/stargazers)
[![GitHub forks](https://img.shields.io/github/forks/chicogong/ffvoice-engine?style=social)](https://github.com/chicogong/ffvoice-engine/network/members)

<!-- Dependencies -->
[![FFmpeg](https://img.shields.io/badge/FFmpeg-4.4+-007808.svg?logo=ffmpeg)](https://ffmpeg.org/)
[![PortAudio](https://img.shields.io/badge/PortAudio-19.7+-8B0000.svg)](http://www.portaudio.com/)
[![FLAC](https://img.shields.io/badge/FLAC-1.5+-orange.svg)](https://xiph.org/flac/)
[![Whisper](https://img.shields.io/badge/Whisper-tiny-purple.svg)](https://github.com/ggerganov/whisper.cpp)

<!-- Code Quality -->
[![Code Style](https://img.shields.io/badge/code%20style-Google-blue.svg)](https://google.github.io/styleguide/cppguide.html)
[![PRs Welcome](https://img.shields.io/badge/PRs-welcome-brightgreen.svg)](CONTRIBUTING.md)

> 🎙️ 高性能 C++ 语音引擎 - 实时音频处理 + AI 语音识别 + 边录边转写

## 📋 项目介绍

ffvoice-engine 是一个**轻量级、高性能的音频处理引擎**，专注于实时音频采集、智能处理和语音识别。

### 🎯 使用场景

- **📝 会议记录** - 实时转写会议内容，自动生成字幕文件
- **🎓 在线教育** - 录制课程并生成字幕，支持多语言识别
- **🎙️ 播客制作** - 高质量音频录制 + RNNoise 降噪 + 自动字幕生成
- **🎵 音乐制作** - 低延迟音频采集，支持 FLAC 无损压缩
- **🤖 语音助手** - 实时语音识别和处理，构建本地 AI 语音应用
- **📡 直播字幕** - 边录边转写，生成实时字幕流

### ✨ 核心优势

**vs 商业服务（Azure/Google Cloud Speech）**:
- ✅ **完全离线** - 无需网络，保护隐私，零 API 费用
- ✅ **低延迟** - 本地处理，<100ms 音频采集延迟
- ✅ **开源免费** - MIT 协议，可商用

**vs FFmpeg 命令行**:
- ✅ **实时转写** - 边录边识别，支持 VAD 智能分段
- ✅ **AI 降噪** - 集成 RNNoise 深度学习降噪
- ✅ **C++ SDK** - 可嵌入任何 C++ 应用，非黑盒工具

**vs Python 方案（whisper-cli）**:
- ✅ **高性能** - C++20 实现，比 Python 快 3-10x
- ✅ **低内存** - 单进程 <500MB（含 Whisper tiny 模型）
- ✅ **易部署** - 单一可执行文件，无 Python 环境依赖

### 💡 技术亮点

- 🚀 **零拷贝处理链** - 音频数据在内存中就地处理
- 🧠 **智能 VAD 分段** - 基于 RNNoise VAD 的语音活动检测
- 🎯 **高压缩比** - FLAC 无损压缩 2-3x，质量无损
- ⚡ **whisper.cpp 加速** - 推理速度 5-75x realtime（M2/M3）

### 核心特性

- ✅ **实时音频采集** - 低延迟麦克风/系统声音捕获 (PortAudio)
- ✅ **多格式输出** - WAV、FLAC 无损压缩
- ✅ **音频增强处理** - 音量归一化、高通滤波、RNNoise降噪
- ✅ **离线语音识别** - Whisper ASR (tiny model, 3种字幕格式)
- ✅ **实时字幕生成** - 基于 VAD 的边录边转写

## 🏗️ 当前状态

**Milestone 4**: 实时语音识别 (✨ 100% 完成)

- [x] 项目骨架搭建
- [x] CMake 构建系统
- [x] CLI 参数框架
- [x] **WAV 文件写入** (手写 RIFF 格式)
- [x] **FLAC 无损压缩** (libFLAC, 压缩比 2-3x)
- [x] **音频采集** (PortAudio, 实时流式捕获)
- [x] **音频信号生成器** (正弦波、静音、白噪声)
- [x] **设备枚举与选择**
- [x] **音频处理模块** (音量归一化 + 高通滤波)
- [x] **RNNoise 降噪** (深度学习，可选)
- [x] **RNNoise VAD** (语音活动检测，可选)
- [x] **VAD 音频分段器** (智能语音分段)
- [x] **WebRTC APM 框架** (可选，需外部库)
- [x] **Whisper ASR 集成** (离线语音识别)
- [x] **实时语音识别** (边录边转写)
- [x] **音频格式转换** (WAV/FLAC → 16kHz)
- [x] **字幕生成** (纯文本/SRT/VTT)
- [x] **单元测试** (39+ 个测试用例)
- [x] VSCode 开发环境配置
- [x] Google Test 测试框架集成

## 🚀 快速开始

### 依赖

- CMake 3.20+
- C++20 编译器（GCC 10+, Clang 12+, MSVC 2019+）
- FFmpeg 4.4+ (libavcodec, libavformat, libavutil, libswresample)
- PortAudio 19.7+ (音频采集)
- FLAC 1.5+ (无损压缩)
- **whisper.cpp** (可选，自动下载，用于语音识别)
- **RNNoise** (可选，自动下载，用于深度学习降噪)
- **WebRTC APM** (可选，需手动安装，参见下方说明)

macOS 安装：
```bash
brew install cmake ffmpeg portaudio flac
```

Linux (Ubuntu/Debian) 安装：
```bash
sudo apt-get install cmake build-essential \
  libavcodec-dev libavformat-dev libavutil-dev libswresample-dev \
  portaudio19-dev libflac-dev
```

#### WebRTC APM 安装（可选）

WebRTC APM 提供高级音频处理功能（降噪、自动增益、VAD），但需要手动安装：

**Linux (Ubuntu/Debian)**:
```bash
sudo apt-get install webrtc-audio-processing-dev
```

**从源代码编译** (Linux/macOS):
```bash
# 安装 meson 构建系统
brew install meson  # macOS
# 或
sudo apt-get install meson  # Linux

# 编译安装 WebRTC APM
git clone https://gitlab.freedesktop.org/pulseaudio/webrtc-audio-processing.git
cd webrtc-audio-processing
git checkout v1.3
meson setup build --prefix=/usr/local
meson compile -C build
sudo meson install -C build
```

**注意**: macOS Apple Silicon (ARM64) 用户可能遇到编译问题，建议使用预编译包或跳过此可选功能。

### 编译

**标准编译** (不含 WebRTC APM):
```bash
mkdir build && cd build
cmake .. -DCMAKE_BUILD_TYPE=Release
make -j$(nproc)
```

**启用 WebRTC APM** (需先安装库):
```bash
mkdir build && cd build
cmake .. -DCMAKE_BUILD_TYPE=Release -DENABLE_WEBRTC_APM=ON
make -j$(nproc)
```

**启用 RNNoise 降噪** (推荐，自动下载):
```bash
mkdir build && cd build
cmake .. -DCMAKE_BUILD_TYPE=Release -DENABLE_RNNOISE=ON
make -j$(nproc)
# RNNoise 库会通过 CMake FetchContent 自动下载和编译
```

**启用 Whisper 语音识别** (推荐，自动下载):
```bash
mkdir build && cd build
cmake .. -DCMAKE_BUILD_TYPE=Release -DENABLE_WHISPER=ON
make -j$(nproc)
# whisper.cpp 和 tiny 模型（39MB）会自动下载
```

**启用所有可选功能**:
```bash
cmake .. -DCMAKE_BUILD_TYPE=Release \
  -DENABLE_RNNOISE=ON \
  -DENABLE_WHISPER=ON \
  -DENABLE_WEBRTC_APM=ON
make -j$(nproc)
```

### 使用

```bash
# 查看帮助
./build/ffvoice --help

# 生成测试 WAV 文件（440Hz A4 音符，3秒）
./build/ffvoice --test-wav test.wav

# 列出可用音频设备
./build/ffvoice --list-devices

# 录制 10 秒 WAV 音频（默认格式）
./build/ffvoice --record -o recording.wav -t 10

# 录制 30 秒 FLAC 音频（无损压缩）
./build/ffvoice --record -o recording.flac -t 30

# 使用最大压缩级别录制 FLAC
./build/ffvoice --record -o recording.flac --compression 8 -t 60

# 选择特定设备录制立体声
./build/ffvoice --record -d 1 -o stereo.wav --channels 2 -t 20

# 启用音频处理（音量归一化 + 高通滤波）
./build/ffvoice --record -o clean.wav --enable-processing -t 10

# 仅启用音量归一化
./build/ffvoice --record -o normalized.wav --normalize -t 10

# 自定义高通滤波频率（去除 100Hz 以下噪声）
./build/ffvoice --record -o filtered.flac --highpass 100 -t 20

# 组合：FLAC + 音频处理
./build/ffvoice --record -o studio.flac --normalize --highpass 80 -t 30

# RNNoise 深度学习降噪（推荐用于语音录制）
./build/ffvoice --record -o clean.wav --rnnoise -t 10

# 完整处理链（高通 + RNNoise + 归一化）
./build/ffvoice --record -o studio.flac --highpass 80 --rnnoise --normalize -t 30

# RNNoise + VAD (实验性)
./build/ffvoice --record -o vad.wav --rnnoise-vad -t 20

# 播放录音
afplay recording.wav   # 或 recording.flac

# ==================== 语音识别（需启用 ENABLE_WHISPER） ====================

# 转写音频文件为纯文本
./build/ffvoice --transcribe recording.wav -o transcript.txt

# 生成 SRT 字幕文件
./build/ffvoice --transcribe recording.wav --format srt -o subtitles.srt

# 生成 VTT 字幕文件
./build/ffvoice --transcribe recording.wav --format vtt -o subtitles.vtt

# 指定语言（中文）
./build/ffvoice --transcribe recording.wav --language zh -o transcript_zh.txt

# 转写 FLAC 文件
./build/ffvoice --transcribe recording.flac --format srt -o subtitles.srt

# 完整工作流：录制 + 音频处理 + 转写
./build/ffvoice --record -o speech.flac --highpass 80 --rnnoise --normalize -t 30
./build/ffvoice --transcribe speech.flac --format srt -o speech.srt

# ==================== 实时语音识别（需启用 ENABLE_RNNOISE 和 ENABLE_WHISPER） ====================

# 边录边转写（实时模式）
./build/ffvoice --record -o speech.wav --rnnoise-vad --transcribe-live -t 60

# 实时转写 + 音频处理
./build/ffvoice --record -o speech.flac --rnnoise-vad --transcribe-live --highpass 80 --normalize -t 120
```

## 🐍 Python Bindings

ffvoice 提供高性能的 Python 绑定，让您在 Python 中轻松使用所有功能。

### 安装

**从 PyPI 安装** (推荐):
```bash
pip install ffvoice
```

**从源码安装**:
```bash
git clone https://github.com/chicogong/ffvoice-engine.git
cd ffvoice-engine
pip install .
```

### 平台兼容性

| 平台 | PyPI Wheel | 安装方式 | 状态 |
|------|-----------|---------|------|
| **🍎 Apple Silicon (M1/M2/M3)** | ✅ ARM64 | `pip install ffvoice` | ✅ 原生支持 |
| **🍎 Intel Mac** | ❌ 不兼容 | 从源码编译 | ⚠️ 需手动构建 |
| **🐧 Linux x86_64** | ✅ x86_64 | `pip install ffvoice` | ✅ 原生支持 |
| **🪟 Windows** | ⏳ 计划中 | 从源码编译 | 🚧 开发中 |

**重要说明**:
- **Apple Silicon 用户**: 直接使用 `pip install ffvoice` 即可，性能最佳
- **Intel Mac 用户**: PyPI wheel 不兼容，需要从源码编译:
  ```bash
  # 确保已安装依赖
  brew install cmake ffmpeg portaudio flac

  # 从源码安装
  git clone https://github.com/chicogong/ffvoice-engine.git
  cd ffvoice-engine
  pip install .
  ```
- **Rosetta 2 用户**: ARM64 wheel 在 Rosetta 环境下不工作，请使用 ARM64 原生 Python:
  ```bash
  # 检查 Python 架构
  python -c "import platform; print(platform.machine())"
  # 应该输出 'arm64'，如果是 'x86_64' 则需要重新安装 ARM64 Python

  # 强制使用 ARM64 Python
  arch -arm64 python3 -m pip install ffvoice
  ```

### 快速示例

```python
import ffvoice
import numpy as np

# 1. 语音识别
config = ffvoice.WhisperConfig()
config.model_type = ffvoice.WhisperModelType.TINY
asr = ffvoice.WhisperASR(config)
asr.initialize()

# 从文件转写
segments = asr.transcribe_file("audio.wav")
for seg in segments:
    print(f"[{seg.start_ms}ms - {seg.end_ms}ms] {seg.text}")

# 从 NumPy 数组转写
audio = np.zeros(48000, dtype=np.int16)  # 1秒音频
segments = asr.transcribe_buffer(audio)

# 2. 噪声抑制
rnnoise = ffvoice.RNNoise(ffvoice.RNNoiseConfig())
rnnoise.initialize(sample_rate=48000, channels=1)

audio = np.random.randint(-1000, 1000, 256, dtype=np.int16)
rnnoise.process(audio)  # 原地处理
vad_prob = rnnoise.get_vad_probability()

# 3. 实时音频采集
def audio_callback(audio_array):
    print(f"收到 {len(audio_array)} 个采样")

ffvoice.AudioCapture.initialize()
capture = ffvoice.AudioCapture()
capture.open(sample_rate=48000, channels=1, frames_per_buffer=256)
capture.start(audio_callback)
# ... 录制中 ...
capture.stop()
capture.close()
ffvoice.AudioCapture.terminate()
```

### 完整文档

详细文档和示例请查看 [`python/README.md`](python/README.md):
- 📖 完整 API 参考
- 🎯 13+ 代码示例
- 🚀 Quick Start 指南
- 📓 Jupyter Notebook 教程

**性能优势**:
- ⚡ **3-10x 更快** - C++ 核心 vs 纯 Python 实现
- 💾 **零拷贝** - NumPy 数组直接传递
- 🔒 **100% 离线** - 无需网络，隐私安全
- 🎙️ **完整工作流** - 采集 → 降噪 → VAD → 识别

## 📁 项目结构

```
ffvoice-engine/
├── CMakeLists.txt          # 主构建文件
├── include/ffvoice/        # 公共头文件
│   └── types.h             # 核心类型定义
├── src/                    # 源代码
│   ├── audio/              # 音频采集与处理模块
│   │   ├── audio_capture_device.* # ✅ PortAudio 采集器
│   │   ├── audio_processor.*      # ✅ 音频处理框架
│   │   ├── rnnoise_processor.*    # ✅ RNNoise 深度学习降噪 (可选)
│   │   ├── vad_segmenter.*        # ✅ VAD 音频分段器
│   │   ├── webrtc_processor.*     # ✅ WebRTC APM 框架 (可选)
│   │   └── whisper_processor.*    # ✅ Whisper ASR 语音识别 (可选)
│   ├── media/              # 媒体编码/封装
│   │   ├── wav_writer.*    # ✅ WAV 文件写入器
│   │   ├── flac_writer.*   # ✅ FLAC 无损压缩
│   │   └── audio_file_writer.* # FFmpeg 封装器（待实现）
│   └── utils/              # 工具类
│       ├── signal_generator.* # ✅ 音频信号生成
│       ├── ring_buffer.*   # ✅ 环形缓冲区
│       ├── audio_converter.*  # ✅ 音频格式转换
│       ├── subtitle_generator.* # ✅ 字幕生成（SRT/VTT）
│       └── logger.*        # ✅ 日志工具
├── apps/cli/               # CLI 应用
│   └── main.cpp            # ✅ 完整录音功能
├── tests/                  # 单元测试
│   ├── unit/               # ✅ 39 个测试用例
│   ├── mocks/              # Mock 对象
│   └── fixtures/           # 测试夹具
├── models/                 # AI 模型文件
└── scripts/                # 辅助脚本
```

## 🛣️ 路线图

### Milestone 1: 基础录制 (✨ 97% 完成)
- [x] WAV 文件写入（手写 RIFF 格式）
- [x] FLAC 无损压缩（libFLAC）
- [x] 音频采集（PortAudio 集成）
- [x] 音频信号生成器（测试用）
- [x] 音频处理框架（音量归一化 + 高通滤波）
- [x] CLI 完整功能（设备、格式、参数）
- [x] 单元测试覆盖（39 个测试用例）
- [x] WebRTC APM 框架（可选，需外部库集成）

### Milestone 2: 音频处理增强 (✨ 100% 完成)
- [x] RNNoise 降噪 (深度学习)
- [x] WebRTC APM 框架（可选）
- [x] 实时处理管道（处理器链）

### Milestone 3: 离线语音识别 (✨ 100% 完成)
- [x] whisper.cpp 集成（CMake FetchContent）
- [x] Whisper Tiny 模型自动下载（39MB）
- [x] 音频格式转换（WAV/FLAC → 16kHz float mono）
- [x] 离线转写功能（TranscribeFile）
- [x] 纯文本输出
- [x] SRT 字幕生成
- [x] VTT 字幕生成
- [x] CLI 参数集成（--transcribe, --format, --language）
- [x] 性能优化（5-75x realtime）

### Milestone 4: 实时语音识别 (✨ 100% 完成)
- [x] VADSegmenter 实现（基于 VAD 的智能分段）
- [x] RNNoiseProcessor VAD 支持
- [x] WhisperProcessor 缓冲处理（TranscribeBuffer）
- [x] 音频格式实时转换（int16 48kHz → float 16kHz）
- [x] CLI 实时转写集成（--transcribe-live）
- [x] VAD 分段状态机（Speech → Silence → Trigger）
- [x] 实时转写输出（边录边显示）

### Milestone 5: 性能优化与增强 (✨ 100% 完成)
- [x] Whisper 模型选择（TINY/BASE/SMALL/MEDIUM/LARGE）
- [x] 性能计时系统（分段计时 + RTF 计算）
- [x] VAD 灵敏度预设（5 种预设配置）
- [x] VAD 自适应阈值（动态环境适应）
- [x] 内存优化（缓冲区重用 + 条件扩容）

### Milestone 6: 高级功能（规划中）
- 多音轨混音
- 实时推流（SRT/RTMP）
- GUI 客户端（Qt）

## 📝 开发说明

当前分支：`dev/milestone-1`

### 代码规范

- C++20 标准
- Google C++ Style Guide（部分）
- 使用 clang-format 格式化

### 测试

```bash
# 配置并编译测试
cmake .. -DBUILD_TESTS=ON -DCMAKE_BUILD_TYPE=Debug
make -j4

# 运行所有测试
make test

# 运行单个测试（详细输出）
./build/tests/ffvoice_tests --gtest_filter=WavWriter*
```

### 已实现功能

#### AudioCaptureDevice - 音频采集器
- 基于 PortAudio 的跨平台音频捕获
- 实时流式采集（回调模式）
- 设备枚举和自动选择
- 低延迟配置（256 帧缓冲）
- 支持 mono/stereo
- 可配置采样率（默认 48kHz）

#### WavWriter - WAV 文件写入器
- 手写 RIFF/WAV 格式实现
- 支持 PCM 16-bit 音频
- 支持 mono/stereo
- 可调采样率
- 实时写入支持

#### FlacWriter - FLAC 无损压缩
- 基于 libFLAC 1.5.0
- 实时流式编码
- 可配置压缩级别（0-8，默认 5）
- 压缩比 1.5-3x（取决于音频内容）
- 支持 16/24-bit PCM
- 自动压缩比统计

#### SignalGenerator - 音频信号生成器
- 正弦波生成（可调频率、时长、振幅）
- 静音生成
- 白噪声生成
- 用于测试和调试

#### AudioProcessor - 音频处理框架
**架构设计**：
- 抽象接口 `AudioProcessor` 支持模块化扩展
- `AudioProcessorChain` 处理器链（串联多个处理器）
- 实时处理（在采集回调中）
- 就地处理（in-place）提高效率

**VolumeNormalizer - 音量归一化**：
- 基于 RMS 的自动增益控制
- 平滑增益调整（exponential moving average）
  - Attack time: 0.1s（增益提升速度）
  - Release time: 0.3s（增益下降速度）
- 目标电平：0.3（可配置 0.0-1.0）
- 增益范围：0.1x - 10.0x
- 防止削波和保持一致响度

**HighPassFilter - 高通滤波器**：
- 一阶 IIR 滤波器实现
- 去除低频噪声（呼吸声、麦克风碰撞、环境噪音）
- 默认截止频率：80Hz（可配置）
- 每通道独立状态（支持立体声）
- 滤波器公式：`y[n] = α(y[n-1] + x[n] - x[n-1])`

**RNNoiseProcessor - RNNoise 深度学习降噪** (可选)：
- 基于 Xiph RNNoise 的 RNN 深度学习模型
- 专为语音优化的降噪算法
- 帧大小：480 samples (10ms @48kHz)
- 支持采样率：48kHz, 44.1kHz, 24kHz
- 多声道支持：每通道独立 DenoiseState
- 格式转换：自动处理 int16 ↔ float
- 帧缓冲管理：256 samples → 480 samples
- VAD 选项：可选语音活动检测（实验性）
- CPU 开销：~5-10%（显著低于 WebRTC APM）
- 降噪效果：~20dB（语音场景）

**性能**：
- 实时处理（<10ms 延迟）
- 低 CPU 开销（RNNoise: ~8%）
- 支持 mono/stereo

#### WhisperProcessor - 离线语音识别 (可选)
- 基于 OpenAI Whisper 的 C++ 实现 (whisper.cpp)
- 自动下载和集成 tiny 模型（39MB）
- 支持多种语言识别（中文、英文、自动检测）
- 音频格式自动转换（WAV/FLAC → 16kHz float mono）
- 三种输出格式：
  - 纯文本（无时间戳）
  - SRT 字幕（SubRip 格式）
  - VTT 字幕（WebVTT 格式）
- **性能指标**（Apple M3 Pro, Rosetta 2）：
  - 转写速度：5-75x realtime（取决于音频长度）
  - 内存占用：~272MB（模型 + 计算缓冲区）
  - 准确率：英文 ~8-10% WER，中文 ~12-15% WER
- 推理线程数可配置（默认 4 线程）
- 可选翻译功能（转写 + 翻译成英文）

**性能优化（v0.3.0 新增）**：
- **Whisper 模型选择**：
  - 支持 TINY/BASE/SMALL/MEDIUM/LARGE 模型
  - 灵活平衡速度与精度（10x → 0.5x realtime）
- **性能计时系统**：
  - 详细分段计时（转换/推理/提取）
  - 实时因子 (RTF) 自动计算
  - 性能瓶颈识别
- **VAD 智能优化**：
  - 5 种灵敏度预设（VERY_SENSITIVE → VERY_CONSERVATIVE）
  - 自适应阈值调整（根据环境噪声动态优化）
  - 实时统计（平均 VAD 概率、语音占比）
- **内存优化**：
  - 缓冲区重用（减少 90% 内存分配）
  - 条件扩容（避免不必要的 resize）
  - 降低内存碎片化和 GC 压力

**AudioConverter - 音频格式转换**：
- WAV/FLAC 文件加载
- 采样率转换（48kHz/44.1kHz → 16kHz）
- 格式转换（int16 → float）
- 声道转换（stereo → mono）
- 线性插值重采样

**SubtitleGenerator - 字幕生成**：
- SRT 格式（`00:00:01,500` 时间戳格式）
- VTT 格式（`00:00:01.500` 时间戳格式 + WEBVTT 头）
- 纯文本格式（无时间戳）
- 自动时间戳格式化

#### 测试覆盖
- 39+ 个单元测试用例
- WavWriter 测试（16 个）
- SignalGenerator 测试（23 个）
- Google Test 框架

## 🤝 贡献 / Contributing

我们欢迎并感谢所有形式的贡献！无论是报告 bug、提出新功能、改进文档还是提交代码，都对项目有很大帮助。

We welcome and appreciate all forms of contributions! Whether it's reporting bugs, proposing new features, improving documentation, or submitting code.

### 如何贡献 / How to Contribute

1. 🐛 **报告 Bug** - 使用 [Bug Report 模板](https://github.com/chicogong/ffvoice-engine/issues/new?template=bug_report.md)
2. ✨ **请求功能** - 使用 [Feature Request 模板](https://github.com/chicogong/ffvoice-engine/issues/new?template=feature_request.md)
3. 📝 **改进文档** - 提交 PR 改进 README、docs 或代码注释
4. 💻 **提交代码** - Fork → 开发 → 测试 → PR

### 开发指南 / Development Guide

详细的贡献指南请参阅 [CONTRIBUTING.md](CONTRIBUTING.md)

**快速开始**:
```bash
# 1. Fork 并克隆仓库
git clone https://github.com/YOUR_USERNAME/ffvoice-engine.git

# 2. 创建功能分支
git checkout -b feature/your-feature-name

# 3. 进行开发并测试
cmake -B build -DBUILD_TESTS=ON
make -C build -j$(nproc)
make -C build test

# 4. 格式化代码
./scripts/format.sh

# 5. 提交并推送
git commit -m "feat: add your feature"
git push origin feature/your-feature-name

# 6. 创建 Pull Request
```

### 代码规范 / Code Style

- **语言**: C++20
- **风格指南**: Google C++ Style Guide（变体）
- **格式化工具**: clang-format（配置见 `.clang-format`）
- **静态分析**: clang-tidy（配置见 `.clang-tidy`）
- **提交规范**: [Conventional Commits](https://www.conventionalcommits.org/)

### 行为准则 / Code of Conduct

请遵守我们的 [行为准则](CODE_OF_CONDUCT.md)，营造友好和包容的社区环境。

Please follow our [Code of Conduct](CODE_OF_CONDUCT.md) to maintain a welcoming and inclusive community environment.

---

## 📊 项目状态 / Project Status

- ✅ **Milestone 1**: 基础音频采集和文件保存 - 完成
- ✅ **Milestone 2**: 音频处理增强 (RNNoise) - 完成
- ✅ **Milestone 3**: 离线语音识别 (Whisper ASR) - 完成
- ⏳ **Milestone 4**: 实时语音识别 - 进行中

详见 [CHANGELOG.md](CHANGELOG.md)

---

## 📞 支持与反馈 / Support & Feedback

- 📖 **文档**: [docs/](docs/)
- 💬 **讨论**: [GitHub Discussions](https://github.com/chicogong/ffvoice-engine/discussions)
- 🐛 **Bug 报告**: [GitHub Issues](https://github.com/chicogong/ffvoice-engine/issues)
- 📧 **联系**: chicogong@tencent.com

---

## 📄 许可证 / License

本项目采用 MIT 许可证 - 详见 [LICENSE](LICENSE) 文件。

This project is licensed under the MIT License - see the [LICENSE](LICENSE) file for details.

---

## 🙏 致谢 / Acknowledgments

感谢以下开源项目：

Thanks to the following open-source projects:

- [FFmpeg](https://ffmpeg.org/) - 多媒体处理框架
- [PortAudio](http://www.portaudio.com/) - 跨平台音频 I/O 库
- [FLAC](https://xiph.org/flac/) - 无损音频压缩
- [whisper.cpp](https://github.com/ggerganov/whisper.cpp) - OpenAI Whisper 的 C++ 实现
- [RNNoise](https://github.com/xiph/rnnoise) - 深度学习降噪库
- [Google Test](https://github.com/google/googletest) - C++ 测试框架

---

## ⭐ Star History

如果这个项目对你有帮助，请考虑给我们一个 ⭐ Star!

If this project helps you, please consider giving us a ⭐ Star!

[![Star History Chart](https://api.star-history.com/svg?repos=chicogong/ffvoice-engine&type=Date)](https://star-history.com/#chicogong/ffvoice-engine&Date)

---

<p align="center">
  Made with ❤️ by the ffvoice-engine team
</p>

<p align="center">
  <a href="#top">⬆️ Back to Top</a>
</p>
