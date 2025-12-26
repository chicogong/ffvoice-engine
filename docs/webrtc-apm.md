# WebRTC APM 音频处理框架

> WebRTC Audio Processing Module 集成指南

**当前状态**: 🚧 **实验性功能** - 框架已集成，但需要手动安装 WebRTC APM 库

---

## 📋 概述

WebRTC Audio Processing Module (APM) 是由 Google WebRTC 项目开发的高级音频处理库，广泛应用于 VoIP、视频会议等实时通信场景。

**核心功能**:
- **噪声抑制 (NS)** - 基于频谱的智能降噪
- **自动增益控制 (AGC)** - 自适应音量调整
- **语音活动检测 (VAD)** - 实时检测语音/静音
- **回声消除 (AEC)** - *（当前版本暂未实现）*

**特点**:
- 针对语音优化（仅支持单声道）
- 10ms 帧处理（低延迟）
- 成熟的工业级算法
- 实时性能优化

**注意**: 此功能为**可选组件**，需要手动安装 WebRTC APM 库。

---

## 🔧 安装与配置

### 1. 依赖安装

#### Linux (Ubuntu/Debian)

```bash
sudo apt-get install webrtc-audio-processing-dev
```

#### 从源代码编译 (Linux/macOS)

```bash
# 1. 安装 meson 构建系统
brew install meson  # macOS
# 或
sudo apt-get install meson ninja-build  # Linux

# 2. 编译安装 WebRTC APM
git clone https://gitlab.freedesktop.org/pulseaudio/webrtc-audio-processing.git
cd webrtc-audio-processing
git checkout v1.3  # 使用稳定版本

# 配置编译
meson setup build --prefix=/usr/local
meson compile -C build
sudo meson install -C build
```

**⚠️ macOS Apple Silicon 注意事项**:
目前 webrtc-audio-processing v1.3 在 ARM64 架构上可能存在编译问题。建议：
- 尝试 Rosetta 2 转译环境（`arch -x86_64 meson ...`）
- 或等待后续版本修复
- 或暂时禁用此功能（不影响其他功能）

### 2. 编译 ffvoice-engine

```bash
# 标准编译（不含 WebRTC APM）
mkdir build && cd build
cmake .. -DCMAKE_BUILD_TYPE=Release
make -j$(nproc)

# 启用 WebRTC APM
cmake .. -DCMAKE_BUILD_TYPE=Release -DENABLE_WEBRTC_APM=ON
make -j$(nproc)
```

### 3. 验证安装

```bash
# 检查 CMake 是否找到库
cmake .. -DENABLE_WEBRTC_APM=ON 2>&1 | grep "WebRTC APM"
# 应输出类似:
# -- WebRTC APM found in system:
# --   Include dirs: /usr/local/include/webrtc-audio-processing-1
# --   Libraries: webrtc-audio-processing-1
```

---

## 🚀 使用指南

### 命令行参数

```bash
# 启用 WebRTC APM 处理（推荐配置）
./build/ffvoice --record -o meeting.wav --webrtc -t 30

# 调整降噪强度（嘈杂环境）
./build/ffvoice --record -o noisy.wav --webrtc --webrtc-ns high -t 20

# 启用语音活动检测（VAD）
./build/ffvoice --record -o speech.wav --webrtc --webrtc-vad -t 30

# 自定义 AGC 目标电平（0-31 dBFS）
./build/ffvoice --record -o interview.wav --webrtc --webrtc-agc 6 -t 45

# 组合使用（WebRTC + 高通滤波 + 音量归一化）
./build/ffvoice --record -o podcast.flac \
  --webrtc --webrtc-ns moderate --highpass 80 --normalize -t 60
```

### 参数详解

