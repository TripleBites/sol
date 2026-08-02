# Sol Engine — File Index

> Auto-generated map of every source file. Regenerate by having an LLM agent re-read the codebase.  
> **Last updated:** 2026-08-01 (post-lint pass)

---

## Core engine — `src/sol/`

| File | Summary |
|------|---------|
| `core.h` | Engine lifecycle: `sol_init`, `sol_update`, `sol_shutdown`, `sol_get_size`, event push |
| `core.c` | Main loop implementation — platform init, input polling, frame orchestration |
| `scene.h` | Convenience include for the entire scene subsystem |
| `__init__.py` | Package loader — imports all submodules from `python/`, re-exports public API |

## Platform I/O — `src/sol/io/`

| File | Summary |
|------|---------|
| `io.h` | `SolIO` platform vtable — window, audio, input abstraction; backend constructors (`sol_io_sdl3`, `sol_io_headless`) |
| `io_sdl3.c` | SDL3 backend — window creation, Vulkan surface, SDL audio stream, event loop |
| `io_headless.c` | Headless backend — ALSA audio via `snd_pcm`, no window, for Raspberry Pi / CI |
| `input_state.h` | Godot-style `InputState` — 256-key bitmap, mouse/touch state, one-shot `SolEvent` queue, USB HID keycodes, query API |
| `input_state.c` | InputState implementation — frame lifecycle (begin_frame), key/mouse/touch injection, event queue |

## Vulkan renderer — `src/sol/photon/`

| File | Summary |
|------|---------|
| `photon.h` | Renderer-agnostic header — currently just includes Vulkan backend |
| `photon_vulkan.h` | `SolVulkan` struct — swapchain, two pipelines (main + UI), framebuffers, sync objects, resize handling |
| `photon_vulkan.c` | Vulkan implementation — init, frame loop (acquire→draw→submit→present), UI pipeline, 2D batch integration |
| `render2d.h` | `Render2D` — immediate-mode 2D batch renderer: rects, lines, circles with z-sorting, NDC transform, 64K vertex buffer |
| `render2d.c` | Render2D implementation — item accumulation, sort, vertex packing, flush to Vulkan |

## Scene / UI system — `src/sol/scene/`

| File | Summary |
|------|---------|
| `types.h` | Math types: `Vec2`, `Rect`, `Color` with convenience macros |
| `node.h` | Base `Node` type — `NodeClass` vtable (init/destroy/enter_tree/exit_tree/ready/process/draw/get_minimum_size/arrange_children/handle_input), tree hierarchy, refcounting, preorder/postorder traversal |
| `node.c` | Node implementation — alloc, ref/unref, add/remove child, name, type check, base init/destroy |
| `control.h` | `Control` extends Node — rect, `Anchor` struct, offset, size flags (FILL/EXPAND/SHRINK), mouse filter, focus mode, container flag |
| `control.c` | Control implementation — anchor+offset→rect computation, min size, global rect |
| `draw_list.h` | `DrawList` — renderer-agnostic command buffer: RECT_FILLED, RECT_BORDER, TEXT, TEXTURE, CLIP_PUSH/POP, clip stack |
| `draw_list.c` | DrawList implementation — append commands, clip stack management, iteration |
| `scene_tree.h` | `SceneTree` — frame orchestrator (process→layout→draw), input routing, hit testing, deferred queue, debug print |
| `scene_tree.c` | SceneTree implementation — two-pass layout solver, draw pass, input dispatch, deferred call flush |
| `color_rect.h` | `ColorRect` widget — filled colored rectangle, extends Control |
| `color_rect.c` | ColorRect implementation — draw vtable emits filled rect command |
| `vbox_container.h` | `VBoxContainer` — vertical layout container with separation, extends Control |
| `vbox_container.c` | VBoxContainer implementation — measure pass (child min sizes), arrange pass (distribute space) |
| `hbox_container.h` | `HBoxContainer` — horizontal layout container with separation |
| `hbox_container.c` | HBoxContainer implementation — measure + arrange horizontally |
| `margin_container.h` | `MarginContainer` — padding around single child, per-side margins |
| `margin_container.c` | MarginContainer implementation — inset child by margins |
| `center_container.h` | `CenterContainer` — centers single child within own rect |
| `center_container.c` | CenterContainer implementation — measure child, center in arrange |
| `panel_container.h` | `PanelContainer` — draws StyleBox background, arranges single child inside |
| `panel_container.c` | PanelContainer implementation — draw StyleBox, inset child |
| `label.h` | `Label` widget — text, font_size, alignment (L/C/R + T/M/B), autowrap, clip, font color |
| `label.c` | Label implementation — measure text size, draw text commands |
| `button.h` | `Button` widget — hover/press/toggle states, corner radius, three color states, signals (pressed/toggled/button_down/button_up) |
| `button.c` | Button implementation — mouse filter, state transitions, signal emission |
| `signal.h` | Signal system — `Signal` type, `SignalCallback`, CONNECT_NORMAL/ONE_SHOT/DEFERRED, DeferredQueue for deferred calls |
| `signal.c` | Signal implementation — connection array, emit, deferred queue push/flush |
| `style_box.h` | `StyleBox` — flat/rounded/bordered background drawing with margins, inner content rect calculation |
| `style_box.c` | StyleBox implementation — draw rect/border commands, margin-inset rect |
| `variant.h` | `Variant` — discriminated union (nil/bool/int/float/string/Vec2/Rect/Color) with inline constructor functions |
| `input_event.h` | `UIInputEvent` — mouse motion/button/scroll, keyboard, focus enter/exit events |

## Audio system — `src/sol/audio/`

