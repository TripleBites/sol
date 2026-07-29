import os
import sys
from setuptools import Extension, setup

# Platform detection
IS_WINDOWS = sys.platform == "win32"
# Buildozer sets ANDROID_ARGUMENT or CC containing android cross-compilers
IS_ANDROID = "ANDROID_ARGUMENT" in os.environ or "aarch64-linux-android" in os.environ.get("CC", "")

# Base configuration
libraries = ["SDL3"]
include_dirs = ["src"]
library_dirs = []
extra_compile_args = ["-std=c99", "-O3"]

# Platform-specific Vulkan linking
if IS_WINDOWS:
    # On Windows, locate the LunarG Vulkan SDK via environment variable
    vulkan_sdk = os.environ.get("VULKAN_SDK")
    if vulkan_sdk:
        include_dirs.append(os.path.join(vulkan_sdk, "Include"))
        library_dirs.append(os.path.join(vulkan_sdk, "Lib"))
    libraries.append("vulkan-1")  # Links vulkan-1.lib
else:
    # Linux & Android NDK both expose standard -lvulkan
    libraries.append("vulkan")

sol_module = Extension(
    "sol",
    sources=["src/sol.c"],
    include_dirs=include_dirs,
    library_dirs=library_dirs,
    libraries=libraries,
    extra_compile_args=extra_compile_args,
)

setup(
    name="sol",
    version="0.1",
    ext_modules=[sol_module],
)