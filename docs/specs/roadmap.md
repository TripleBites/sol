# Sol Engine — Development Roadmap

> Updated: 2026-08-01. Tracks actual progress against `docs/sol_spec.md`.

---

## Legend

| Symbol | Meaning |
|--------|---------|
| ✅ | Complete |
| 🔧 | In progress |
| 🔜 | Next up |
| ⬜ | Planned |
| 💡 | Future idea |

---

## Phase 1: Foundation — Build, Audio, Hello Sound

| Task | Status | Notes |
|------|--------|-------|
| Fix `scripts/build_engine.py` to match `src/sol/` paths | 🔜 | Current script references wrong paths |
| Fix `#include "io_vulkan.h"` → `"../photon/photon_vulkan.h"` | 🔜 | In `io_sdl3.c` |
| Extend `SolIO` with `audio_init/audio_shutdown/audio_lock/audio_unlock` | 🔜 | `src/sol/io/io.h` |
| Implement SDL3 audio backend (`SDL_AudioStream`) | 🔜 | `src/sol/io/io_sdl3.c` |
| Implement ALSA audio backend (`snd_pcm`) | 🔜 | `src/sol/io/io.c` (headless) |
| `AudioNode` base type (extends `Node`) | 🔜 | `src/sol/audio/audio_node.h/c` |
| `OscillatorNode` — sine, square, saw, triangle | 🔜 | `src/sol/audio/osc.h/c` |
| `MixerNode` — sums children, normalizes | 🔜 | `src/sol/audio/mixer.h/c` |
| `GainNode` — amplitude multiplier | 🔜 | `src/sol/audio/gain.h/c` |
| `AudioPipeline` — control queue, audio callback, probes | 🔜 | `src/sol/audio/pipeline.h/c` |
| Python ctypes bindings: `src/sol/python/audio_bindings.py` | 🔜 | |
| Neptune `main.py --headless` plays sine tone from QWERTY | 🔜 | v1 hello world |
| Remove prototype Neptune code from `src/neptune/` | 🔜 | Superseded by C audio nodes |
| Verify `build_engine.py` produces working `libsol.so` | 🔜 | |

---

## Phase 2: Voices, Envelopes, Polyphony

| Task | Status |
|------|--------|
| `EnvelopeNode` — ADSR state machine | ⬜ |
| `VoiceNode` — binds OscNode + EnvNode to one MIDI note | ⬜ |
| Polyphony: multiple VoiceNodes, voice stealing | ⬜ |
| `ProbeNode` — ring buffer tap at any pipeline point | ⬜ |
| MIDI input via `mido` + `python-rtmidi` | ⬜ |
| Neptune `--midi` flag | ⬜ |
| `AudioPipeline.send_note_on/off` → control queue | ⬜ |
| Test: play 8-note chord, no glitching | ⬜ |

---

## Phase 3: Debug & Text Infrastructure

| Task | Status |
|------|--------|
| `src/sol/debug/mem.h/c` — tracked `sol_malloc`/`sol_free` | ⬜ |
| `src/sol/debug/logger.h/c` — leveled `sol_log()` | ⬜ |
| `src/sol/debug/prof.h/c` — scoped `SOL_PROFILE()` | ⬜ |
| Migrate ALL existing C code to `sol_malloc`/`sol_log` | ⬜ |
| `src/sol/text/utf8.h/c` — UTF-8 iterator + validation | ✅ |
| `src/sol/text/font.h/c` — stb_truetype glyph cache | ⬜ |
| `src/sol/text/emoji.h/c` — emoji codepoint detection | ⬜ |
| `src/sol/text/layout.h/c` — text wrapping + alignment | ⬜ |
| Python bindings for text + debug | ⬜ |

---

## Phase 4: UI Widget Completion

| Task | Status |
|------|--------|
| `HBoxContainer` — horizontal layout | ✅ |
| `MarginContainer` — padding | ✅ |
| `CenterContainer` — centering | ✅ |
| `Label` — text widget | ✅ |
| `Button` — clickable with signals | ✅ |
| `StyleBox` — flat, rounded, bordered styles | ✅ |
| Signal system (`signal_connect`/`signal_emit`) | ✅ |
| `Variant` — discriminated union for signal args | ✅ |
| `PanelContainer` — StyleBox background + child | ✅ |
| `Render2D` — immediate-mode 2D batch renderer | ✅ |
| Input event routing (hit-test, bubble, focus) | 🔧 |
| Theme system | ⬜ |

---

## Phase 5: Neptune GUI + Polish

| Task | Status |
|------|--------|
| Neptune `gui/synth_panel.py` — Sol Control tree for synth UI | 🔜 |
| Waveform viewer widget (DrawList lines from probe data) | 🔜 |
| Knob widget (mouse-drag, maps to `send_set_param`) | 🔜 |
| Pipeline node inspector (boxes, labels, probe taps) | 🔜 |
| `patch.py` — JSON save/load of AudioNode tree | 🔜 |
| `@register_audio_node` decorator for extensibility | 🔜 |

---

## Phase 6: Extensibility & Templates

| Task | Status |
|------|--------|
| `examples/extending/my_first_voice.py` — <40 line starter | ⬜ |
| `examples/extending/my_first_effect.py` — <40 line starter | ⬜ |
| `readme.md` update with quickstart + links | ⬜ |
| Raspberry Pi validation (ALSA headless, MIDI keyboard) | ⬜ |
| Latency profiling and optimization | ⬜ |

---

## Phase 7: 2D World (Future)

| Task | Status |
|------|--------|
| `src/sol/spatial2d/` — Sprite2D, Camera2D | 💡 |
| Box2D physics integration | 💡 |
| Tilemap rendering | 💡 |
| Particle system | 💡 |

---

## Phase 8: 3D Rendering (Future)

| Task | Status |
|------|--------|
| `src/sol/spatial3d/` — Spatial3D, MeshInstance3D, Camera3D, Light3D | 💡 |
| 3D pipeline in `photon/` (depth buffer, mesh rendering) | 💡 |
| glTF loader (cgltf) | 💡 |
| PBR material pipeline | 💡 |

---

## Key Design Decisions Log

| Date | Decision | Rationale |
|------|----------|-----------|
| 2026-08-01 | Vulkan over OpenGL | Already working prototype, lower overhead |
| 2026-08-01 | Engine name: **Sol** | Established naming |
| 2026-08-01 | Retained-mode UI (keep `scene/`) | Working well, Godot-parity |
| 2026-08-01 | `AudioNode` / `AudioPipeline` naming | Cleaner than `SynNode`/`SynPipeline` |
| 2026-08-01 | Audio I/O via SDL3 + ALSA (native in `io/`) | No `sounddevice` dependency. SDL3 audio is built-in. ALSA for Pi headless. |
| 2026-08-01 | `VoiceNode` polyphony model | One AudioNode per active note. Simple, debuggable, Godot-like. |
| 2026-08-01 | `AudioNode.process_audio` as direct function pointer | Not in vtable — zero dispatch overhead in audio hot path |
| 2026-08-01 | 3D-ready architecture: Node is pure tree container | `Node` has no spatial state. `Spatial3D` extends it when 3D is implemented. No breaking changes needed. |
| 2026-08-01 | Terra editor = placeholder | Shared synth types at engine level. GUI logic stays in Neptune. |

---

*Roadmap maintained alongside `docs/sol_spec.md`.*
