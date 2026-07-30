# Sol Engine

A cross-platform game engine written in **C99** with **SDL3 + Vulkan** rendering,
wrapped for **Python** via `ctypes`.

## Architecture

```
game/main.py          ← your game code
    │
    ▼
src/sol/              ← Python package (ctypes bindings)
    ├── bindings.py   ← loads libsol.so, exposes sol.init/update/shutdown
    ├── ui_bindings.py ← UI system wrapper (Godot-inspired retained-mode)
    └── sol.py        ← public API re-exports
    │
    ▼
libsol.so             ← compiled C99 shared library
    │
    ├── engine.c               ← core dispatch (init → platform)
    ├── platform/
    │   ├── platform_sdl3.c    ← SDL3 window + Vulkan bootstrap
    │   ├── platform_vulkan.c  ← pure Vulkan renderer (swapchain, pipeline, draw)
    │   └── platform_headless.c← no-op backend
    └── ui/                    ← Godot-inspired retained-mode UI
        ├── node.c             ← base object (vtable, refcount, tree ops)
        ├── control.c          ← rect, anchors, layout
        ├── draw_list.c        ← renderer-agnostic draw commands
        ├── scene_tree.c       ← frame orchestrator (process→layout→draw)
        ├── color_rect.c       ← colored rectangle widget
        └── vbox_container.c   ← vertical box layout container
```

## Quick Start

### Prerequisites

- **SDL3** (system install)
- **Vulkan SDK** (system install)
- **spirv-as** (from `spirv-tools`) for shader assembly
- **Python 3.10+**

### Build & Run

```bash
# Build the C engine (shared library)
python3 scripts/build_engine.py

# Run the hello-triangle demo
PYTHONPATH=src python3 game/main.py

# Run the UI system example
PYTHONPATH=src python3 examples/ui_example.py
```

### Development Workflow

```bash
# One-time setup
uv venv
source .venv/bin/activate
uv pip install -e .

# After C changes — rebuild
python3 scripts/build_engine.py

# After Python changes — just re-run
PYTHONPATH=src python3 game/main.py
```

## Platform Targets

| Platform | Windowing | Renderer | Packaging |
|----------|-----------|----------|-----------|
| **Linux Desktop** | SDL3 | Vulkan | `uv` + Nuitka |
| **Windows Desktop** | SDL3 | Vulkan | `uv` + Nuitka |
| **Android** | SDL3 | Vulkan | Buildozer (`buildozer.spec`) |
| **Headless** | — | — | CI/testing |

## UI System

The engine includes a Godot-inspired retained-mode UI system at `src/engine/ui/`.
It produces renderer-agnostic draw commands consumed by the Vulkan backend.

Key concepts:
- **Node** → base type with manual vtables, refcounting, tree hierarchy
- **Control** → rect, anchors, size flags, layouts
- **Container** → VBoxContainer with SIZE_FILL, SIZE_EXPAND, separation
- **DrawList** → command buffer (filled rects, borders, text, clip rects)
- **SceneTree** → orchestrates process → two-pass layout → draw each frame

See `examples/ui_example.py` for a complete Python example.

## Project Layout

```
sol/
├── game/main.py               ← application entry point
├── examples/ui_example.py     ← UI system demo
├── src/
│   ├── engine/                ← C99 engine
│   │   ├── engine.h/c         ← public API
│   │   ├── platform/          ← platform backends
│   │   ├── ui/                ← UI system
│   │   └── shaders/           ← SPIR-V assembly sources
│   └── sol/                   ← Python package
│       ├── __init__.py
│       ├── bindings.py        ← ctypes engine bindings
│       ├── ui_bindings.py     ← ctypes UI bindings
│       └── sol.py
├── scripts/build_engine.py    ← build script (invoked by setup.py)
├── setup.py                   ← setuptools integration
├── pyproject.toml             ← project metadata
├── buildozer.spec             ← Android packaging
└── docs/                      ← specifications
```