| File | Summary |
|------|---------|
| `audio_node.h` | `AudioNode` base — extends Node, `process_audio` fn pointer (not vtable — zero dispatch overhead), param interface, node ID, probe ring buffer |
| `audio_node.c` | AudioNode implementation — alloc/init/free, post-order tree processing, ID management, child add/remove |
| `pipeline.h` | `AudioPipeline` — SPSC control queue, node table (ID→ptr), probe slots (SW/SR ring buffers), audio callback, platform binding |
| `pipeline.c` | AudioPipeline implementation — new/free, set_root, start/stop, control message drain (note_on/off, set_param), probe add/read, audio callback entry point |
| `osc.h` | `OscillatorNode` — sine/square/saw/triangle/noise waveforms, freq/amp/phase, waveform name↔enum lookup |
| `osc.c` | OscillatorNode implementation — per-sample waveform synthesis, phase accumulation, bandlimited-ish saw/square |
| `voice.h` | `VoiceNode` — one active MIDI note, owns envelope (child[0]) + oscillator (child[1]), midi_to_freq helper |
| `voice.c` | VoiceNode implementation — note_on/note_off sets children's params via control queue |
| `envelope.h` | `EnvelopeNode` — ADSR state machine (IDLE→ATTACK→DECAY→SUSTAIN→RELEASE), trigger/release |
| `envelope.c` | EnvelopeNode implementation — per-sample level computation, stage transitions |
| `mixer.h` | `MixerNode` — sums children (accumulation via post-order traversal, no additional state) |
| `mixer.c` | MixerNode implementation — minimal, process_audio is no-op (children accumulate) |
| `gain.h` | `GainNode` — multiplies buffer by level scalar after children |
| `gain.c` | GainNode implementation — level setter, buffer multiplication |

## Debug system — `src/sol/debug/`

| File | Summary |
|------|---------|
| `mem.h` | Tracked `sol_malloc`/`sol_free` with file/line/tag, allocation records, leak dump, compile-time disable (`SOL_MEM_TRACKING=0`) |
| `mem.c` | Memory tracker — hash table lookup by pointer, allocation counting, leak reporting by tag |
| `logger.h` | Leveled `sol_log()` (TRACE→FATAL) with file:line:tag, 256-message ring buffer, Python callback hook, quiet mode |
| `logger.c` | Logger implementation — printf with ANSI color, ring buffer, thread-local state |

## Text system — `src/sol/text/`

| File | Summary |
|------|---------|
| `utf8.h` | `Utf8Iter` — UTF-8 codepoint iteration, validation, encode/decode, codepoint count, continuation detection, basic emoji detection |
| `utf8.c` | UTF-8 implementation — byte→codepoint decoding, codepoint→byte encoding, codepoint counting |

## Python bindings — `src/sol/python/`

| File | Summary |
|------|---------|
| `__init__.py` | Package marker |
| `bindings.py` | Engine ctypes bindings — `sol_init/update/shutdown/get_size`, cross-platform library finder |
| `sol.py` | High-level re-exports — `init`, `update`, `shutdown`, `get_size` |
| `ui_bindings.py` | UI ctypes bindings — Node, Control, ColorRect, VBoxContainer, HBoxContainer, MarginContainer, CenterContainer, Button, Label (?), SceneTree, DrawList/DrawCmd, Signal, UIInputEvent, Variant, vtable lookup, signal trampoline |
| `audio_bindings.py` | Audio ctypes bindings — AudioNode, OscillatorNode (with Sine/Square/Saw/Triangle/Noise classmethods), MixerNode, GainNode, EnvelopeNode (with ADSR classmethod), VoiceNode, AudioPipeline (with note_on/off, probe access) |
| `input_bindings.py` | Input ctypes bindings — USB HID keycode constants, keyboard/mouse/touch query, SolEvent polling, MIDI event push |

## Neptune synth — `neptune/`

| File | Summary |
|------|---------|
| `main.py` | Entry point — builds AudioPipeline with VoiceNode polyphony pool, QWERTY keyboard→MIDI via KeyboardInput, ALSA audio output |
| `input/keyboard.py` | `KeyboardInput` — maps Z-X-C-V-B-N-M-, and S-D-G-H-J keys to MIDI notes (piano layout), polls terminal via getch |

## Build & config — root

| File | Summary |
|------|---------|
| `scripts/build.py` | Build script — SPIR-V shader assembly via `spirv-as`, C99 compilation of all `src/sol/` .c files → `libsol.so` |
| `pyproject.toml` | Python project metadata and dependencies |
| `setup.py` | Package setup (legacy setuptools) |
| `buildozer.spec` | Android packaging config (Buildozer) |

## Documentation — `docs/`

| File | Summary |
|------|---------|
| `llm.md` | LLM agent instructions — conventions, operations, anti-goals, key files, build commands |
| `llms.md` | Curated index of all documentation (LLM-friendly entry point, follows llms.txt spec) |
| `SESSION_STARTER.md` | Copy-paste prompt for starting any LLM coding session |
| `specs/sol_spec.md` | Master architecture spec — design philosophy, node model, all subsystems, node taxonomy |
| `specs/scene_spec.md` | UI system detailed spec — vtable design, two-pass layout, DrawList, signals, theming |
| `specs/neptune_spec.md` | Neptune synth layer spec — Python composition over C audio, GUI panels, patches |
| `specs/build_spec.md` | Build system spec — compilation, dependencies, platform targets |
| `specs/roadmap.md` | Development phases with status markers (✅ 🔧 🔜 ⬜ 💡) |
| `wiki/file_index.md` | This file — every source file with a one-line summary |

---

*To regenerate: ask an LLM agent to read every source file and update the summaries above. Or run `find src -name '*.c' -o -name '*.h' -o -name '*.py' | sort` as a starting point.*
