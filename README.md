# ffvoice-engine

[![License: MIT](https://img.shields.io/badge/License-MIT-yellow.svg)](https://opensource.org/licenses/MIT)
[![C++20](https://img.shields.io/badge/C%2B%2B-20-blue.svg)](https://en.cppreference.com/w/cpp/20)
[![CMake](https://img.shields.io/badge/CMake-3.20+-064F8C.svg?logo=cmake)](https://cmake.org/)
[![Platform](https://img.shields.io/badge/Platform-macOS%20|%20Linux%20|%20Windows-lightgrey.svg)]()
[![GitHub stars](https://img.shields.io/github/stars/chicogong/ffvoice-engine?style=social)](https://github.com/chicogong/ffvoice-engine/stargazers)

[![FFmpeg](https://img.shields.io/badge/FFmpeg-4.4+-007808.svg?logo=ffmpeg)](https://ffmpeg.org/)
[![PortAudio](https://img.shields.io/badge/PortAudio-19.7+-8B0000.svg)](http://www.portaudio.com/)
[![FLAC](https://img.shields.io/badge/FLAC-1.5+-orange.svg)](https://xiph.org/flac/)
[![Google Test](https://img.shields.io/badge/Google%20Test-1.14+-4285F4.svg?logo=google)](https://github.com/google/googletest)

> A low-latency C++ voice engine with FFmpeg filters and offline ASR

## 📋 项目介绍

ffvoice-engine 是一个高性能的音频处理引擎，专注于实时音频采集、处理和录制。

### 核心特性（规划）

- ✅ **实时音频采集** - 低延迟麦克风/系统声音捕获 (PortAudio)
- ✅ **多格式输出** - WAV、FLAC 无损压缩
- ✅ **音频增强处理** - 音量归一化、高通滤波
- ⏳ **离线语音识别** - whisper.cpp 集成
- ⏳ **实时字幕生成** - 边录边转写

## 🏗️ 当前状态

**Milestone 1**: 基础音频采集和文件保存 (✨ 95% 完成)

- [x] 项目骨架搭建
- [x] CMake 构建系统
- [x] CLI 参数框架
- [x] **WAV 文件写入** (手写 RIFF 格式)
- [x] **FLAC 无损压缩** (libFLAC, 压缩比 2-3x)
- [x] **音频采集** (PortAudio, 实时流式捕获)
- [x] **音频信号生成器** (正弦波、静音、白噪声)
- [x] **设备枚举与选择**
- [x] **音频处理模块** (音量归一化 + 高通滤波)
- [x] **单元测试** (39 个测试用例)
- [x] VSCode 开发环境配置
- [x] Google Test 测试框架集成
- [ ] WebRTC APM 音频处理 (可选)

## 🚀 快速开始

### 依赖

- CMake 3.20+
- C++20 编译器（GCC 10+, Clang 12+, MSVC 2019+）
- FFmpeg 4.4+ (libavcodec, libavformat, libavutil, libswresample)
- PortAudio 19.7+ (音频采集)
- FLAC 1.5+ (无损压缩)

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

### 编译

```bash
mkdir build && cd build
cmake .. -DCMAKE_BUILD_TYPE=Release
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

# 播放录音
afplay recording.wav   # 或 recording.flac
```

## 📁 项目结构

```
ffvoice-engine/
├── CMakeLists.txt          # 主构建文件
├── include/ffvoice/        # 公共头文件
│   └── types.h             # 核心类型定义
├── src/                    # 源代码
│   ├── audio/              # 音频采集与处理模块
│   │   ├── audio_capture_device.* # ✅ PortAudio 采集器
│   │   └── audio_processor.*      # ✅ 音频处理框架
│   ├── media/              # 媒体编码/封装
│   │   ├── wav_writer.*    # ✅ WAV 文件写入器
│   │   ├── flac_writer.*   # ✅ FLAC 无损压缩
│   │   └── audio_file_writer.* # FFmpeg 封装器（待实现）
│   └── utils/              # 工具类
│       ├── signal_generator.* # ✅ 音频信号生成
│       ├── ring_buffer.*   # ✅ 环形缓冲区
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

### Milestone 1: 基础录制 (当前 - ✨ 95% 完成)
- [x] WAV 文件写入（手写 RIFF 格式）
- [x] FLAC 无损压缩（libFLAC）
- [x] 音频采集（PortAudio 集成）
- [x] 音频信号生成器（测试用）
- [x] 音频处理框架（音量归一化 + 高通滤波）
- [x] CLI 完整功能（设备、格式、参数）
- [x] 单元测试覆盖（39 个测试用例）
- [ ] WebRTC APM 音频处理（可选）

### Milestone 2: 音频处理
- RNNoise 降噪
- WebRTC APM（可选）
- 实时处理管道

### Milestone 3: 语音识别
- whisper.cpp 集成
- VAD 智能分段
- SRT/VTT 字幕生成

### Milestone 4: 高级功能
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

**性能**：
- 实时处理（零额外延迟）
- 低 CPU 开销
- 支持 mono/stereo

#### 测试覆盖
- 39 个单元测试用例
- WavWriter 测试（16 个）
- SignalGenerator 测试（23 个）
- Google Test 框架

## 📄 许可证

MIT License

## 🤝 贡献

欢迎提交 Issue 和 Pull Request！
