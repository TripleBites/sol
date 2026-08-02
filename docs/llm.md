# Sol Engine — LLM Agent Instructions

> **For AI coding agents (Claude Code, Codex, Pi, Cursor, etc.).**  
> Read this before making changes. It tells you how the project is structured,  
> what the conventions are, and how to keep docs in sync with code.

---

## How to read this project

1. **Start here:** [`docs/llms.md`](llms.md) — the curated index of all documentation
2. **Master spec:** [`docs/specs/sol_spec.md`](specs/sol_spec.md) — architecture, design philosophy, node model
3. **Subsystem specs:** read the relevant one for your task:
   - [`docs/specs/scene_spec.md`](specs/scene_spec.md) — UI system (Control, layout, DrawList, SceneTree, signals)
   - [`docs/specs/neptune_spec.md`](specs/neptune_spec.md) — Python synth composition layer
   - [`docs/specs/build_spec.md`](specs/build_spec.md) — build system, dependencies, targets
4. **Progress tracking:** [`docs/specs/roadmap.md`](specs/roadmap.md) — what's done (✅), in progress (🔧), planned (⬜)
5. **File index:** [`docs/wiki/file_index.md`](wiki/file_index.md) — every source file with a one-line summary

## Operations

### Before implementing anything
1. Read the relevant spec section
2. Read the actual `.c` and `.h` files for that subsystem
3. Check the roadmap to see what's already done
4. If code and spec disagree, **trust the code** and flag the stale spec

### When you change code
- **If API changes:** update the corresponding spec
- **If you add a new node type:** add it to the taxonomy in `sol_spec.md` §3.2
- **If you add a new source file:** update `docs/wiki/file_index.md`
- **When you finish a roadmap task:** update the status marker (✅ 🔧 ⬜)

### When you update a spec
- Update the "Last updated" date at the top
- Add a brief entry to the changelog section (if the spec has one)

### Periodically
- Run `docs/wiki/file_index.md` regeneration (read every `.c`/`.h`, update summaries)
- Ask: "read the specs, read the code, flag contradictions or stale claims"

## Conventions — the rules you must follow

### Language
- **C99 only.** `gcc --std=c99 -pedantic`. No C++, no C11/C17 features.
- **Python 3.10+** for bindings, Neptune, and tooling.

### Architecture
- **Node is a pure tree container.** Never add spatial, audio, or rendering state to `struct Node`.
- **Flat vtables, not deep hierarchies.** Struct embedding + vtable composition. No C++ inheritance.
- **AudioNode uses direct function pointers** (not vtable dispatch) for the audio hot path.
- **Control** extends Node for UI. **AudioNode** extends Node for audio. They are separate sub-trees.
- A single Node cannot be both Control and AudioNode.

### Bindings
- **Hand-written ctypes only.** Never generate bindings. No Cython, no SWIG, no pybind11.
- Bindings live in `src/sol/python/`.

### Memory
- All C heap allocations use `sol_malloc`/`sol_free` macros (from `debug/mem.h`).
- Refcounting for nodes: parent owns children (strong ref), child→parent is weak.
- In audio, the main thread sends control messages; the audio thread never allocates, locks, or does I/O.

### Naming
- C types: `PascalCase` (`Node`, `Control`, `AudioPipeline`)
- C functions: `snake_case` with descriptive prefix (`node_add_child`, `audio_pipeline_start`)
- Macros/constants: `UPPER_SNAKE_CASE` (`SIZE_EXPAND`, `SOL_LOG_INFO`)
- Python: standard PEP 8 (`AudioPipeline`, `set_root`, `note_on`)

### Anti-goals — do NOT suggest or implement
- No C++ code
- No visual editor / GUI builder
- No generated bindings
- No Variant union type for general use (structs, always structs)
- No 3D code yet (architecture supports it, implementation doesn't)
- No plugin auto-discovery (explicit imports + decorators only)
- No custom DSL or visual patcher for audio

## Key files you'll need

| File | Why |
|------|-----|
| `src/sol/scene/node.h` | Base Node type, vtable, refcounting |
| `src/sol/scene/control.h` | Rect, anchors, size flags, layout |
| `src/sol/audio/audio_node.h` | AudioNode base, process_audio fn pointer |
| `src/sol/audio/pipeline.h` | AudioPipeline, control queue, probes |
| `src/sol/photon/photon_vulkan.h` | Vulkan swapchain, pipelines, UI rendering |
| `src/sol/io/io.h` | Platform abstraction (SDL3, headless) |
| `src/sol/python/bindings.py` | Engine ctypes bindings |
| `src/sol/python/ui_bindings.py` | UI system ctypes bindings |
| `src/sol/python/audio_bindings.py` | Audio system ctypes bindings |
| `scripts/build.py` | C compilation + shader assembly |

## Build commands

```bash
python3 scripts/build.py                          # build libsol.so
PYTHONPATH=src python3 examples/ui_example.py     # run UI demo
PYTHONPATH=src python3 neptune/main.py            # run synth
```

---

*Tell your LLM agent to read this file at the start of every session.*
