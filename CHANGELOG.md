# 变更日志 / Changelog

All notable changes to this project will be documented in this file.

The format is based on [Keep a Changelog](https://keepachangelog.com/en/1.0.0/),
and this project adheres to [Semantic Versioning](https://semver.org/spec/v2.0.0.html).

## [Unreleased]

### 计划中 / Planned
- 实时语音识别（VAD 分段）
- 多音轨混音
- GUI 客户端

---

## [0.3.0] - 2025-12-27

### 新增 / Added
- ✨ **Whisper ASR 离线语音识别**
  - whisper.cpp 集成（CMake FetchContent 自动下载）
  - Whisper Tiny 模型支持（39MB）
  - 音频格式自动转换（WAV/FLAC → 16kHz float mono）
  - 离线转写功能（TranscribeFile API）
  - 三种字幕格式输出（纯文本/SRT/VTT）
  - CLI 参数集成（--transcribe, --format, --language）
  - 多语言支持（中文/英文/自动检测）

- 📦 **新增组件**
  - `WhisperProcessor`: Whisper ASR 核心处理器
  - `AudioConverter`: 音频格式转换工具（重采样、声道转换）
  - `SubtitleGenerator`: 字幕格式生成器（SRT/VTT/文本）

- 📚 **文档**
  - `docs/whisper-asr.md`: Whisper ASR 完整技术文档（450+ 行）
  - README 更新：添加 Whisper 使用说明和性能指标

### 改进 / Improved
- 📈 **性能优化**
  - 转写速度：5.8x ~ 75x realtime（取决于音频长度）
  - 内存优化：~272MB 峰值内存（模型 + 计算缓冲区）

- 🛠️ **技术改进**
  - logger.h: 添加 LOG_INFO/ERROR/WARNING 宏
  - CMakeLists.txt: 完善 ENABLE_WHISPER 选项和依赖管理

### 修复 / Fixed
- 🐛 修复 Rosetta 2 上的 AVX 指令集编译错误
- 🐛 修复 LOG_* 宏未定义的编译错误

---

## [0.2.0] - 2025-12-27

### 新增 / Added
- ✨ **RNNoise 深度学习降噪**
  - 基于 Xiph RNNoise 的 RNN 模型
  - 实时降噪处理（~20dB 降噪效果）
  - 低 CPU 开销（~8-10%）
  - 帧缓冲管理（256 → 480 samples）
  - 多声道支持（每通道独立处理）
  - VAD 功能（实验性）

- 📦 **新增组件**
  - `RNNoiseProcessor`: RNNoise 降噪处理器
  - `AudioProcessorChain`: 音频处理器链

- 📚 **文档**
  - `docs/rnnoise.md`: RNNoise 完整技术文档
  - `docs/audio-processing.md`: 音频处理框架文档

### 改进 / Improved
- 🎵 **音频处理增强**
  - 音量归一化（基于 RMS）
  - 高通滤波器（去除低频噪声）
  - 处理器链支持（组合多个处理器）

---

## [0.1.0] - 2025-12-23

### 新增 / Added
- 🎙️ **音频采集**
  - PortAudio 集成
  - 实时流式采集
  - 设备枚举和选择
  - 低延迟配置（256 帧缓冲）
  - Mono/Stereo 支持

- 💾 **文件保存**
  - WAV 文件写入（手写 RIFF 格式）
  - FLAC 无损压缩（libFLAC）
  - 可配置压缩级别（0-8）
  - 实时写入支持

- 🔧 **工具和测试**
  - 音频信号生成器（正弦波、静音、白噪声）
  - 环形缓冲区
  - CLI 参数框架
  - Google Test 测试框架（39 个测试用例）

- 📚 **文档**
  - README.md: 项目主文档
  - LICENSE: MIT 许可证
  - VSCode 开发环境配置

### 技术栈 / Tech Stack
- C++20
- CMake 3.20+
- FFmpeg 4.4+
- PortAudio 19.7+
- FLAC 1.5+
- Google Test 1.14+

---

## 版本说明 / Versioning

版本号格式：`MAJOR.MINOR.PATCH`

- **MAJOR**: 不兼容的 API 变更
- **MINOR**: 向后兼容的新功能
- **PATCH**: 向后兼容的 Bug 修复

Version format: `MAJOR.MINOR.PATCH`

- **MAJOR**: Incompatible API changes
- **MINOR**: Backward-compatible new features
- **PATCH**: Backward-compatible bug fixes

---

[Unreleased]: https://github.com/chicogong/ffvoice-engine/compare/v0.3.0...HEAD
[0.3.0]: https://github.com/chicogong/ffvoice-engine/compare/v0.2.0...v0.3.0
[0.2.0]: https://github.com/chicogong/ffvoice-engine/compare/v0.1.0...v0.2.0
[0.1.0]: https://github.com/chicogong/ffvoice-engine/releases/tag/v0.1.0
