# Build System Specification v3.0

> **TL;DR:** `scripts/build.py` compiles C99 → `libsol.so`. SPIR-V shaders assembled via `spirv-as` + embedded as C arrays. Targets: Linux (SDL3+Vulkan), Raspberry Pi (ALSA headless), Windows, Android. Dependencies: SDL3, Vulkan SDK, spirv-tools, Python 3.10+.

**Targets:** Linux (x86_64), Raspberry Pi (arm64), Windows (x86_64), Android (arm64)

---

## 1. Source File Map

| Component | Files |
|-----------|-------|
| Engine core | `src/sol/core.h`, `src/sol/core.c` |
| Platform abstraction | `src/sol/io/io.h` |
| SDL3 backend | `src/sol/io/io_sdl3.c` |
| Headless/ALSA backend | `src/sol/io/io.c` |
| Vulkan renderer | `src/sol/photon/photon_vulkan.h`, `src/sol/photon/photon_vulkan.c` |
| Shaders | `src/sol/shaders/` → `src/sol/shaders.h` (generated) |
| Node system | `src/sol/scene/node.h`, `src/sol/scene/node.c` |
| Control | `src/sol/scene/control.h`, `src/sol/scene/control.c` |
| DrawList | `src/sol/scene/draw_list.h`, `src/sol/scene/draw_list.c` |
| SceneTree | `src/sol/scene/scene_tree.h`, `src/sol/scene/scene_tree.c` |
| ColorRect | `src/sol/scene/color_rect.h`, `src/sol/scene/color_rect.c` |
| VBoxContainer | `src/sol/scene/vbox_container.h`, `src/sol/scene/vbox_container.c` |
| HBoxContainer | `src/sol/scene/hbox_container.h`, `src/sol/scene/hbox_container.c` |
| MarginContainer | `src/sol/scene/margin_container.h`, `src/sol/scene/margin_container.c` |
| CenterContainer | `src/sol/scene/center_container.h`, `src/sol/scene/center_container.c` |
| Math types | `src/sol/scene/types.h` |
| Audio system *(Phase 1)* | `src/sol/audio/` |
| Text system *(Phase 3)* | `src/sol/text/` |
| Debug system *(Phase 3)* | `src/sol/debug/` |

---

## 2. Build Script (`scripts/build.py`)

### 2.1 Shader Assembly

SPIR-V assembly sources (`src/sol/shaders/*.vert`, `*.frag`) → `.spv` binary via `spirv-as` → embedded as C byte arrays in `src/sol/shaders.h`.

### 2.2 C Compilation

```bash
gcc -std=c99 -O3 -fPIC -shared \
    -I src/sol \
    $(pkg-config --cflags --libs sdl3) \
    src/sol/core.c \
    src/sol/io/io_sdl3.c \
    src/sol/io/io.c \
    src/sol/photon/photon_vulkan.c \
    src/sol/scene/node.c \
    src/sol/scene/control.c \
    src/sol/scene/draw_list.c \
    src/sol/scene/scene_tree.c \
    src/sol/scene/color_rect.c \
    src/sol/scene/vbox_container.c \
    -o src/sol/python/libsol.so \
    -lvulkan -lm
```

Audio, text, and debug sources are added as they are implemented.

### 2.3 Incremental Build

File modification timestamps checked. If `libsol.so` is newer than all sources, build is skipped.

---

## 3. Dependencies

### Build-time

| Dependency | Purpose |
|------------|---------|
| GCC or Clang (C99) | C compilation |
| SDL3 ≥ 3.2 | Windowing, Vulkan surface, audio |
| Vulkan SDK ≥ 1.0 | Headers + loader |
| spirv-as (from spirv-tools) | Shader assembly |
| Python ≥ 3.10 | Build script + runtime |

### Runtime

| Dependency | Notes |
|------------|-------|
| libSDL3.so | Dynamic link |
| libvulkan.so | Dynamic link |
| libasound.so | ALSA (headless mode, Linux only) |
| libm.so | Math library |

---

## 4. Developer Commands

```bash
# Full build
python3 scripts/build.py

# Rebuild from clean
rm -f src/sol/python/libsol.so && python3 scripts/build.py

# Run with local engine
PYTHONPATH=src python3 game/main.py

# Run UI example
PYTHONPATH=src python3 examples/ui_example.py

# Run Neptune (headless audio)
PYTHONPATH=src python3 neptune/main.py --headless

# Run Neptune (GUI)
PYTHONPATH=src python3 neptune/main.py --gui

# Install as editable package
uv pip install -e .
```

---

## 5. Raspberry Pi Notes

- ALSA audio requires `libasound2-dev`
- Vulkan: Pi 4+ supports Vulkan 1.2 via v3dv (Mesa). May require `mesa-vulkan-drivers`
- SDL3: install from source or distro package if available
- MIDI: `sudo apt install librtmidi-dev` for python-rtmidi

---

*Specification version 3.0*
