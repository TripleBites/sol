# Sol Engine

> C99 game & audio engine with SDL3 + Vulkan rendering, flat-vtable node architecture, and hand-written Python ctypes bindings. Godot-inspired scene tree, retained-mode 2D UI, real-time audio synthesis pipeline, and a QWERTY-playable synth (Neptune). 2D-first, 3D-ready. Python is the editor.

## Before anything else

- [LLM agent instructions](llm.md): Conventions, operations, anti-goals — **read this first if you're an AI agent**

## Architecture & specs

- [Master architecture spec](specs/sol_spec.md): Design philosophy, node model, all subsystems, node taxonomy, file formats, project layout
- [UI system spec](specs/scene_spec.md): Node/Control vtable, two-pass layout solver, DrawList, SceneTree frame loop, input routing, signals, theming
- [Neptune synth spec](specs/neptune_spec.md): Python composition layer over the C audio engine — AudioNode tree building, QWERTY/MIDI input, GUI panels, patch JSON serialization
- [Build system spec](specs/build_spec.md): Compilation targets (Linux, Pi, Windows, Android), dependencies, shader assembly, developer commands
- [Roadmap](specs/roadmap.md): Phase tracking with status markers (✅ done, 🔧 in progress, 🔜 next, ⬜ planned, 💡 future)

## Source code maps

- [File index](wiki/file_index.md): Every `.c`, `.h`, and `.py` file with a one-line summary, organized by subsystem

## Key source files (quick access)

- [Node system header](src/sol/scene/node.h): Base `Node` type, `NodeClass` vtable, refcounting, tree operations
- [Control header](src/sol/scene/control.h): `Control` extends Node — rect, anchors, size flags, mouse filter
- [DrawList header](src/sol/scene/draw_list.h): Renderer-agnostic draw command buffer
- [SceneTree header](src/sol/scene/scene_tree.h): Frame orchestrator — process → layout → draw
- [AudioNode header](src/sol/audio/audio_node.h): Audio processing node, `process_audio` fn pointer
- [AudioPipeline header](src/sol/audio/pipeline.h): Real-time audio execution, control queue, probes
- [Vulkan renderer](src/sol/photon/photon_vulkan.h): Swapchain, pipelines, UI draw consumption
- [Platform abstraction](src/sol/io/io.h): `SolIO` vtable — SDL3 and headless/ALSA backends
- [Input state](src/sol/io/input_state.h): Godot-style input system — key bitmap, mouse, touch, events
- [Memory tracking](src/sol/debug/mem.h): Tagged `sol_malloc`/`sol_free` with leak detection
- [Logger](src/sol/debug/logger.h): Leveled logging, ring buffer, Python callback hook
- [UTF-8 iterator](src/sol/text/utf8.h): Codepoint iteration, validation, emoji detection

## Entry points

- [Engine core](src/sol/core.h): `sol_init`, `sol_update`, `sol_shutdown`
- [Python engine bindings](src/sol/python/bindings.py): ctypes wrappers for core engine API
- [Python UI bindings](src/sol/python/ui_bindings.py): Node, Control, ColorRect, VBoxContainer, Button, SceneTree
- [Python audio bindings](src/sol/python/audio_bindings.py): OscillatorNode, VoiceNode, EnvelopeNode, AudioPipeline
- [Neptune main](neptune/main.py): QWERTY-playable synthesizer entry point

## Optional

- [Karpathy LLM Wiki pattern](https://gist.github.com/karpathy/442a6bf555914893e9891c11519de94f): The inspiration for this documentation structure
- [llms.txt spec](https://llmstxt.org/): The standard this file is based on
