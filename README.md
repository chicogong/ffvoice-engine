# ffvoice-engine

> 🎙️ A low-latency C++ voice engine with FFmpeg filters and offline ASR

## 📋 项目介绍

ffvoice-engine 是一个高性能的音频处理引擎，专注于实时音频采集、处理和录制。

### 核心特性（规划）

- ✅ **实时音频采集** - 低延迟麦克风/系统声音捕获
- ⏳ **音频增强处理** - 降噪、回声消除、自动增益
- ⏳ **离线语音识别** - whisper.cpp 集成
- ⏳ **多格式输出** - WAV、FLAC，支持多音轨封装
- ⏳ **实时字幕生成** - 边录边转写

## 🏗️ 当前状态

**Milestone 1**: 基础音频采集和文件保存 (进行中)

- [x] 项目骨架搭建
- [x] CMake 构建系统
- [x] CLI 参数框架
- [ ] 音频采集实现（RtAudio）
- [ ] WAV/FLAC 文件写入（FFmpeg）
- [ ] 基础单元测试

## 🚀 快速开始

### 依赖

- CMake 3.20+
- C++20 编译器（GCC 10+, Clang 12+, MSVC 2019+）
- FFmpeg 4.4+ (libavcodec, libavformat, libavutil, libswresample)

macOS 安装：
```bash
brew install cmake ffmpeg
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

# 列出音频设备（待实现）
./build/ffvoice --list-devices

# 录制 10 秒音频（待实现）
./build/ffvoice -d 0 -t 10 -o recording.wav
```

## 📁 项目结构

```
ffvoice-engine/
├── CMakeLists.txt          # 主构建文件
├── include/ffvoice/        # 公共头文件
│   └── types.h             # 核心类型定义
├── src/                    # 源代码
│   ├── audio/              # 音频采集模块
│   ├── media/              # 媒体编码/封装
│   └── utils/              # 工具类
├── apps/cli/               # CLI 应用
├── tests/                  # 单元测试
├── models/                 # AI 模型文件
└── scripts/                # 辅助脚本
```

## 🛣️ 路线图

### Milestone 1: 基础录制 (当前)
- 音频采集（RtAudio）
- WAV/FLAC 保存（FFmpeg）
- CLI 接口

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
cmake .. -DBUILD_TESTS=ON
make test
```

## 📄 许可证

MIT License

## 🤝 贡献

欢迎提交 Issue 和 Pull Request！
