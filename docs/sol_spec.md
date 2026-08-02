# Sol Engine Specification v1.0
## Python + C99 Game & Audio Engine Architecture

> **Design Philosophy:** Godot's scene tree and composition model, without Godot's binding-layer tax. C99 for the hot path, Python for the logic path, `ctypes` for zero-overhead interop. Flat vtables, not C++ inheritance. 2D-first with 3D-ready architecture.

---

## 1. Design Principles

1. **Node is a pure tree container.** Base `Node` carries no spatial, audio, or rendering state — only tree hierarchy, name, flags, and refcounting. Domain-specific state lives in extensions (`Control`, `AudioNode`, future `Spatial2D`/`Spatial3D`).
2. **No Binding Layer.** Python calls C directly through `ctypes.CDLL`. No `MethodBind`, no `ClassDB`, no generated glue code.
3. **Flat Vtables, Not Deep Hierarchies.** Every entity is a `Node` with a `NodeVTable`. Inheritance is struct embedding + vtable composition, never `class Sprite : public Node2D : public CanvasItem`.
4. **Stack-First API.** Everything that can live on the stack, does. No heap allocations for transient data.
5. **Python is the Editor.** No custom IDE. Projects are folders with `.py` scripts. `uv run` is the launch command.
6. **3D-Ready Design.** The architecture explicitly accommodates future `Spatial3D`, `Camera3D`, `MeshInstance3D`, and a 3D render pipeline — without any current 3D code. The extension points are designed, not hacked.

---

## 2. Architecture Overview

```
┌─────────────────────────────────────────────────────────────┐
│                   Python Application                       │
│  (game scripts, scene composition, Neptune synth layer)    │
├─────────────────────────────────────────────────────────────┤
│              ctypes (zero overhead)                         │
├─────────────────────────────────────────────────────────────┤
│                   libsol.so (C99)                           │
│                                                             │
│  ┌───────────┐ ┌───────────┐ ┌───────────┐ ┌───────────┐  │
│  │  scene/   │ │  audio/   │ │  text/    │ │  debug/   │  │
│  │  (2D UI)  │ │  (synth)  │ │  (font,   │ │  (mem,    │  │
│  │  Node     │ │  AudioNode│ │   emoji)  │ │   log)    │  │
│  │  Control  │ │  OscNode  │ │           │ │           │  │
│  │  DrawList │ │  VoiceNode│ │           │ │           │  │
│  └───────────┘ └───────────┘ └───────────┘ └───────────┘  │
│  ┌───────────┐ ┌───────────┐                               │
│  │  photon/  │ │  io/      │  ┌────────────┐ ┌─────────┐  │
│  │  (Vulkan) │ │  SDL3     │  │ spatial3d/ │ │spatial2d│  │
│  │           │ │  ALSA     │  │ [future]   │ │[future] │  │
│  └───────────┘ └───────────┘  └────────────┘ └─────────┘  │
└─────────────────────────────────────────────────────────────┘
```

---

## 3. The Node Extensions Model

### 3.1 Base Node (Pure Tree Container)

```c
typedef struct Node Node;
typedef struct NodeVTable NodeVTable;

struct NodeVTable {
    const char* type_name;
    uint32_t    type_id;
    size_t      instance_size;

    /* Lifecycle — all optional (NULL = no-op) */
    void (*init)(Node* self);
    void (*destroy)(Node* self);
    void (*enter_tree)(Node* self);
    void (*exit_tree)(Node* self);
    void (*ready)(Node* self);

    /* Per-frame — main thread */
    void (*process)(Node* self, float dt);

    /* Rendering — main thread */
    void (*draw)(Node* self, struct DrawList* dl);

    /* Layout — for Control and Containers */
    void (*get_minimum_size)(Node* self, Vec2* out);
    void (*arrange_children)(Node* self);

    /* Input */
    int (*handle_input)(Node* self, const struct InputEvent* ev);
};

struct Node {
    const NodeVTable* vtable;

    Node*       parent;          /* weak */
    Node**      children;
    uint32_t    child_count;
    uint32_t    child_capacity;

    char*       name;
    uint32_t    flags;           /* VISIBLE, PROCESS, IN_TREE, READY */
    void*       user_data;       /* Python back-pointer */

    int         refcount;
};
```

**Design rule:** `Node` never grows spatial or domain-specific fields. New domains add new structs that embed `Node` as their first member.

### 3.2 Node Extensions Map