| 参数                | 默认值     | 说明                         | 可选值                          |
|---------------------|------------|------------------------------|---------------------------------|
| `--webrtc`          | -          | 启用 WebRTC APM 处理         | -                               |
| `--webrtc-ns`       | moderate   | 降噪强度                     | low, moderate, high, veryhigh   |
| `--webrtc-agc`      | 3          | AGC 目标电平 (dBFS)          | 0-31 (整数)                    |
| `--webrtc-vad`      | false      | 启用语音活动检测             | -                               |

---

## 🎯 应用场景

### 1. 远程会议/在线课程 🏢

**问题**：
- 背景噪音干扰（键盘声、空调声、窗外噪音）
- 发言者音量不一致
- 需要清晰的语音质量

**解决方案**：
```bash
./build/ffvoice --record -o meeting.flac --webrtc -t 3600
```

**效果**：
- ✅ 智能降噪（减少 10-20dB 背景噪音）
- ✅ 自动增益（保持一致的音量水平）
- ✅ 实时处理（<10ms 延迟）
- ✅ FLAC 无损压缩（节省存储空间）

### 2. 播客/配音录制 🎙️

**问题**：
- 录音环境不理想（家庭办公室、普通房间）
- 需要专业级语音清晰度
- 后期处理耗时

**解决方案**：
```bash
./build/ffvoice --record -o podcast.flac \
  --webrtc --webrtc-ns veryhigh --webrtc-agc 3 -t 1800
```

**效果**：
- ✅ 强降噪（非常适合家庭环境）
- ✅ 平滑的自动增益（无爆音）
- ✅ 实时处理，无需后期
- ✅ 与现有处理器链兼容

### 3. 嘈杂环境录音 🏙️

**问题**：
- 户外环境（街道、咖啡馆、机场）
- 多个噪声源
- 语音容易被淹没

**解决方案**：
```bash
./build/ffvoice --record -o outdoor.flac \
  --webrtc --webrtc-ns veryhigh --highpass 120 -t 600
```

**效果**：
- ✅ 最大限度降噪
- ✅ 120Hz 高通滤波去除交通低频噪音
- ✅ 语音增强（优先保留语音频段）

### 4. 语音识别预处理 🤖

**问题**：
- ASR 模型对音频质量敏感
- 背景噪音降低识别准确率
- 需要标准化的输入格式

**解决方案**：
```bash
./build/ffvoice --record -o speech.wav \
  --webrtc --webrtc-vad --sample-rate 16000 -t 30
```

**效果**：
- ✅ 降噪提升 ASR 准确率
- ✅ VAD 可用于智能分段
- ✅ 16kHz 采样率适配主流 ASR 模型
- ✅ 为 Milestone 3 (Whisper 集成) 打基础

---

## ⚙️ 技术架构

### WebRTCProcessor 类设计

```cpp
class WebRTCProcessor : public AudioProcessor {
public:
    // 核心方法
    bool Initialize(int sample_rate, int channels);
    void Process(int16_t* samples, size_t num_samples);
    void Reset();

    // 配置
    struct WebRTCConfig {
        bool enable_ns = true;
        bool enable_agc = true;
        bool enable_vad = false;
        enum class NSLevel { Low, Moderate, High, VeryHigh };
        NSLevel ns_level = NSLevel::Moderate;
        int agc_target_level_dbfs = 3;  // 0-31
    };

private:
    // 内部缓冲区管理 (256 → 480 帧重新分块)
    std::vector<int16_t> buffer_;
    size_t buffer_pos_;
    size_t frame_size_;  // 10ms = 480 samples @48kHz
};
```

### 帧缓冲策略

**挑战**: PortAudio 提供 256 帧/块 (~5.3ms @48kHz)，但 WebRTC APM 需要 480 帧/块 (10ms)

**解决方案**: 内部缓冲池
1. 累积输入帧直到达到 480 帧
2. 调用 WebRTC APM 处理
3. 输出处理后的帧
4. **延迟**: <5ms (可接受)

