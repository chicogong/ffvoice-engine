"""
Setup configuration for ffvoice Python package
"""

import os
import sys
import subprocess
import platform
from pathlib import Path

from setuptools import setup, Extension
from setuptools.command.build_ext import build_ext


class CMakeExtension(Extension):
    """Custom Extension class for CMake-based builds"""

    def __init__(self, name, sourcedir=""):
        Extension.__init__(self, name, sources=[])
        self.sourcedir = os.path.abspath(sourcedir)


class CMakeBuild(build_ext):
    """Custom build command that uses CMake"""

    def build_extension(self, ext):
        if not isinstance(ext, CMakeExtension):
            super().build_extension(ext)
            return

        extdir = os.path.abspath(os.path.dirname(self.get_ext_fullpath(ext.name)))

        # Create build directory
        if not os.path.exists(self.build_temp):
            os.makedirs(self.build_temp)

        # CMake configuration arguments
        cmake_args = [
            f"-DCMAKE_LIBRARY_OUTPUT_DIRECTORY={extdir}",
            f"-DPYTHON_EXECUTABLE={sys.executable}",
            "-DBUILD_PYTHON=ON",
            "-DBUILD_TESTS=OFF",
            "-DBUILD_EXAMPLES=OFF",
            "-DENABLE_RNNOISE=ON",
            "-DENABLE_WHISPER=ON",
        ]

        # Build type (Release or Debug)
        cfg = "Debug" if self.debug else "Release"
        cmake_args.append(f"-DCMAKE_BUILD_TYPE={cfg}")

        # Platform-specific configuration
        if platform.system() == "Darwin":  # macOS
            # Build for native architecture (arm64 on Apple Silicon, x86_64 on Intel)
            # Note: Universal2 builds require universal2 dependencies which adds complexity
            arch = platform.machine()
            cmake_args.append(f"-DCMAKE_OSX_ARCHITECTURES={arch}")
            # Use macOS 11.0 for ARM64 (Big Sur+), 10.9 for x86_64
            deployment_target = "11.0" if arch == "arm64" else "10.9"
            cmake_args.append(f"-DCMAKE_OSX_DEPLOYMENT_TARGET={deployment_target}")

        # Build arguments
        build_args = ["--config", cfg]

        # Parallel build
        if hasattr(self, "parallel") and self.parallel:
            build_args.extend(["--", f"-j{self.parallel}"])
        else:
            build_args.extend(["--", "-j4"])

        env = os.environ.copy()
        env["CXXFLAGS"] = f'{env.get("CXXFLAGS", "")} -DVERSION_INFO=\\"{self.distribution.get_version()}\\"'

        # Run CMake configuration
        print(f"Running CMake in {self.build_temp}...")
        subprocess.check_call(
            ["cmake", ext.sourcedir] + cmake_args,
            cwd=self.build_temp,
            env=env,
        )

        # Run CMake build
        print(f"Building extension {ext.name}...")
        subprocess.check_call(
            ["cmake", "--build", ".", "--target", "_ffvoice"] + build_args,
            cwd=self.build_temp,
        )


# Read README for long description
readme_path = Path(__file__).parent / "README.md"
long_description = ""
if readme_path.exists():
    with open(readme_path, encoding="utf-8") as f:
        long_description = f.read()

# Read version from __init__.py
version = "0.4.0"
init_path = Path(__file__).parent / "python" / "ffvoice" / "__init__.py"
if init_path.exists():
    with open(init_path, encoding="utf-8") as f:
        for line in f:
            if line.startswith("__version__"):
                version = line.split("=")[1].strip().strip('"').strip("'")
                break

setup(
    name="ffvoice",
    version=version,
    author="ffvoice-engine contributors",
    author_email="",
    description="High-performance offline speech recognition library for Python",
    long_description=long_description,
    long_description_content_type="text/markdown",
    url="https://github.com/chicogong/ffvoice-engine",
    project_urls={
        "Bug Reports": "https://github.com/chicogong/ffvoice-engine/issues",
        "Source": "https://github.com/chicogong/ffvoice-engine",
    },
    packages=["ffvoice"],
    package_dir={"": "python"},
    ext_modules=[CMakeExtension("ffvoice._ffvoice")],
    cmdclass={"build_ext": CMakeBuild},
    zip_safe=False,
    python_requires=">=3.8",
    classifiers=[
        "Development Status :: 3 - Alpha",
        "Intended Audience :: Developers",
        "Intended Audience :: Science/Research",
        "Topic :: Multimedia :: Sound/Audio :: Speech",
        "Topic :: Scientific/Engineering :: Artificial Intelligence",
        "Programming Language :: Python :: 3",
        "Programming Language :: Python :: 3.8",
        "Programming Language :: Python :: 3.9",
        "Programming Language :: Python :: 3.10",
        "Programming Language :: Python :: 3.11",
        "Programming Language :: Python :: 3.12",
        "Programming Language :: C++",
        "Operating System :: MacOS :: MacOS X",
        "Operating System :: POSIX :: Linux",
    ],
    keywords="speech-recognition asr whisper voice-activity-detection noise-reduction rnnoise offline-transcription real-time-audio",
    install_requires=[
        # No Python dependencies required for the native module
        # Users can optionally install numpy for advanced features
    ],
    extras_require={
        "dev": [
            "pytest>=7.0",
            "pytest-cov>=4.0",
            "black>=23.0",
            "flake8>=6.0",
            "mypy>=1.0",
        ],
        "numpy": [
            "numpy>=1.20",
        ],
    },
)