```
Node                              [tree, name, flags, refcount]
├── Control                       [rect, anchors, offsets, size_flags, theme]
│   ├── ColorRect                 [color]
│   ├── VBoxContainer             [separation, layout algorithm]
│   ├── HBoxContainer             (planned)
│   ├── Label                     (planned, depends on text/)
│   ├── Button                    (planned)
│   └── (...more widgets...)
│
├── AudioNode                     [AudioPipeline context, probe ring buffer]
│   ├── OscillatorNode            [waveform, freq, amp, phase]
│   ├── VoiceNode                 [MIDI note, velocity, references Osc+Env]
│   ├── EnvelopeNode             [ADSR state machine]
│   ├── LFOFilterNode             (planned)
│   ├── MixerNode                 [sums children, normalizes]
│   ├── GainNode                  [multiplies amplitude]
│   └── ProbeNode                 [ring buffer tap for GUI waveform display]
│
├── Spatial2D        [future]     [Vec2 position, float rotation, Vec2 scale]
│   ├── Sprite2D                  [texture, region, modulate, z_index]
│   ├── CollisionBody2D           [body_type, velocity, collision_mask]
│   └── Camera2D                  [view_matrix, zoom]
│
└── Spatial3D        [future]     [Vec3 position, Quat rotation, Vec3 scale]
    ├── MeshInstance3D            [mesh, material]
    ├── CollisionBody3D           [body_type, velocity]
    ├── Camera3D                  [view_matrix, projection_matrix]
    └── Light3D                   [type, color, intensity, range]
```

### 3.3 Domain-Specific Execution Contexts

Different node types run on different threads, driven by different orchestrators:

| Domain | Orchestrator | Thread | Frequency |
|--------|-------------|--------|-----------|
| `Control` (UI) | `SceneTree` | Main thread | ~60Hz (`process` → layout → `draw`) |
| `AudioNode` (audio) | `AudioPipeline` | Audio callback | ~86Hz (512 samples @ 44.1kHz) |
| `Spatial2D` (2D world) | `SceneTree` (future) | Main thread | ~60Hz (`process` → `draw`) |
| `Spatial3D` (3D world) | `SceneTree` (future) | Main thread | ~60Hz (`process` → `draw`) |

`SceneTree` already walks the full node tree for `process` and `draw`. `AudioPipeline` maintains its own root and walks it from the audio callback. A single `Node` cannot be both `Control` and `AudioNode` — they are separate sub-trees.

---

## 4. Scene System (2D UI) — `src/sol/scene/`

### 4.1 Control & Layout

Already implemented per `docs/scene_spec.md`. `Control` extends `Node` with:

- `Rect rect` — local position relative to parent
- `Anchor anchor` — 0.0–1.0 relative to parent rect
- `Offset offset` — pixel offsets added post-anchor
- `Vec2 min_size` — minimum dimensions
- `uint32_t size_flags_h, size_flags_v` — `SIZE_FILL`, `SIZE_EXPAND`, `SIZE_SHRINK_*`
- Two-pass layout solver: measure (bottom-up) → arrange (top-down)

### 4.2 DrawList

Renderer-agnostic command buffer. Commands: `RECT_FILLED`, `RECT_BORDER`, `TEXT`, `TEXTURE`, `CLIP_PUSH`, `CLIP_POP`. Consumed by the Vulkan backend in `photon/`.

Future 3D commands will be added as new enum values (`DRAW_CMD_MESH`, `DRAW_CMD_SET_TRANSFORM`, `DRAW_CMD_SET_MATERIAL`) — no breaking change.

### 4.3 SceneTree Frame Loop

```
scene_tree_process(delta):
    process_pass(root, delta)        # pre-order
    if layout_dirty:
        measure_pass(root)           # post-order
        arrange_pass(root, rect)     # pre-order
        layout_dirty = false
    draw_pass(root, draw_list)       # pre-order → DrawList
```

---

## 5. Audio System — `src/sol/audio/`

### 5.1 AudioNode

```c
typedef struct AudioNode AudioNode;

typedef void (*AudioProcessFunc)(AudioNode* self, float* buffer, int n_frames);

struct AudioNode {
    Node base;                    /* tree, refcount, name, flags */

    AudioProcessFunc process_audio;  /* called from audio thread */

    /* Parameter control — set from main thread via control queue */
    void  (*set_param)(AudioNode* self, const char* name, float value);
    float (*get_param)(AudioNode* self, const char* name);

    /* Probe tap */
    float* probe_buffer;          /* ring buffer, NULL if not probed */
    int    probe_write;
    int    probe_size;
};
```

