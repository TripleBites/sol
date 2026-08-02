# Sol Engine

A cross-platform game and audio engine written in **C99** with **SDL3 + Vulkan** rendering, wrapped for **Python** via `ctypes`.

## Architecture

```
libsol.so (C99 engine)          Python (ctypes wrappers)
┌─────────────────────────┐     ┌──────────────────────┐
│ scene/  — 2D UI system  │     │ sol.bindings         │
│ audio/  — synth nodes   │ ◄── │ sol.ui_bindings      │
│ text/   — font + emoji  │     │ sol.audio_bindings   │
│ debug/  — mem + logging │     │ sol.text_bindings    │
│ photon/ — Vulkan        │     └──────────────────────┘
│ io/    — SDL3, ALSA     │
└─────────────────────────┘
     ▲
     │
┌────┴─────────────────────────────────────────────┐
│  Neptune — Python synth composer layer           │
│  (composes AudioNodes, drives GUI via Controls)  │
└──────────────────────────────────────────────────┘
```

**Key concepts:**
- **Node** → base type with manual vtables, refcounting, tree hierarchy
- **Control** → rect, anchors, size flags, two-pass layout
- **AudioNode** → audio processing nodes (Oscillator, Voice, Mixer, Gain, Envelope)
- **AudioPipeline** → real-time audio execution (control queue, probes, audio callback)
- **DrawList** → renderer-agnostic command buffer
- **SceneTree** → orchestrates process → layout → draw each frame

## Quick Start

### Prerequisites

- **SDL3** (system install)
- **Vulkan SDK** (system install)
- **spirv-as** (from `spirv-tools`) for shader assembly
- **Python 3.10+**

### Build & Run

```bash
# Build the C engine
python3 scripts/build_engine.py

# Run UI example
PYTHONPATH=src python3 examples/ui_example.py

# Run game demo
PYTHONPATH=src python3 game/main.py

# Run Neptune synth (headless)
PYTHONPATH=src python3 neptune/main.py --headless
```

## Project Layout

```
sol/
├── game/main.py
├── examples/ui_example.py
├── src/
│   ├── sol/                   ← C99 engine
│   │   ├── core.h/c           ← init/update/shutdown
│   │   ├── io/                ← platform backends (SDL3, ALSA, headless)
│   │   ├── photon/            ← Vulkan renderer
│   │   ├── scene/             ← 2D UI (Node, Control, Containers, DrawList)
│   │   ├── audio/             ← audio synthesis nodes (PHASE 1)
│   │   ├── text/              ← unicode text + emoji (PHASE 3)
│   │   ├── debug/             ← memory tracking + logging (PHASE 3)
│   │   └── shaders/           ← SPIR-V assembly
│   └── sol/                   ← Python package (ctypes bindings)
├── neptune/                   ← Python synth layer
├── scripts/build_engine.py
├── pyproject.toml
└── docs/                      ← specifications + roadmap
```

## Documentation

| Document | Description |
|----------|-------------|
| [`docs/sol_spec.md`](docs/sol_spec.md) | **Master architecture spec** — read this first |
| [`docs/scene_spec.md`](docs/scene_spec.md) | UI system detailed spec |
| [`docs/neptune_spec.md`](docs/neptune_spec.md) | Neptune synth layer spec |
| [`docs/build_spec.md`](docs/build_spec.md) | Build system spec |
| [`docs/roadmap.md`](docs/roadmap.md) | Development phases and progress |

## Design Principles

- **C99 hot path, Python logic path** — ctypes, zero overhead
- **Flat vtables, not deep hierarchies** — struct embedding, no C++ inheritance
- **Node is a pure tree container** — no spatial/audio state in the base
- **2D-first, 3D-ready** — Spatial3D is designed but not yet implemented
- **Python is the editor** — no custom IDE, `uv run` is the launch command
- **No generated bindings** — hand-written ctypes wrappers, readable and debuggable