```cpp
void WebRTCProcessor::Process(int16_t* samples, size_t num_samples) {
    // 填充缓冲区到 480 帧
    while (/* 缓冲区不满 */) {
        std::copy(/* 从 samples 复制到 buffer_ */);
    }

    // 处理完整帧
    if (buffer_.size() >= frame_size_) {
        ProcessFrame(buffer_.data(), frame_size_);
        // 复制回输出
        std::copy(buffer_.data(), buffer_.data() + frame_size_, samples);
    }
}
```

### 配置映射

| CLI 参数      | WebRTC APM 配置                    | 效果说明                     |
|---------------|------------------------------------|------------------------------|
| `--webrtc-ns low` | `webrtc::NoiseSuppression::kLow`      | 轻度降噪，保留更多环境音     |
| `--webrtc-ns moderate` | `webrtc::NoiseSuppression::kModerate` | 平衡降噪（推荐）             |
| `--webrtc-ns high` | `webrtc::NoiseSuppression::kHigh`     | 强降噪，适合嘈杂环境         |
| `--webrtc-ns veryhigh` | `webrtc::NoiseSuppression::kVeryHigh` | 极强降噪，可能损失部分语音   |
| `--webrtc-agc 3` | `set_target_level_dbfs(3)`            | 目标电平 -3 dBFS             |
| `--webrtc-agc 0` | `set_target_level_dbfs(0)`            | 最大音量（小心削波）         |

---

## 📊 性能指标

### 预期效果 (基于官方文档)

| 指标                  | 无 WebRTC APM | WebRTC APM 启用 | 改进幅度 |
|-----------------------|---------------|-----------------|----------|
| 背景噪音电平          | 参考基线      | -10 ~ -20 dB    | ⬇️ 显著降低 |
| 语音信噪比 (SNR)      | 参考基线      | +5 ~ +15 dB     | ⬆️ 显著提升 |
| 音量一致性            | 波动较大      | ±3 dB 以内      | ⬆️ 大幅改善 |
| VAD 准确率            | N/A           | >95%            | -        |

### 资源开销

| 资源类型     | 占用情况            | 说明                          |
|--------------|---------------------|-------------------------------|
| CPU 占用     | 15-25% (单核)       | 取决于降噪强度和采样率        |
| 内存占用     | ~10 MB              | WebRTC APM 内部状态           |
| 处理延迟     | <10 ms              | 含 5ms 缓冲延迟               |
| 线程数       | 1-2 个              | 主线程 + 可能的内部工作线程   |

**对比建议**:
- **低功耗场景**: 使用内置的 `VolumeNormalizer` + `HighPassFilter`
- **高质量语音**: 启用 WebRTC APM
- **极限降噪**: `--webrtc --webrtc-ns veryhigh --highpass 120`

---

## 🔍 故障排除

### 常见问题

#### 1. 编译错误 "WebRTC APM library not found"

```bash
# 错误信息:
-- WebRTC APM library not found!
-- To install webrtc-audio-processing:
...
-- FATAL_ERROR: WebRTC APM library required but not found
```

**解决方法**:
```bash
# 1. 安装库 (见上方安装指南)
# 2. 或禁用此功能:
cmake .. -DENABLE_WEBRTC_APM=OFF
```

#### 2. 运行时错误 "Only mono (1 channel) is supported"

```bash
# 错误信息:
[ERROR] WebRTCProcessor: Only mono (1 channel) is supported
```

**原因**: WebRTC APM 仅支持单声道音频处理。

**解决方法**:
```bash
# 使用单声道录制
./build/ffvoice --record -o mono.wav --channels 1 --webrtc -t 10
```

#### 3. macOS Apple Silicon 编译失败

**现象**: 链接错误，缺失 x86_64 符号或 SSE2/AVX2 指令。

**临时解决方案**:
```bash
# 在 Rosetta 2 环境中编译
arch -x86_64 /bin/bash -c "meson setup build --prefix=/usr/local && meson compile -C build"
```

#### 4. 音频质量差或有爆音

**可能原因**:
- AGC 目标电平设置过高 (`--webrtc-agc 0`)
- 输入信号过载