`AudioNode` extends `Node` via struct embedding — same pattern as `Control`. The `process_audio` function pointer is a direct struct member (not in the vtable) for zero-dispatch-overhead in the audio hot path.

### 5.2 AudioPipeline

```c
typedef struct {
    AudioNode*       root;

    /* Thread-safe SPSC control queue */
    ControlMsg       queue[256];
    int              q_read;      /* audio thread */
    int              q_write;     /* main thread */

    /* Audio state */
    int              sample_rate;
    int              buffer_size;
    float*           mix_buffer;     /* working buffer */
    float*           output_buffer;  /* final interleaved output */

    /* Probes */
    ProbeSlot        probes[16];
    int              probe_count;
} AudioPipeline;
```

The audio callback (from SDL3 or ALSA):

```c
void audio_callback(void* userdata, uint8_t* stream, int len) {
    AudioPipeline* ap = (AudioPipeline*)userdata;
    int n = len / sizeof(float);

    ap->drain_control_queue(ap);                 // apply pending param changes
    memset(ap->mix_buffer, 0, n * sizeof(float));
    ap->root->process_audio(ap->root, ap->mix_buffer, n);  // walk tree
    memcpy(stream, ap->mix_buffer, len);
    ap->update_probes(ap);
}
```

### 5.3 AudioNode Types

| Node | Role | Key Parameters |
|------|------|---------------|
| `OscillatorNode` | Produces waveform | `freq`, `amp`, `waveform` (SINE/SQUARE/SAW/TRI/NOISE) |
| `VoiceNode` | Binds osc + env to MIDI note | `note`, `velocity`, `active` |
| `EnvelopeNode` | ADSR amplitude envelope | `attack`, `decay`, `sustain`, `release` |
| `MixerNode` | Sums children, normalizes | — |
| `GainNode` | Multiplies amplitude | `level` |
| `ProbeNode` | Taps signal for GUI | `buffer_size` |
| `LFOFilterNode` *(v2)* | Low-freq modulation | `freq`, `depth`, `target_param` |
| `DelayNode` *(v2)* | Echo/reverb | `time`, `feedback`, `mix` |

### 5.4 Polyphony Model

`VoiceNode` approach: one `VoiceNode` per active note. `MixerNode` sums the children. Voice stealing: when the voice cap is reached, the oldest still-sustaining voice is recycled. This is simple, debuggable, and Godot-like.

```python
# Python composition example
pipeline = AudioPipeline(sample_rate=44100)
mixer = MixerNode(name="main")

for note in active_notes:
    voice = VoiceNode(note=note, velocity=0.8)
    voice.set_oscillator(OscillatorNode.sine(midi_to_freq(note)))
    voice.set_envelope(EnvelopeNode.adsr(0.01, 0.15, 0.7, 0.3))
    mixer.add_child(voice)

pipeline.set_root(mixer)
pipeline.start()   # hooks into platform audio callback
```

### 5.5 Thread Safety

Per `docs/neptune_spec.md` §3:

- The audio callback runs on the platform's audio thread (SDL3 or ALSA). It must never allocate, take locks, or do I/O.
- The main thread sends control messages via a lock-free SPSC ring buffer.
- Probe ring buffers are single-writer (audio thread), single-reader (main/GUI thread). No locks needed.
- Node hot-swap: replace a reference in Python list (atomic under GIL), never partial construction visible.

---

## 6. Rendering — `src/sol/photon/`

### 6.1 Current State

Vulkan renderer with:
- Swapchain management + resize handling
- Two graphics pipelines: main (hello-triangle) + UI (colored rects from DrawList)
- Scissor-based clipping via `CLIP_PUSH`/`CLIP_POP` commands
- Alpha blending on UI pipeline
- SPIR-V shaders assembled from `.vert`/`.frag` sources

### 6.2 3D-Ready Pipeline Architecture

The Vulkan backend already supports multiple pipelines. Future 3D rendering adds:

```
sol_vulkan_frame():
    begin_render_pass(clear_color + clear_depth)
    
    /* Pass 1: 3D world [future] */
    bind_3d_pipeline(depth_test = ON, cull_mode = BACK)
    for each Spatial3D node:
        push_transform(node.global_transform)
        render_mesh(node.mesh, node.material)
    
    /* Pass 2: 2D sprites [future] */
    bind_2d_pipeline(depth_test = OFF, blend = ALPHA)
    sort_by_z_index(spatial_2d_nodes)
    for each Spatial2D node:
        render_sprite(node.sprite_cmd)
    
    /* Pass 3: UI overlay [current] */
    bind_ui_pipeline()
    consume_draw_list(ui_draw_list)
    
    end_render_pass()
    present()
```

