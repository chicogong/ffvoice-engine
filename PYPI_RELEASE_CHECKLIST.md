# PyPI Release Checklist

**Project**: ffvoice-engine
**PyPI Package**: `ffvoice`
**Current Version**: v0.6.1
**PyPI URL**: https://pypi.org/project/ffvoice/

---

## Published Platforms (v0.6.1)

| Platform | Wheel Tag | Python | Status |
|----------|-----------|--------|--------|
| Linux x86_64 | manylinux_2_39_x86_64 | 3.9–3.12 | Published |
| macOS ARM64 | macosx_11_0_arm64 | 3.9–3.12 | Published |
| Windows x86_64 | win_amd64 | 3.9–3.12 | Published |
| Source distribution | sdist | any | Published |

**Note**: macOS Intel (x86_64) wheels are not published; Intel Mac users must build from source.
**Note**: Windows wheels ship without RNNoise (MSVC VLA incompatibility).

---

## Pre-release Checklist

### 1. Version Consistency
- [ ] `pyproject.toml` version matches target release tag
- [ ] `python/ffvoice/__init__.py` `__version__` matches
- [ ] `CMakeLists.txt` project version matches
- [ ] CHANGELOG.md entry drafted for new version

### 2. Python Bindings
- [ ] Core bindings compile on Linux, macOS ARM64, Windows
- [ ] NumPy integration tests pass (`python/tests/test_numpy.py`)
- [ ] Basic import test passes: `python -c "import ffvoice; print(ffvoice.__version__)"`
- [ ] New API surface tested if applicable (AudioMixer, RingBuffer, word timestamps)

### 3. CI/CD Checks
- [ ] `ci.yml` passes on all platforms (Ubuntu, macOS, Windows)
- [ ] `pr.yml` code quality checks pass
- [ ] Release workflow (`release.yml`) builds all wheels successfully before tagging

### 4. Documentation
- [ ] `python/README.md` reflects current API
- [ ] Root `README.md` Python section is up to date
- [ ] CHANGELOG.md has an entry for the new version

---

## Release Steps (Automated)

Tag-triggered releases are fully automated via `.github/workflows/release.yml`:

```bash
# 1. Ensure all changes are committed and CI passes on master
git status

# 2. Create and push the version tag
git tag -a v0.6.2 -m "Release v0.6.2"
git push origin v0.6.2
```

GitHub Actions will automatically:
1. Create a GitHub Release with release notes
2. Build wheels for Linux x86_64, macOS ARM64, Windows x86_64 (Python 3.9–3.12)
3. Build the sdist
4. Upload all artifacts to GitHub Release
5. Upload to PyPI using the `PYPI_API_TOKEN` secret

---

## Post-release Verification

After the release workflow completes:

```bash
# Wait a few minutes for PyPI to index the new version, then:
pip install --upgrade ffvoice

# Verify installed version
python -c "import ffvoice; print(ffvoice.__version__)"

# Basic smoke test
python -c "
import ffvoice
config = ffvoice.WhisperConfig()
print('WhisperConfig OK')
ring = ffvoice.RingBuffer(1024)
print('RingBuffer OK:', ring.capacity())
"
```

### GitHub Release Page
- [ ] Release notes are complete
- [ ] All platform wheels are attached (12 wheels + 1 sdist = 13 files for a full release)
- [ ] Tag points to the correct commit

### PyPI Project Page
- [ ] Version is visible at https://pypi.org/project/ffvoice/
- [ ] README renders correctly
- [ ] Correct number of wheel files listed under "Download files"

---

## Manual Release (fallback)

If the automated workflow fails, build and upload manually:

```bash
# Build wheel (macOS example)
python3 -m build --wheel

# Check the wheel
twine check dist/*.whl

# Upload to PyPI
export TWINE_USERNAME="__token__"
export TWINE_PASSWORD="pypi-AgEI..."
twine upload dist/*.whl dist/*.tar.gz
```

---

## Important Notes

1. **Version numbers cannot be reused** — PyPI does not allow deleting or overwriting published versions. Increment the version for every release, even for hotfixes.
2. **Token security** — The `PYPI_API_TOKEN` GitHub secret must have upload scope for the `ffvoice` project. Rotate the token if it is ever exposed.
3. **Test on TestPyPI first** (optional but recommended for major versions):
   ```bash
   twine upload --repository testpypi dist/*.whl
   pip install --index-url https://test.pypi.org/simple/ ffvoice==<version>
   ```
4. **Windows RNNoise limitation** — Windows wheels intentionally omit RNNoise. Do not attempt to enable it without first replacing the VLA-dependent C code in RNNoise with MSVC-compatible alternatives.

---

## Dependency Reference (system deps for source builds)

| Dependency | Min Version | macOS | Linux | Windows |
|------------|-------------|-------|-------|---------|
| CMake | 3.20+ | brew | apt | vcpkg / choco |
| FFmpeg | 4.4+ | brew | apt | vcpkg |
| PortAudio | 19.7+ | brew | apt | vcpkg |
| FLAC | 1.5+ | brew | apt | vcpkg |
| whisper.cpp | auto | FetchContent | FetchContent | FetchContent |
| RNNoise | auto | FetchContent | FetchContent | disabled |

---

**Status**: Automated CI/CD pipeline active. Tag a new version to trigger a release.

**Contact**: [GitHub Issues](https://github.com/chicogong/ffvoice-engine/issues)
