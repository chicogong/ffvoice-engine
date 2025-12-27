# GitHub Actions Workflows

这个目录包含了 ffvoice-engine 项目的所有 GitHub Actions workflow 配置。

## Workflows 概览

### 1. CI Workflow (`ci.yml`)

**触发条件**: 
- Push to `master` 或 `develop` 分支
- Pull Request to `master` 或 `develop` 分支

**功能**:
- 在多个平台（Ubuntu, macOS）和多个 Python 版本（3.9-3.12）上构建和测试
- 编译 C++ 核心库
- 运行 C++ 单元测试
- 构建 Python bindings
- 测试 Python 包导入
- 运行 Python 测试
- 构建 wheel 包并作为 artifact 保存

**Matrix 策略**:
```yaml
os: [ubuntu-latest, macos-latest]
python-version: ['3.9', '3.10', '3.11', '3.12']
```

### 2. Release Workflow (`release.yml`)

**触发条件**:
- 创建新的 tag (格式: `v*`，如 `v0.4.0`)

**功能**:
- 自动创建 GitHub Release
- 在多个平台构建 wheel 包:
  - Ubuntu (Linux x86_64)
  - macOS ARM64 (Apple Silicon)
  - macOS x86_64 (Intel)
- 构建源码分发包 (sdist)
- 上传 wheel 到 GitHub Release
- (可选) 上传到 PyPI

**使用方法**:
```bash
# 创建新 release
git tag -a v0.5.0 -m "Release v0.5.0"
git push origin v0.5.0

# Workflow 将自动:
# 1. 创建 GitHub Release
# 2. 构建所有平台的 wheel
# 3. 上传 wheel 到 Release
```

### 3. Pull Request Workflow (`pr.yml`)

**触发条件**:
- Pull Request 打开、同步或重新打开

**功能**:
- 代码质量检查 (black, flake8, mypy)
- PR 标题格式检查 (Conventional Commits)
- PR 大小检查
- TODO/FIXME 注释检查
- 自动添加标签

**PR 标题格式要求**:
```
type(scope): description

类型 (type):
- feat: 新功能
- fix: Bug 修复
- docs: 文档更新
- style: 代码格式化
- refactor: 重构
- test: 测试相关
- chore: 构建/工具相关
- perf: 性能优化
- ci: CI/CD 相关
- build: 构建系统
- revert: 回退

示例:
- feat(python): add NumPy array support
- fix(vad): resolve segmentation fault
- docs: update README with examples
```

## 本地测试

### 测试 CI Workflow

```bash
# 安装依赖
brew install ffmpeg portaudio flac cmake ninja  # macOS
# 或
sudo apt-get install libavcodec-dev libavformat-dev ...  # Ubuntu

# 构建 C++ 库
mkdir -p build && cd build
cmake .. -DCMAKE_BUILD_TYPE=Release -DBUILD_TESTS=ON -DBUILD_PYTHON=OFF -DENABLE_RNNOISE=ON -DENABLE_WHISPER=ON -GNinja
ninja

# 运行 C++ 测试
./tests/ffvoice_tests --gtest_brief=1

# 构建 Python 包
cd ..
pip install -e .

# 测试 Python 导入
python -c "import ffvoice; print(ffvoice.__version__)"

# 运行 Python 测试
pytest python/tests -v
```

### 测试代码格式

```bash
# Python 代码格式化
pip install black flake8 mypy
black --check python/
flake8 python/
mypy python/ --ignore-missing-imports

# C++ 代码格式化
find src include -name '*.cpp' -o -name '*.h' | xargs clang-format -i
```

### 本地构建 Wheel

```bash
pip install build
python -m build --wheel

# Wheel 将生成在 dist/ 目录
ls dist/*.whl
```

## Secrets 配置

### PyPI 发布 (可选)

如果要启用自动 PyPI 发布，需要配置以下 secret:

1. 在 PyPI 创建 API token
2. 在 GitHub repository settings 添加 secret:
   - Name: `PYPI_API_TOKEN`
   - Value: `pypi-...` (你的 PyPI API token)

配置后，每次创建新 tag 时会自动上传到 PyPI。

## Workflow 状态 Badge

在 README 中添加 workflow 状态徽章:

```markdown
[![CI](https://github.com/chicogong/ffvoice-engine/actions/workflows/ci.yml/badge.svg)](https://github.com/chicogong/ffvoice-engine/actions/workflows/ci.yml)
[![Release](https://github.com/chicogong/ffvoice-engine/actions/workflows/release.yml/badge.svg)](https://github.com/chicogong/ffvoice-engine/actions/workflows/release.yml)
```

## 故障排除

### Workflow 失败常见原因

1. **依赖安装失败**
   - 检查系统依赖是否正确安装
   - 查看 workflow 日志中的错误信息

2. **测试失败**
   - 本地运行测试确保通过
   - 检查 Python 版本兼容性

3. **Wheel 构建失败**
   - 检查 setup.py 和 pyproject.toml 配置
   - 确保 CMake 配置正确

4. **PyPI 上传失败**
   - 检查 PYPI_API_TOKEN secret 是否正确配置
   - 确保版本号未被使用

### 查看 Workflow 日志

访问: https://github.com/chicogong/ffvoice-engine/actions

点击具体的 workflow run 查看详细日志。

## 维护

### 更新依赖版本

定期更新 workflow 中使用的 actions 版本:
- `actions/checkout@v4` → 最新版本
- `actions/setup-python@v5` → 最新版本
- `actions/upload-artifact@v4` → 最新版本

### 添加新的测试平台

编辑 `ci.yml` 的 matrix:
```yaml
matrix:
  os: [ubuntu-latest, macos-latest, windows-latest]  # 添加 Windows
  python-version: ['3.9', '3.10', '3.11', '3.12', '3.13']  # 添加新版本
```

## 参考资料

- [GitHub Actions 文档](https://docs.github.com/en/actions)
- [Python 打包指南](https://packaging.python.org/)
- [Conventional Commits](https://www.conventionalcommits.org/)