New `VkPipeline` objects, new shaders, new descriptor sets. No architectural change — the photon module already handles multi-pipeline rendering.

---

## 7. Text System — `src/sol/text/`

### 7.1 Design

- **stb_truetype** for font rasterization → glyph cache texture atlas
- **UTF-8 iterator** for safe codepoint traversal
- **Emoji detection** via Unicode codepoint ranges → Noto Color Emoji fallback
- **Zero-allocation rendering** — text produces textured quads into the DrawList
- **Memory safe** — font data is immutable, refcounted, never copied

### 7.2 Modules

| File | Purpose |
|------|---------|
| `utf8.h/c` | `Utf8Iter` — validate, iterate, measure UTF-8 strings |
| `font.h/c` | `Font` — load TTF, rasterize glyphs to atlas, produce DrawList text commands |
| `emoji.h/c` | Emoji codepoint classifier + Noto Color Emoji byte array (embedded) |
| `layout.h/c` | Text layout — wrapping, alignment, bounds computation |

### 7.3 API

```c
Font*  font_load(const char* ttf_path, float size_px);
void   font_draw(Font* f, DrawList* dl, const char* utf8, Rect bounds, Color c);
Vec2   font_measure(Font* f, const char* utf8);
bool   text_is_emoji(uint32_t codepoint);
```

---

## 8. Debug System — `src/sol/debug/`

### 8.1 Memory Tracking (`mem.h/c`)

```c
void* sol_malloc_tag(size_t size, const char* file, int line, const char* tag);
void* sol_calloc_tag(size_t n, size_t size, const char* file, int line, const char* tag);
void  sol_free_tag(void* ptr, const char* file, int line);

// Macros for ergonomic usage
#define sol_malloc(sz, tag)     sol_malloc_tag(sz, __FILE__, __LINE__, tag)
#define sol_free(ptr, tag)      sol_free_tag(ptr, __FILE__, __LINE__, tag)

// Queries
size_t           sol_mem_total_allocated(void);
void             sol_mem_dump_leaks(void);
SolAllocRecord*  sol_mem_find(void* ptr);
```

Features: hash-table lookup by pointer, tag-based grouping, leak dump at shutdown, optional guard bytes, compile-time disable (`#ifdef SOL_MEM_TRACKING`).

### 8.2 Logger (`logger.h/c`)

```c
typedef enum { SOL_LOG_TRACE, SOL_LOG_DEBUG, SOL_LOG_INFO,
               SOL_LOG_WARN, SOL_LOG_ERROR, SOL_LOG_FATAL } SolLogLevel;

void sol_log(SolLogLevel level, const char* file, int line,
             const char* tag, const char* fmt, ...);

#define sol_trace(tag, ...)  sol_log(SOL_LOG_TRACE, __FILE__, __LINE__, tag, __VA_ARGS__)
#define sol_debug(tag, ...)  sol_log(SOL_LOG_DEBUG, __FILE__, __LINE__, tag, __VA_ARGS__)
#define sol_info(tag, ...)   sol_log(SOL_LOG_INFO,  __FILE__, __LINE__, tag, __VA_ARGS__)
#define sol_warn(tag, ...)   sol_log(SOL_LOG_WARN,  __FILE__, __LINE__, tag, __VA_ARGS__)
#define sol_error(tag, ...)  sol_log(SOL_LOG_ERROR, __FILE__, __LINE__, tag, __VA_ARGS__)
#define sol_fatal(tag, ...)  sol_log(SOL_LOG_FATAL, __FILE__, __LINE__, tag, __VA_ARGS__)
```

Features: thread-local ring buffer for crash-postmortem, Python callback hook, color TTY output, microsecond timestamps.

### 8.3 Profiler (`prof.h/c`)

```c
void sol_prof_begin(const char* name);
void sol_prof_end(void);
void sol_prof_dump_frame(void);  // print this frame's timing tree

// Macro: auto-closes scope via __attribute__((cleanup))
#define SOL_PROFILE(name) ...
```

---

## 9. Platform Layer — `src/sol/io/`

### 9.1 SolPlatform

