"""Build the Sol engine shared library and install the Python package."""
import subprocess
import sys
from pathlib import Path

from setuptools import setup
from setuptools.command.build_ext import build_ext as _build_ext


ROOT = Path(__file__).resolve().parent


class BuildEngine(_build_ext):
    """Custom build_ext that compiles the C99 engine as a shared library."""
    def run(self):
        build_script = ROOT / "scripts" / "build_engine.py"
        subprocess.run([sys.executable, str(build_script)], check=True)
        # Don't call super().run() - we handle C building ourselves


setup(
    name="sol",
    version="0.1.0",
    description="Sol Engine - C99 SDL3 + Vulkan game engine with Python bindings",
    packages=["sol"],
    package_dir={"sol": "src/sol"},
    package_data={"sol": ["libsol.so", "libsol.dylib", "sol.dll"]},
    cmdclass={"build_ext": BuildEngine},
)
