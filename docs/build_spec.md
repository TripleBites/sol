# Build System Specification: Cross-Platform Python & C99 Engine

**Document Version:** 1.0.0

**Target Audience:** AI Coding Assistants & Human Contributors

**Primary Language Stack:** C99, Python 3.10+

**Graphics / Windowing API:** SDL3, Vulkan 1.0+

---

## 1. System Philosophy & Architecture

This project deliberately avoids meta-build systems like CMake or Meson. Instead, it relies strictly on **Python native tooling (`setuptools`)** as the universal build engine for C code across all desktop and mobile targets.

### Key Principles

1. **Tooling Minimization:** Developers and AI assistants must only interact with **C** and **Python** tools.
2. **Unified C-Extension Entrypoint:** The core C engine (`src/sol.c`) is compiled as a native Python C-extension module (`engine`). Python loads it directly via standard module imports (`import engine`), avoiding fragile `ctypes` paths across OS file structures.
3. **Dual Execution Pipeline:**
* **Desktop (Linux/Windows):** Managed via `uv` for local virtual environments and compiled into native standalone binaries using **Nuitka**.
* **Mobile (Android):** Cross-compiled via **Buildozer** (`python-for-android`), which invokes the Android NDK and passes target environment variables (`$CC`, `$CFLAGS`, `$LDFLAGS`) into `setup.py`.



```text
                        ┌──► Linux Desktop ──► uv + Nuitka ─────► Standalone Executable
                        │
src/sol.c ──► setup.py ──┼──► Windows Desktop ──► uv + Nuitka ─────► Standalone Executable (.exe)
 (C99 + SDL3 + Vulkan)  │
                        └──► Android APK ────► Buildozer (NDK) ──► Native Shared Library (.so) inside APK

```

---

## 2. Directory Structure Requirements

All contributors and AI agents must preserve this layout. Do not introduce root-level CMakeLists.txt or custom build shell scripts unless explicitly specified.

```text
my_game_engine/
├── BUILD_SPEC.md           # This specification document
├── buildozer.spec          # Mobile target configuration (Buildozer / p4a)
├── pyproject.toml          # Desktop project dependencies & build-backend (uv)
├── setup.py                # C-Extension build configuration (replaces CMake)
├── main.py                 # Application entry point
└── src/
    ├── sol.h            # C99 Engine API header
    ├── sol.c            # C99 Engine implementation & Python C API bindings
    └── sol.py          # (Optional) High-level Python helper class

```

---

## 3. Toolchain & Prerequisites

### Desktop Host (Linux / Windows)

* **C99 Compiler:** GCC, Clang, or MSVC.
* **Python Environment:** `uv` installed (`curl -sSf [https://astral.sh/uv/install.sh](https://astral.sh/uv/install.sh) | sh`).
* **Graphics Libraries:**
* **SDL3:** Shared library installed system-wide or available in header/library paths.
* **Vulkan SDK:** Installed on the host system (LunarG Vulkan SDK on Windows; system Vulkan drivers and headers on Linux).



### Mobile Target (Android)

* **Buildozer:** Installed inside a Linux environment (or WSL2 on Windows).
* **Android NDK:** Automatically fetched by Buildozer (minimum target API level **24** for Vulkan support).

---

## 4. Build Configuration Specifications

### A. C Extension Build (`setup.py`)

`setup.py` acts as the single source of truth for building `src/sol.c`.

#### Compilation Logic

* **Standard:** C99 (`-std=c99`), High Optimization (`-O3`).
* **Include Directories:** Local `src/` folder.
* **Platform Linking Table:**

| Target OS | Linked Libraries | Extra Include / Library Paths |
| --- | --- | --- |
| **Linux Desktop** | `SDL3`, `vulkan` | Standard system paths (`/usr/include`, `/usr/lib`) |
| **Windows Desktop** | `SDL3`, `vulkan-1` | Appends `%VULKAN_SDK%/Include` and `%VULKAN_SDK%/Lib` |
| **Android (p4a)** | `SDL3`, `vulkan` | Resolved automatically via NDK sysroot (API level ≥ 24) |

#### Implemented Specification (`setup.py`)