```c
typedef struct SolPlatform {
    bool (*init)(const char* title, int w, int h);
    void (*shutdown)(void);
    bool (*update)(void);
    void (*get_size)(int* w, int* h);

    /* Audio */
    bool (*audio_init)(int sample_rate, int channels,
                       SolAudioCallback cb, void* userdata);
    void (*audio_shutdown)(void);
    void (*audio_lock)(void);
    void (*audio_unlock)(void);
} SolPlatform;
```

### 9.2 Backends

| Backend | File | Window | Render | Audio | Target |
|---------|------|--------|--------|-------|--------|
| SDL3 | `io_sdl3.c` | SDL3 | Vulkan | SDL_AudioStream | Linux/Win desktop |
| Headless | `io_headless.c` | — | — | ALSA (`snd_pcm`) | Raspberry Pi, CI |

---

## 10. Python Bindings

### 10.1 Module Map

| Python Module | Wraps | Purpose |
|---------------|-------|---------|
| `sol.bindings` | `core.h` | Engine init/update/shutdown |
| `sol.ui_bindings` | `scene/` | Node, Control, ColorRect, VBoxContainer, SceneTree, DrawList |
| `sol.audio_bindings` | `audio/` | AudioNode, OscillatorNode, VoiceNode, MixerNode, AudioPipeline |
| `sol.text_bindings` | `text/` | Font, text layout |
| `sol.debug_bindings` | `debug/` | Memory stats, log level control |

### 10.2 Binding Pattern

All bindings are hand-written `ctypes` wrappers — no code generation:

```python
import ctypes

_lib = ctypes.CDLL("libsol.so")
_lib.oscillator_node_new.restype = ctypes.c_void_p
_lib.oscillator_node_new.argtypes = [ctypes.c_int, ctypes.c_float, ctypes.c_float]

class OscillatorNode:
    def __init__(self, waveform: int, freq: float, amp: float):
        self._ptr = _lib.oscillator_node_new(waveform, freq, amp)
```

---

## 11. Neptune Synth Layer

Neptune is a **Python composition layer** over the Sol audio engine. It is not a separate codebase — it creates `AudioNode` trees, routes input events, and renders GUI panels using Sol's `Control` widgets.

```
neptune/
├── main.py               # entry point: --headless / --midi / --gui
├── gui/
│   └── synth_panel.py    # Sol Control tree consuming AudioPipeline probes
├── input/
│   ├── keyboard.py       # QWERTY → MIDI note events
│   └── midi.py           # mido → MIDI note events
└── patch.py              # JSON save/load of AudioNode tree
```

Patches are serialized `AudioNode` trees to JSON via each node's `get_params()`/`set_param()` methods — same pattern as the future scene serializer for `Control` trees.

---

## 12. File Formats

### 12.1 Audio Patch (`.solpatch`)

```json
{
  "sample_rate": 44100,
  "root": {
    "type": "MixerNode",
    "name": "main",
    "children": [
      {
        "type": "VoiceNode",
        "name": "lead",
        "params": { "note": 60, "velocity": 0.8 },
        "children": [
          { "type": "OscillatorNode", "params": { "waveform": "SINE", "freq": 261.6, "amp": 1.0 }},
          { "type": "EnvelopeNode", "params": { "attack": 0.01, "decay": 0.15, "sustain": 0.7, "release": 0.3 }}
        ]
      },
      { "type": "GainNode", "params": { "level": 0.8 }}
    ]
  }
}
```

### 12.2 UI Scene (`.solscene`) — future

Same pattern for `Control` trees.

---

## 13. Project Layout

