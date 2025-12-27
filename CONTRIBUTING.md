# 贡献指南 / Contributing Guide

欢迎为 ffvoice-engine 做出贡献！我们非常感谢你的帮助。

Welcome to contribute to ffvoice-engine! We really appreciate your help.

## 📋 目录 / Table of Contents

- [行为准则](#行为准则--code-of-conduct)
- [如何贡献](#如何贡献--how-to-contribute)
- [开发流程](#开发流程--development-workflow)
- [代码规范](#代码规范--code-style)
- [提交规范](#提交规范--commit-conventions)
- [测试要求](#测试要求--testing-requirements)

---

## 行为准则 / Code of Conduct

请阅读并遵守我们的 [行为准则](CODE_OF_CONDUCT.md)。

Please read and follow our [Code of Conduct](CODE_OF_CONDUCT.md).

---

## 如何贡献 / How to Contribute

### 🐛 报告 Bug / Reporting Bugs

1. 在提交 issue 前，请先搜索是否已有相同问题
2. 使用 Bug Report 模板创建 issue
3. 提供详细的复现步骤、环境信息和错误日志

Before submitting an issue, please search for existing issues first. Use the Bug Report template and provide detailed reproduction steps, environment info, and error logs.

### ✨ 提出新功能 / Requesting Features

1. 使用 Feature Request 模板创建 issue
2. 清晰描述功能需求和使用场景
3. 说明为什么这个功能对项目有价值

Use the Feature Request template and clearly describe the feature requirements, use cases, and why it's valuable to the project.

### 📝 改进文档 / Improving Documentation

文档改进同样重要！欢迎提交 PR 改进：
- README.md
- docs/ 目录下的技术文档
- 代码注释
- 示例代码

Documentation improvements are equally important! PRs are welcome for:
- README.md
- Technical docs in docs/
- Code comments
- Example code

---

## 开发流程 / Development Workflow

### 1. Fork 仓库 / Fork the Repository

点击 GitHub 页面右上角的 "Fork" 按钮。

Click the "Fork" button in the upper right corner of the GitHub page.

### 2. 克隆代码 / Clone the Code

```bash
git clone https://github.com/YOUR_USERNAME/ffvoice-engine.git
cd ffvoice-engine
```

### 3. 创建分支 / Create a Branch

```bash
# 功能分支
git checkout -b feature/your-feature-name

# 修复分支
git checkout -b fix/issue-number-description

# 文档分支
git checkout -b docs/improvement-description
```

分支命名规范：
- `feature/` - 新功能
- `fix/` - Bug 修复
- `docs/` - 文档改进
- `refactor/` - 代码重构
- `test/` - 测试相关
- `chore/` - 构建、CI/CD 等

### 4. 安装依赖 / Install Dependencies

```bash
# macOS
brew install cmake ffmpeg portaudio flac

# Ubuntu/Debian
sudo apt-get install cmake build-essential \
  libavcodec-dev libavformat-dev libavutil-dev libswresample-dev \
  portaudio19-dev libflac-dev
```

### 5. 配置和编译 / Configure and Build

```bash
mkdir build && cd build
cmake .. -DCMAKE_BUILD_TYPE=Debug -DBUILD_TESTS=ON
make -j$(nproc)
```

### 6. 进行开发 / Make Changes

- 遵循 [代码规范](#代码规范--code-style)
- 添加单元测试
- 更新相关文档

Follow the [Code Style](#code-style), add unit tests, and update relevant documentation.

### 7. 运行测试 / Run Tests

```bash
# 运行所有测试
make test

# 运行特定测试
./tests/ffvoice_tests --gtest_filter=WavWriter*

# 代码格式检查
./scripts/format-check.sh

# 静态分析
./scripts/lint.sh
```

### 8. 提交代码 / Commit Changes

```bash
git add .
git commit -m "feat: add your feature description"
```

遵循 [提交规范](#提交规范--commit-conventions)。

Follow the [Commit Conventions](#commit-conventions).

### 9. 推送到 Fork / Push to Fork

```bash
git push origin feature/your-feature-name
```

### 10. 创建 Pull Request / Create Pull Request

1. 访问你的 Fork 仓库页面
2. 点击 "New Pull Request"
3. 填写 PR 模板
4. 等待 CI 通过和代码审查

Visit your fork's page, click "New Pull Request", fill in the PR template, and wait for CI to pass and code review.

---

## 代码规范 / Code Style

### C++ 代码规范

我们使用 **Google C++ Style Guide** 的变体，配置在 `.clang-format` 文件中。

We use a variant of the **Google C++ Style Guide**, configured in `.clang-format`.

**关键规则**:

1. **缩进**: 4 个空格（不使用 Tab）
2. **行宽**: 最多 100 字符
3. **命名规范**:
   - 类名: `CamelCase`
   - 函数名: `CamelCase`
   - 变量名: `lower_case`
   - 常量名: `kCamelCase`
   - 成员变量: `lower_case_` (后缀下划线)
   - 宏定义: `UPPER_CASE`

4. **注释**:
   - 使用 `//` 单行注释
   - 使用 `/** */` Doxygen 风格文档注释
   - 重要的算法和逻辑必须添加注释

**示例**:

```cpp
// 好的示例
class AudioProcessor {
public:
    explicit AudioProcessor(const Config& config);

    bool Initialize(int sample_rate, int channels);
    void Process(int16_t* samples, size_t num_samples);

private:
    Config config_;
    int sample_rate_;
    std::vector<float> buffer_;
};

// 不好的示例
class audioprocessor {  // 应该使用 CamelCase
public:
    audioprocessor(Config config);  // 缺少 explicit

    bool init(int SampleRate, int Channels);  // 不一致的命名
    void process(int16_t* Samples, size_t NumSamples);  // 参数名应该用 lower_case

private:
    Config m_config;  // 不要使用 m_ 前缀
    int sampleRate;   // 应该用 sample_rate_
};
```

### 自动格式化

```bash
# 格式化所有代码
./scripts/format.sh

# 仅检查格式（不修改）
./scripts/format-check.sh
```

---

## 提交规范 / Commit Conventions

我们使用 [Conventional Commits](https://www.conventionalcommits.org/) 规范。

We follow the [Conventional Commits](https://www.conventionalcommits.org/) specification.

### 格式 / Format

```
<type>(<scope>): <subject>

<body>

<footer>
```

### 类型 / Types

- `feat`: 新功能 (New feature)
- `fix`: Bug 修复 (Bug fix)
- `docs`: 文档改进 (Documentation)
- `style`: 代码格式（不影响功能）(Code formatting)
- `refactor`: 代码重构 (Refactoring)
- `perf`: 性能优化 (Performance improvement)
- `test`: 测试相关 (Testing)
- `chore`: 构建、CI/CD 等 (Build, CI/CD)

### 示例 / Examples

```bash
# 新功能
feat(audio): add support for 24-bit PCM format

# Bug 修复
fix(wav-writer): correct RIFF chunk size calculation

# 文档
docs(readme): update installation instructions for Windows

# 重构
refactor(processor): extract frame buffering logic into separate class

# 性能优化
perf(rnnoise): reduce memory allocation in processing loop
```

---

## 测试要求 / Testing Requirements

### 单元测试

- 所有新功能必须添加单元测试
- 测试覆盖率应保持在 80% 以上
- 使用 Google Test 框架

**示例**:

```cpp
#include <gtest/gtest.h>
#include "audio/whisper_processor.h"

class WhisperProcessorTest : public ::testing::Test {
protected:
    void SetUp() override {
        processor_ = std::make_unique<WhisperProcessor>();
    }

    std::unique_ptr<WhisperProcessor> processor_;
};

TEST_F(WhisperProcessorTest, Initialize) {
    EXPECT_TRUE(processor_->Initialize());
    EXPECT_TRUE(processor_->IsInitialized());
}
```

### 集成测试

- 重要功能需要添加集成测试
- 测试真实场景和边界条件

### 性能测试

- 性能关键路径需要性能测试
- 记录基准性能指标

---

## 代码审查 / Code Review

所有 PR 都需要经过代码审查。审查重点：

All PRs require code review. Review focuses on:

1. **功能正确性** - 代码是否正确实现了需求
2. **代码质量** - 是否遵循代码规范
3. **测试充分性** - 测试是否覆盖主要场景
4. **文档完整性** - 是否更新了相关文档
5. **性能影响** - 是否有性能退化
6. **兼容性** - 是否破坏现有 API

---

## 发布流程 / Release Process

1. 更新 `CHANGELOG.md`
2. 更新版本号
3. 创建 Git tag: `git tag v1.0.0`
4. 推送 tag: `git push origin v1.0.0`
5. GitHub Actions 自动构建和发布

---

## 获得帮助 / Getting Help

- 📖 阅读 [文档](docs/)
- 💬 在 [Discussions](https://github.com/chicogong/ffvoice-engine/discussions) 提问
- 🐛 在 [Issues](https://github.com/chicogong/ffvoice-engine/issues) 报告问题

---

## 致谢 / Acknowledgments

感谢所有贡献者！你们的贡献让这个项目变得更好。

Thanks to all contributors! Your contributions make this project better.

---

**最后更新**: 2025-12-27