```python
import os
import sys
from setuptools import Extension, setup

IS_WINDOWS = sys.platform == "win32"
IS_ANDROID = "ANDROID_ARGUMENT" in os.environ or "aarch64-linux-android" in os.environ.get("CC", "")

libraries = ["SDL3"]
include_dirs = ["src"]
library_dirs = []
extra_compile_args = ["-std=c99", "-O3"]

if IS_WINDOWS:
    vulkan_sdk = os.environ.get("VULKAN_SDK")
    if vulkan_sdk:
        include_dirs.append(os.path.join(vulkan_sdk, "Include"))
        library_dirs.append(os.path.join(vulkan_sdk, "Lib"))
    libraries.append("vulkan-1")
else:
    libraries.append("vulkan")

engine_module = Extension(
    "engine",
    sources=["src/sol.c"],
    include_dirs=include_dirs,
    library_dirs=library_dirs,
    libraries=libraries,
    extra_compile_args=extra_compile_args,
)

setup(
    name="engine",
    version="0.1.0",
    ext_modules=[engine_module],
)

```

---

### B. Host Dependency Management (`pyproject.toml`)

Managed strictly via `uv` for local developer virtual environments.

```toml
[project]
name = "my_game_engine"
version = "0.1.0"
description = "Cross-platform C99/Python SDL3+Vulkan Engine"
readme = "README.md"
requires-python = ">=3.10"
dependencies = [
    "numpy",
    "pandas",
    "torch",
]

[build-system]
requires = ["setuptools"]
build-backend = "setuptools.build_meta"

```

---

### C. Android Packaging (`buildozer.spec`)

Requires Android NDK API level **24** minimum to guarantee native Vulkan system symbol availability (`libvulkan.so`).

```ini
[app]
title = Vulkan Engine
package.name = vulkanengine
package.domain = org.example

source.dir = .
source.include_exts = py,png,jpg,so

# Requirements: Must include C-extension host dependencies
requirements = python3, numpy, pandas, hostpython3

# Target Architectures
android.archs = arm64-v8a, x86_64

# Vulkan Requirements
android.minapi = 24
android.ndk_api = 24

```

---

## 5. Developer Command Reference Cheat Sheet

| Task | Command | Description |
| --- | --- | --- |
| **Environment Setup** | `uv venv && source .venv/bin/activate` | Initializes local Python virtual environment. |
| **Install Dependencies** | `uv pip install -e .` | Installs project requirements and compiles `engine` extension. |
| **Compile Extension (Local)** | `python setup.py build_ext --inplace` | Recompiles C changes to `engine.so` or `engine.pyd` in-place. |
| **Run Desktop Application** | `python main.py` | Runs Python entrypoint loading the local C extension. |
| **Build Desktop Binary** | `python -m nuitka --standalone --enable-plugin=numpy --include-module=engine main.py` | Compiles Python script and C extension into native standalone executable. |
| **Build Android Debug APK** | `buildozer android debug` | Cross-compiles C extension and packages APK using NDK. |
| **Deploy & Run on Device** | `buildozer android debug deploy run` | Builds, flashes to connected USB device, and executes. |
| **View Live Device Logs** | `buildozer android logcat | grep -E "python|SDL"` | Filters device log stream for Python/SDL output. |

---

## 6. Guidelines for AI Coding Assistants

When tasked with generating, updating, or refactoring code within this project, AI agents must adhere to the following strict constraints:

> 1. **Do Not Add Meta-Build Tools:** Never propose adding CMake (`CMakeLists.txt`), Meson, SCons, or Makefile files. All C library configuration must remain inside `setup.py`.
> 2. **Maintain Pure C99 Compliance:** All C code inside `src/sol.c` and `src/sol.h` must adhere strictly to C99 standards (`-std=c99`). Do not use C++ features or vendor-specific language extensions unless wrapped in preprocessor checks.
> 3. **Preserve C API Extension Module Interface:** The C engine must expose its public functions to Python via standard `<Python.h>` binding structures (`PyMethodDef`, `PyModuleDef`, `PyInit_engine`). Do not replace this with raw `ctypes` pointers unless explicitly instructed.
> 4. **Keep Vulkan Surface Creation Delegated to SDL3:** Always use `SDL_Vulkan_GetInstanceExtensions` and `SDL_Vulkan_CreateSurface` inside `sol.c` to maintain cross-platform window surface compatibility between X11/Wayland (Linux), Win32 (Windows), and ANativeWindow (Android).
> 5. **Environment Variable Integrity:** In `setup.py`, always check for `ANDROID_ARGUMENT` or `CC` environment variables to detect when `p4a` is running cross-compilation for Android.
> 
>