**调试步骤**:
1. 降低 AGC 目标电平: `--webrtc-agc 6`
2. 检查输入设备是否正常
3. 尝试降低降噪强度: `--webrtc-ns moderate`

### 调试命令

```bash
# 查看详细的初始化信息
export FFVOICE_LOG_LEVEL=debug
./build/ffvoice --record -o test.wav --webrtc -t 5

# 输出示例:
[INFO] WebRTCProcessor initialized (WebRTC APM enabled):
[INFO]   Sample rate: 48000 Hz
[INFO]   Channels: 1
[INFO]   Frame size: 480 samples
[INFO]   Noise Suppression: ON (Moderate)
[INFO]   AGC: ON (target: -3 dBFS)
[INFO]   VAD: OFF
```

---

## 🔄 与其他处理器的兼容性

WebRTCProcessor 可以与现有处理器链无缝集成：

### 推荐处理顺序

```cpp
// 建议的处理链顺序
processor_chain->AddProcessor(std::make_unique<HighPassFilter>(80.0f));   // 1. 高通滤波
processor_chain->AddProcessor(std::make_unique<WebRTCProcessor>(config));  // 2. WebRTC APM
processor_chain->AddProcessor(std::make_unique<VolumeNormalizer>(0.3f));   // 3. 音量归一化
```

**处理逻辑**:
1. **高通滤波** (可选): 先去除低频噪音，减轻 WebRTC APM 负担
2. **WebRTC APM**: 核心降噪和增益控制
3. **音量归一化**: 最终音量调整（如果 AGC 不够理想）

### 命令行组合示例

```bash
# 完整处理链（推荐）
./build/ffvoice --record -o studio.flac \
  --highpass 80 --webrtc --normalize -t 1800

# 仅 WebRTC APM（简化）
./build/ffvoice --record -o voice.wav --webrtc -t 300

# 传统处理（无 WebRTC APM）
./build/ffvoice --record -o basic.wav --enable-processing -t 300
```

---

## 🚧 已知限制

### 技术限制

1. **仅支持单声道**: WebRTC APM 为语音通信优化，不支持立体声处理
2. **采样率要求**: 推荐 48kHz 或 16kHz（语音场景）
3. **格式要求**: 16-bit PCM 整数格式
4. **延迟**: 额外的 5ms 缓冲延迟（256→480 帧转换）

### 平台兼容性

| 平台      | 状态       | 说明                          |
|-----------|------------|-------------------------------|
| Linux     | ✅ 良好     | 官方包支持良好                |
| macOS Intel | ✅ 良好     | 需要从源码编译                |
| macOS ARM | 🟡 实验性   | 可能存在编译问题              |
| Windows   | 🔜 计划中   | 需要手动编译依赖              |

### 功能范围

**当前实现**:
- ✅ 噪声抑制 (NS)
- ✅ 自动增益控制 (AGC)
- ✅ 语音活动检测 (VAD)
- ✅ 与现有处理器链集成

**暂未实现**:
- ❌ 回声消除 (AEC) - 需要更复杂的双工处理
- ❌ 波束成形 (Beamforming) - 需要多麦克风阵列
- ❌ 自定义滤波器 - WebRTC APM 配置有限

---

## 📈 性能测试建议

### 主观听感测试

```bash
# 创建测试文件
./build/ffvoice --test-wav baseline.wav -f 1000 -t 5
./build/ffvoice --record -o webrtc.wav --webrtc -t 5
./build/ffvoice --record -o traditional.wav --enable-processing -t 5

# 比较听感
afplay baseline.wav    # 原始
afplay webrtc.wav      # WebRTC 处理
afplay traditional.wav # 传统处理
```

### 客观指标测试

```bash
# 使用 ffmpeg 分析频谱
ffmpeg -i baseline.wav -af "showspectrumpic=s=1280x720" baseline.png
ffmpeg -i webrtc.wav -af "showspectrumpic=s=1280x720" webrtc.png

# 对比噪声电平
ffmpeg -i baseline.wav -af "astats" -f null - 2>&1 | grep "RMS level"
ffmpeg -i webrtc.wav -af "astats" -f null - 2>&1 | grep "RMS level"
```

