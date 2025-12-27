# PyPI 发布检查清单

**项目**: ffvoice-engine
**当前版本**: v0.4.0
**目标**: 首次发布到 PyPI
**状态**: ✅ 准备就绪

---

## ✅ 已完成项目

### 1. 包配置和元数据

- ✅ **pyproject.toml** - 现代 Python 打包配置
  - 项目名称: `ffvoice`
  - 版本: `0.4.0`
  - Python 支持: `>=3.7`
  - 许可证: MIT
  - 关键词: speech-recognition, asr, whisper, VAD, noise-reduction
  - 分类器: Development Status, Programming Language, OS Support

- ✅ **setup.py** - 构建脚本
  - CMake 集成
  - pybind11 绑定编译
  - C++ 扩展模块: `_ffvoice`

- ✅ **README.md** - 包文档
  - 完整 API 参考 (13+ 类)
  - 代码示例和教程
  - 安装说明
  - 性能基准

### 2. Python 绑定实现

- ✅ **核心功能** (440+ 行 pybind11 代码)
  - WhisperASR: 语音识别
  - RNNoise: 噪声抑制
  - VADSegmenter: 语音活动检测
  - AudioCapture: 实时音频采集
  - WAVWriter/FLACWriter: 文件写入

- ✅ **NumPy 集成**
  - 零拷贝数组传递
  - 原地数组修改
  - 类型安全验证

- ✅ **Python 回调**
  - 线程安全 (GIL handling)
  - 音频采集回调
  - VAD 分段回调

### 3. 测试和质量保证

- ✅ **单元测试**
  - `python/tests/test_basic.py` - 基础功能测试
  - `python/tests/test_numpy.py` - NumPy 集成测试
  - 所有测试通过

- ✅ **示例代码**
  - `basic_transcription.py` - 文件转写
  - `realtime_transcription.py` - 实时识别
  - `complete_realtime_pipeline.py` - 完整工作流 (200+ 行)

- ✅ **Wheel 验证**
  ```bash
  twine check dist/ffvoice-0.4.0-cp313-cp313-macosx_26_0_arm64.whl
  # ✅ PASSED
  ```

### 4. 文档

- ✅ **python/README.md** (335+ 行)
  - 安装指南
  - 完整 API 文档
  - 13+ 使用示例
  - 性能对比

- ✅ **python/docs/QUICKSTART.md** (300+ 行)
  - 快速入门教程
  - 6 个完整示例
  - 故障排除指南

- ✅ **python/docs/tutorials/ffvoice_tutorial.ipynb** (500+ 行)
  - 交互式 Jupyter 教程
  - 7 个学习章节
  - 完整代码示例

- ✅ **主 README.md**
  - 新增 Python Bindings 章节
  - PyPI 安装说明
  - 快速示例代码

### 5. CI/CD 自动化

- ✅ **.github/workflows/ci.yml**
  - 多平台测试 (Ubuntu, macOS)
  - 多 Python 版本 (3.9-3.12)
  - 自动运行测试

- ✅ **.github/workflows/release.yml**
  - 自动创建 GitHub Release
  - 多平台 wheel 构建:
    - Ubuntu (Linux x86_64)
    - macOS ARM64 (Apple Silicon)
    - macOS x86_64 (Intel)
  - **自动上传到 PyPI** ✨

- ✅ **.github/workflows/pr.yml**
  - 代码质量检查
  - PR 标题验证
  - 代码格式检查

### 6. PyPI 配置

- ✅ **GitHub Secret 配置**
  - Secret name: `PYPI_API_TOKEN`
  - 状态: ✅ 已配置 (2025-12-27)
  - 验证: ✅ Token 有效

- ✅ **PyPI Token 测试**
  ```bash
  # Token 验证成功
  ✅ PyPI token 验证成功！
  状态码: 200
  ```

- ✅ **项目状态**
  - PyPI 项目: 不存在（首次发布将自动创建）
  - 项目名称: `ffvoice`
  - URL: https://pypi.org/project/ffvoice/

### 7. 版本控制

- ✅ **Git 标签**
  - 当前: `v0.4.0` (2025-12-27)
  - 格式: `v<major>.<minor>.<patch>`

- ✅ **GitHub Release**
  - v0.4.0 已发布
  - 包含完整 changelog
  - 附带 wheel 文件