```
sol/
├── game/main.py                  ← application entry point
├── examples/
│   ├── ui_example.py             ← UI system demo
│   └── extending/                ← copy-paste templates
├── src/
│   ├── sol/                      ← C99 engine
│   │   ├── core.h / core.c       ← engine init/update/shutdown
│   │   ├── io/                   ← platform backends
│   │   │   ├── io.h
│   │   │   ├── io_sdl3.c         ← SDL3: window + Vulkan + audio
│   │   │   └── io.c              ← headless: ALSA audio
│   │   ├── photon/               ← Vulkan renderer
│   │   │   ├── photon_vulkan.h/c
│   │   │   └── photon.h
│   │   ├── scene/                ← 2D retained-mode UI
│   │   │   ├── node.h/c          ← base Node + vtable
│   │   │   ├── control.h/c       ← rect, anchors, layout
│   │   │   ├── draw_list.h/c     ← renderer-agnostic draw commands
│   │   │   ├── scene_tree.h/c    ← frame orchestrator
│   │   │   ├── color_rect.h/c    ← colored rectangle widget
│   │   │   └── vbox_container.h/c← vertical layout container
│   │   ├── audio/                ← audio synthesis nodes (PHASE 1)
│   │   │   ├── audio_node.h/c    ← AudioNode base type
│   │   │   ├── pipeline.h/c      ← AudioPipeline + control queue
│   │   │   ├── osc.h/c           ← OscillatorNode (sine/square/saw/tri)
│   │   │   ├── voice.h/c         ← VoiceNode (osc + env per note)
│   │   │   ├── envelope.h/c      ← EnvelopeNode (ADSR)
│   │   │   ├── mixer.h/c         ← MixerNode (sum children)
│   │   │   ├── gain.h/c          ← GainNode (amplitude multiply)
│   │   │   └── probe.h/c         ← ProbeNode (ring buffer tap)
│   │   ├── text/                 ← unicode text + emoji (PHASE 3)
│   │   │   ├── utf8.h/c
│   │   │   ├── font.h/c
│   │   │   ├── emoji.h/c
│   │   │   └── layout.h/c
│   │   ├── debug/                ← memory + logging (PHASE 3)
│   │   │   ├── mem.h/c
│   │   │   ├── logger.h/c
│   │   │   └── prof.h/c
│   │   ├── spatial3d/            ← [future] 3D scene nodes
│   │   ├── spatial2d/            ← [future] 2D world nodes
│   │   └── shaders/              ← SPIR-V assembly sources
│   └── sol/                      ← Python package
│       ├── __init__.py
│       ├── bindings.py           ← ctypes engine bindings
│       ├── ui_bindings.py        ← ctypes UI bindings
│       ├── audio_bindings.py     ← ctypes audio bindings
│       └── sol.py                ← public API re-exports
├── neptune/                      ← Python synth composer layer
│   ├── main.py
│   ├── gui/synth_panel.py
│   ├── input/keyboard.py
│   ├── input/midi.py
│   └── patch.py
├── scripts/build_engine.py       ← C compilation + SPIR-V assembly
├── setup.py
├── pyproject.toml
└── docs/                         ← specifications
    ├── sol_spec.md               ← THIS FILE — master architecture spec
    ├── scene_spec.md             ← UI system detailed spec
    ├── neptune_spec.md           ← Neptune synth detailed spec
    ├── build_spec.md             ← Build system spec
    └── roadmap.md                ← Development phases + progress
```

---

## 14. Development Roadmap

| Phase | Deliverable | Status |
|-------|-------------|--------|
| **1** | Fix build system. AudioNode base + OscNode + MixerNode + GainNode. AudioPipeline with control queue. SDL3 audio + ALSA audio. Hello sound from QWERTY. | 🔜 |
| **2** | EnvelopeNode (ADSR). VoiceNode (osc+env per note). Polyphony + voice stealing. MIDI input via mido. | ⬜ |
| **3** | Memory tracking (debug/mem). Leveled logger (debug/logger). Profiler (debug/prof). Migrate all C code to sol_malloc/sol_log. | ⬜ |
| **4** | UTF-8 iterator. Font rasterization (stb_truetype). Emoji detection. Glyph cache. Text rendering into DrawList. | ⬜ |
| **5** | HBoxContainer, MarginContainer, Label, Button widgets. Signal system. Input event routing. | ⬜ |
| **6** | Neptune synth panel GUI (Sol Control tree). Waveform probe display. Knob widgets. Patch JSON save/load. | ⬜ |
| **7** | Example templates. Raspberry Pi validation. MIDI keyboard end-to-end. Latency profiling. | ⬜ |
| **8** | Spatial2D + Sprite2D + Camera2D. 2D physics (Box2D). | ⬜ |
| **9** | Spatial3D + MeshInstance3D + Camera3D. 3D pipeline in photon. | ⬜ |

---

## 15. Anti-Goals

- **No C++.** C99 only. `gcc --std=c99 -pedantic`.
- **No visual editor.** Code is the editor.
- **No generated bindings.** Hand-written `ctypes`.
- **No universal Variant type.** Structs, always.
- **No deep class hierarchies.** Flat vtables, struct embedding.
- **No 3D yet.** Architecture supports it; code doesn't implement it yet.

---

*Specification version 1.0 — Sol Engine Project*