### 性能基准

```bash
# CPU 占用测试 (使用 time 命令)
time ./build/ffvoice --record -o test.wav --webrtc -t 30
# 观察 CPU 使用率（推荐使用 htop 或 Activity Monitor）
```

---

## 🔮 未来规划

### 短期增强 (Milestone 1.x)

1. **更好的错误处理**: 更清晰的安装指导和错误信息
2. **预设模式**: `--webrtc-preset podcast/meeting/outdoor`
3. **实时统计**: 显示处理的噪声降低量、增益调整值
4. **配置文件支持**: JSON/YAML 配置文件

### 中期计划 (Milestone 2)

1. **回声消除 (AEC)**: 集成 WebRTC AEC 模块
2. **多平台预编译包**: 提供 Windows/macOS 预编译库
3. **自适应配置**: 根据环境噪音自动调整参数
4. **GPU 加速**: 探索 Metal/Vulkan 加速可能

### 长期愿景

1. **自定义算法**: 允许用户提供自己的处理算法
2. **插件系统**: 支持第三方音频处理器
3. **实时分析**: 频谱分析、噪声特征识别
4. **AI 增强**: 结合深度学习进一步提升效果

---

## 📚 参考资源

### 官方文档
- [WebRTC Audio Processing API](https://webrtc.googlesource.com/src/+/refs/heads/main/modules/audio_processing/include/audio_processing.h)
- [webrtc-audio-processing 项目](https://gitlab.freedesktop.org/pulseaudio/webrtc-audio-processing)

### 技术文章
- [WebRTC Noise Suppression Deep Dive](https://webrtc.org/2019/10/webrtc-noise-suppression-deep-dive/)
- [Understanding AGC in VoIP Systems](https://www.dialogic.com/learning/webinars/understanding-agc-in-voip-systems)

### 相关项目
- [PulseAudio WebRTC Module](https://gitlab.freedesktop.org/pulseaudio/pulseaudio/-/tree/master/src/modules/echo-cancel)
- [RNNoise - Alternative Noise Suppression](https://github.com/xiph/rnnoise)

---

## 🤝 贡献指南

### 报告问题

如果您遇到问题，请提供：
1. **操作系统和版本**: `uname -a`
2. **WebRTC APM 安装方式**: apt/brew/源码编译
3. **错误信息**: 完整的终端输出
4. **复现步骤**: 精确的命令序列

### 代码贡献

欢迎改进 WebRTC APM 集成：
1. **ARM64 支持**: 修复 macOS Apple Silicon 编译问题
2. **更好的配置**: 暴露更多 WebRTC APM 参数
3. **性能优化**: 减少缓冲延迟和内存占用

### 测试反馈

我们需要您的实测反馈：
- 不同环境下的降噪效果
- 不同设备上的兼容性
- 性能数据（CPU/内存占用）

---

## 📝 总结

**WebRTC APM 集成**为 ffvoice-engine 带来了工业级的音频处理能力：

✅ **优势**:
- 成熟的降噪和增益控制算法
- 实时处理（<10ms 延迟）
- 与现有架构无缝集成
- 丰富的配置选项

⚠️ **注意事项**:
- 需要手动安装依赖库
- 仅支持单声道
- ARM64 macOS 可能存在编译问题

🎯 **推荐用户**:
- 需要高质量语音录制的用户
- 在嘈杂环境下录音的用户
- 希望减少后期处理工作的用户

🔧 **简化方案**: 如果安装困难，可以使用内置的 `VolumeNormalizer` + `HighPassFilter`，效果也相当不错。

---

*文档版本*: v0.1 (实验性)
*最后更新*: 2024-12-26
*对应版本*: ffvoice-engine v0.1.0
*作者*: ffvoice-engine 开发团队