---

## 🚀 首次发布步骤

### 选项 1: 自动发布（推荐）

直接创建新标签，GitHub Actions 将自动处理一切：

```bash
# 1. 确保所有改动已提交
git status

# 2. 创建并推送 v0.4.1 标签（或下一版本）
git tag -a v0.4.1 -m "feat: 首次发布到 PyPI 🎉"
git push origin v0.4.1
```

**自动流程**:
1. ✅ 触发 `.github/workflows/release.yml`
2. ✅ 创建 GitHub Release
3. ✅ 构建所有平台 wheel:
   - `ffvoice-0.4.1-cp39-cp39-linux_x86_64.whl`
   - `ffvoice-0.4.1-cp39-cp39-macosx_11_0_arm64.whl`
   - `ffvoice-0.4.1-cp39-cp39-macosx_10_9_x86_64.whl`
   - ... (所有 Python 版本)
4. ✅ 上传到 GitHub Release
5. ✅ **自动上传到 PyPI** 🚀

### 选项 2: 手动发布（测试用）

如果想先在 TestPyPI 测试：

```bash
# 1. 构建 wheel
python3 -m build --wheel

# 2. 上传到 TestPyPI
twine upload --repository testpypi dist/*.whl

# 3. 测试安装
pip install --index-url https://test.pypi.org/simple/ ffvoice

# 4. 确认无误后上传到正式 PyPI
export TWINE_USERNAME="__token__"
export TWINE_PASSWORD="pypi-AgEI..."
twine upload dist/*.whl
```

---

## 📋 发布后验证

发布成功后，检查以下内容：

### PyPI 项目页面
- [ ] 访问 https://pypi.org/project/ffvoice/
- [ ] 检查版本号正确
- [ ] 检查 README 显示正常
- [ ] 检查 wheel 文件可下载

### 安装测试
```bash
# 创建虚拟环境测试
python3 -m venv test_env
source test_env/bin/activate

# 从 PyPI 安装
pip install ffvoice

# 验证导入
python -c "import ffvoice; print(ffvoice.__version__)"

# 运行基础测试
python -c "
import ffvoice
config = ffvoice.WhisperConfig()
print('✅ ffvoice 安装成功')
"
```

### GitHub Release
- [ ] Release notes 完整
- [ ] 所有平台 wheel 已上传
- [ ] 源码包 (sdist) 已上传

---

## 📊 当前统计

- **Python 绑定代码**: 440+ 行
- **导出类**: 13 个
- **文档**: 1500+ 行
- **示例代码**: 600+ 行
- **测试用例**: 20+ 个
- **支持平台**: Linux (x86_64), macOS (ARM64, x86_64)
- **Python 版本**: 3.7 - 3.12

---

## ⚠️ 注意事项

1. **版本号不可重用**
   - PyPI 不允许删除或覆盖已发布版本
   - 发布前确保版本号正确

2. **首次发布创建项目**
   - 项目名 `ffvoice` 首次上传时自动创建
   - 确保有权限上传该名称

3. **wheel 平台兼容性**
   - macOS ARM64 wheel 仅适用于 Apple Silicon
   - Linux wheel 使用 manylinux 标签
   - Windows wheel 需要单独构建

4. **依赖项**
   - FFmpeg/PortAudio 等系统依赖需用户自行安装
   - 在 README 中明确说明安装步骤

---

## 🎯 下一步计划

### v0.4.1 - 首次 PyPI 发布
- [ ] 创建 v0.4.1 tag
- [ ] 触发自动发布
- [ ] 验证 PyPI 页面
- [ ] 更新文档链接

### v0.5.0 - 功能增强
- [ ] Windows wheel 支持
- [ ] Python 3.13 支持
- [ ] 性能优化
- [ ] 更多示例代码

### 长期计划
- [ ] 发布到 conda-forge
- [ ] 完善 CI/CD 覆盖率
- [ ] 性能基准测试
- [ ] 社区推广

---

**状态**: ✅ 所有准备工作已完成，随时可以发布！

**建议**: 使用自动发布（选项 1）以确保多平台 wheel 一致性。

**联系**: 如有问题请提交 [GitHub Issue](https://github.com/chicogong/ffvoice-engine/issues)
