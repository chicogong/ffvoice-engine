---
name: Bug Report / Bug 报告
about: Create a report to help us improve / 创建 Bug 报告帮助我们改进
title: '[BUG] '
labels: bug
assignees: ''

---

## 🐛 Bug 描述 / Bug Description

<!-- 清晰简洁地描述这个 Bug -->
<!-- A clear and concise description of what the bug is -->

## 🔄 复现步骤 / Steps to Reproduce

1.
2.
3.
4.

## ✅ 预期行为 / Expected Behavior

<!-- 描述你期望发生什么 -->
<!-- A clear and concise description of what you expected to happen -->

## ❌ 实际行为 / Actual Behavior

<!-- 描述实际发生了什么 -->
<!-- A clear and concise description of what actually happened -->

## 📸 截图 / Screenshots

<!-- 如果适用，添加截图来帮助解释问题 -->
<!-- If applicable, add screenshots to help explain your problem -->

## 🖥️ 环境信息 / Environment

**操作系统 / OS**:
- [ ] macOS (版本: )
- [ ] Linux (发行版: , 版本: )
- [ ] Windows (版本: )

**编译器 / Compiler**:
- [ ] GCC (版本: )
- [ ] Clang (版本: )
- [ ] MSVC (版本: )

**版本信息 / Version**:

请提供您正在使用的 ffvoice-engine 版本号（如 `v0.6.1`）以及获取方式（PyPI / 源码编译 / GitHub Release）。
Please provide the ffvoice-engine version you are using (e.g. `v0.6.1`) and how you obtained it (PyPI wheel / built from source / GitHub Release).

```bash
# Python 用户 / Python users:
python -c "import ffvoice; print(ffvoice.__version__)"

# 从源码编译的用户，请查看 CMakeLists.txt 顶部或 CHANGELOG.md 中的版本号
# Users who built from source: check the version at the top of CMakeLists.txt or in CHANGELOG.md
```

**编译选项 / Build Options**:
```bash
# 粘贴你的 cmake 配置命令
# Paste your cmake configuration command
cmake .. -DCMAKE_BUILD_TYPE=Release ...
```

**依赖版本 / Dependencies**:
- FFmpeg:
- PortAudio:
- FLAC:
- RNNoise: [ ] 已启用 / Enabled
- Whisper: [ ] 已启用 / Enabled

## 📋 错误日志 / Error Logs

```
粘贴相关的错误日志
Paste relevant error logs here
```

## 🔍 额外信息 / Additional Context

<!-- 添加任何其他相关信息 -->
<!-- Add any other context about the problem here -->

## ✅ 检查清单 / Checklist

- [ ] 我已经搜索过现有的 issues / I have searched existing issues
- [ ] 我使用的是最新版本 / I am using the latest version
- [ ] 我已经阅读了文档 / I have read the documentation
- [ ] 我提供了完整的环境信息 / I have provided complete environment information
- [ ] 我提供了可复现的步骤 / I have provided reproducible steps
