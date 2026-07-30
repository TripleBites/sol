# Build System Specification

**Version:** 2.0  
**Targets:** Linux (x86_64), Windows (x86_64), Android (arm64-v8a, x86_64)  

---

## 1. Build Philosophy

No CMake, Meson, or Makefiles. The build system uses:

- **`scripts/build_engine.py`** — single Python script that does everything
- **`setup.py`** — setuptools shim (invokes build script via custom `build_ext`)
- **`spirv-as`** — SPIR-V assembler (from `spirv-tools`) for shader compilation

## 2. Build Script (`scripts/build_engine.py`)

### 2.1 Shader Assembly

SPIR-V assembly sources (`src/engine/shaders/*.vert`, `*.frag`) are assembled to `.spv` binary via `spirv-as`, then embedded as C byte arrays in a generated header `src/engine/shaders.h`.

Rebuild is triggered when:
- The `.vert`/`.frag` source is newer than the `.spv`
- The `.spv` is newer than `shaders.h`
- `shaders.h` doesn't exist

### 2.2 C Compilation

All C99 sources are compiled into a single shared library:

```bash
gcc -std=c99 -O3 -fPIC -shared \
    -I src/engine \
    $(pkg-config --cflags --libs sdl3) \
    src/engine/engine.c \
    src/engine/platform/platform_sdl3.c \
    src/engine/platform/platform_vulkan.c \
    src/engine/platform/platform_headless.c \
    src/engine/ui/node.c \
    src/engine/ui/control.c \
    src/engine/ui/draw_list.c \
    src/engine/ui/scene_tree.c \
    src/engine/ui/color_rect.c \
    src/engine/ui/vbox_container.c \
    -o src/sol/libsol.so \
    -lvulkan -lm
```

### 2.3 Incremental Build

The script checks file modification times. If `libsol.so` is newer than all source files, the build is skipped.

## 3. Source File Map

| Component | Files |
|-----------|-------|
| Engine core | `engine.h`, `engine.c` |
| Platform abstraction | `platform/platform.h` |
| SDL3 backend | `platform/platform_sdl3.c` |
| Vulkan renderer | `platform/platform_vulkan.h`, `platform/platform_vulkan.c` |
| Headless backend | `platform/platform_headless.c` |
| Shaders | `shaders/vertex.vert`, `shaders/fragment.frag` → `shaders.h` (generated) |
| Node system | `ui/node.h`, `ui/node.c` |
| Control | `ui/control.h`, `ui/control.c` |
| DrawList | `ui/draw_list.h`, `ui/draw_list.c` |
| SceneTree | `ui/scene_tree.h`, `ui/scene_tree.c` |
| ColorRect widget | `ui/color_rect.h`, `ui/color_rect.c` |
| VBoxContainer | `ui/vbox_container.h`, `ui/vbox_container.c` |
| Math types | `ui/types.h` |
| Public include | `ui.h` |

## 4. Dependencies

### Build-time

| Dependency | Version | Purpose |
|------------|---------|---------|
| GCC or Clang | any C99-capable | C compilation |
| SDL3 | ≥ 3.2 | Windowing, Vulkan surface |
| Vulkan SDK | ≥ 1.0 | Headers + loader |
| spirv-as | from spirv-tools | Shader assembly |
| Python | ≥ 3.10 | Build script + host runtime |

### Runtime

| Dependency | Notes |
|------------|-------|
| libSDL3.so | Dynamic link |
| libvulkan.so | Dynamic link |
| libm.so | Math library |

## 5. Platform-Specific Notes

### Linux

SDL3 headers expected at `/usr/local/include/SDL3/`. Library at `/usr/local/lib/`.  
Uses `pkg-config` to resolve flags.

### Windows

Set `VULKAN_SDK` environment variable for Vulkan headers/libs.  
SDL3 must be findable by the compiler (add to `INCLUDE` / `LIB`).

### Android (Buildozer)

The `buildozer.spec` targets API level 24+ for Vulkan support.  
`python-for-android` cross-compiles using NDK toolchain.

## 6. Developer Commands

```bash
# Full build
python3 scripts/build_engine.py

# Rebuild from clean
rm -f src/sol/libsol.so && python3 scripts/build_engine.py

# Run with local engine
PYTHONPATH=src python3 game/main.py

# Run UI example
PYTHONPATH=src python3 examples/ui_example.py

# Install as editable package (calls setup.py → build script)
uv pip install -e .
```
