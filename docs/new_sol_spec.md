# Grimoire Engine Specification v0.1
## Python + C99 Game Engine Architecture

> **Design Philosophy:** Godot's scene tree and composition model, without Godot's binding-layer tax. C99 for the hot path, Python for the logic path, `ctypes` for zero-overhead interop. Memory safety through explicit reference counting and flat vtables, not C++ inheritance.

---

## 1. Design Principles

1. **No Universal Variant.** C functions return C structs. Python reads them via `ctypes.Structure`. No boxing, no indirection, no `Dictionary` returns.
2. **No Binding Layer.** Python calls C directly through `ctypes.CDLL`. No `MethodBind`, no `ClassDB` lookup at call time, no generated glue code.
3. **Flat Vtables, Not Deep Hierarchies.** A `Node` is a C struct with a vtable pointer. Inheritance is composition of behavior, not `class Sprite2D : public Node2D : public CanvasItem`.
4. **Stack-First API.** Query parameters, raycast results, transform matrices—everything that can live on the stack, does. No heap allocations for transient data.
5. **Python is the Editor.** No custom IDE. Projects are folders with `.py` scripts and `.gr` asset files. `uv run` is the launch command.

---

## 2. Architecture Overview

```
┌─────────────────────────────────────────────┐
│           Python Game Scripts               │
│  (single-file demos, scene composition,     │
│   game logic, coroutines, hot-reload)       │
├─────────────────────────────────────────────┤
│           Python Engine Module              │
│  (thin wrapper: loads libgrimoire.so,       │
│   exposes Node/Vec2/Color/Input classes)    │
├─────────────────────────────────────────────┤
│              ctypes (zero overhead)         │
├─────────────────────────────────────────────┤
│           libgrimoire.so (C99)              │
│  ┌─────────┐ ┌─────────┐ ┌──────────────┐  │
│  │  Node   │ │Renderer │ │  Audio Mix   │  │
│  │ System  │ │ (OpenGL)│ │  (miniaudio) │  │
│  └─────────┘ └─────────┘ └──────────────┘  │
│  ┌─────────┐ ┌─────────┐ ┌──────────────┐  │
│  │ Physics │ │  Input  │ │  Asset Cache │  │
│  │ (box2d) │ │(glfw)   │ │  (stb_image) │  │
│  └─────────┘ └─────────┘ └──────────────┘  │
└─────────────────────────────────────────────┘
```

---

## 3. Core C99 API

### 3.1 The Node & VTable

Every entity in the engine is a `Node`. There is no inheritance chain—only a flat vtable and a bag of child nodes.

```c
typedef struct Node Node;
typedef struct Scene Scene;

// Lifecycle callbacks. All are optional (NULL = no-op).
typedef struct {
    const char* type_name;
    uint32_t    type_id;

    // Called once when the node enters the active scene tree.
    void (*ready)(Node* self);

    // Called every frame with delta time in seconds.
    void (*process)(Node* self, float dt);

    // Called at fixed timestep (default 60Hz).
    void (*physics_process)(Node* self, float dt);

    // Called when the node enters/exits the tree.
    void (*enter_tree)(Node* self);
    void (*exit_tree)(Node* self);

    // Rendering callback. Receives a RenderContext*.
    void (*draw)(Node* self, void* ctx);

    // Input event callback.
    void (*input_event)(Node* self, const InputEvent* ev);

    // Cleanup. Decrement refcount on owned children.
    void (*free)(Node* self);
} NodeVTable;

struct Node {
    Node*        parent;
    Node**       children;
    uint32_t     child_count;
    uint32_t     child_capacity;

    const NodeVTable* vtable;

    // Transform in local space.
    Vec2         position;
    float        rotation;
    Vec2         scale;

    // Memory safety.
    uint32_t     refcount;

    // Scripting bridge.
    void*        script_handle;      // opaque pointer back to PyObject*
    uint64_t     instance_id;        // unique stable ID (like Godot's ObjectID)

    // User data for component-style behavior.
    void*        user_data;
    size_t       user_data_size;
};
```

### 3.2 Node Management API

```c
// Creation & lifecycle.
Node* node_create(const NodeVTable* vtable, const char* name);
Node* node_ref(Node* node);          // increment refcount.
void  node_unref(Node* node);        // decrement; free at 0.
void  node_add_child(Node* parent, Node* child);
void  node_remove_child(Node* parent, Node* child);
Node* node_get_child(Node* parent, uint32_t idx);
Node* node_find(Node* root, const char* path);  // "Player/Hand/Sword"

// Transform helpers.
Vec2  node_global_position(const Node* node);
float node_global_rotation(const Node* node);
```

### 3.3 Rendering API (Immediate-Mode Sprites)

No retained scene graph for drawables. Nodes draw themselves via the `draw` callback. The engine batches sprites automatically.

```c
typedef struct {
    uint32_t texture_id;
    Rect2    source;        // UV rect in texture.
    Rect2    dest;          // screen-space rect (set by draw callback).
    Color    modulate;
    float    z_index;
} SpriteCommand;

// Called inside node->draw(). Queues a sprite for the current frame.
void render_submit(const SpriteCommand* cmd);

// Draw primitives (useful for debug, particles, vector games).
void render_line(Vec2 a, Vec2 b, Color color, float width);
void render_rect(Rect2 rect, Color color, bool filled);
void render_circle(Vec2 center, float radius, Color color, bool filled);
void render_text(const char* text, Vec2 pos, Color color, float size);
```

### 3.4 Input API

```c
typedef struct {
    uint32_t type;          // KEY, MOUSE_BUTTON, MOUSE_MOTION, etc.
    uint32_t keycode;       // GLFW keycode.
    uint32_t modifiers;     // shift, ctrl, alt.
    Vec2     mouse_pos;     // in screen pixels.
    Vec2     mouse_delta;
    bool     pressed;
    bool     repeated;
} InputEvent;

bool input_is_key_pressed(uint32_t keycode);
bool input_is_mouse_button_pressed(uint32_t button);
Vec2 input_mouse_position(void);
Vec2 input_mouse_delta(void);
```

### 3.5 Physics API (2D)

Lightweight wrapper around a fixed-timestep system. Bodies are nodes with a `PhysicsBody` component attached via `user_data`.

```c
typedef struct {
    Vec2  position;
    Vec2  normal;
    float distance;
    Node* collider;         // NULL if no hit.
} RaycastResult;

// Stack-allocated query. No heap garbage.
typedef struct {
    Vec2  origin;
    Vec2  direction;
    float max_distance;
    uint32_t collision_mask;
} RaycastQuery;

bool physics_raycast(const RaycastQuery* query, RaycastResult* out);

// Body API.
typedef enum { STATIC, KINEMATIC, DYNAMIC } BodyType;
void physics_body_create(Node* node, BodyType type);
void physics_body_set_velocity(Node* node, Vec2 vel);
Vec2 physics_body_get_velocity(Node* node);
void physics_body_apply_impulse(Node* node, Vec2 impulse, Vec2 point);
```

### 3.6 Audio API

```c
typedef struct Sound Sound;
Sound* sound_load(const char* path);        // stb_vorbis + miniaudio.
void   sound_play(Sound* sound, float volume, float pitch);
void   sound_play_oneshot(Sound* sound);    // fire-and-forget.
```

---

## 4. Python API Layer

The Python module (`grimoire.py`) is a thin, hand-written wrapper over `ctypes`. It is **not** generated from reflection metadata.

### 4.1 Python Node Class

```python
import ctypes
from grimoire_native import lib, Vec2, Color, Rect2

class Node:
    __slots__ = ('_ptr', '_children', '_script_ref')

    def __init__(self, name: str = "Node"):
        self._ptr = lib.node_create(self._vtable(), name.encode())
        self._children = []
        self._script_ref = self  # keep alive until unref'd

    def _vtable(self):
        # Returns a ctypes pointer to the vtable for this Python class.
        # Cached per-class.
        pass

    @property
    def position(self) -> Vec2:
        return self._ptr.contents.position
    @position.setter
    def position(self, v: Vec2):
        self._ptr.contents.position = v

    def add_child(self, child: 'Node'):
        lib.node_add_child(self._ptr, child._ptr)
        self._children.append(child)

    def process(self, dt: float):
        """Override in subclass."""
        pass

    def draw(self, ctx):
        """Override in subclass. Called every frame."""
        pass
```

### 4.2 Hot Reloading

Python nodes keep a `script_handle` back-pointer to their PyObject. During development, the engine can:

1. Save the scene tree state (positions, velocities, health).
2. Reload the `.py` module.
3. Re-instantiate Python node subclasses.
4. Restore state.

This gives GDScript-like iteration speed without a custom compiler.

### 4.3 Coroutines / Yield

Python's `async`/`await` is used for cutscenes, timers, and tweens:

```python
async def animate_in(self):
    self.position = Vec2(-100, 0)
    await tween(self.position, Vec2(0, 0), duration=0.5, easing=ease_out_back)
    await wait(0.2)
    self.visible = True
```

The engine's `process` loop drives an `asyncio` event loop each frame.

---

## 5. Project Structure & Build System

```
my_game/
├── pyproject.toml          # uv project; depends on grimoire-engine
├── uv.lock
├── main.py                 # entry point; creates window, loads scene
├── player.py               # Player node subclass
├── enemy.py
├── assets/
│   ├── sprites/
│   ├── sounds/
│   └── tilemaps/
└── scenes/
    └── level_01.json       # serialized node tree (names, positions, scripts)
```

### 5.1 Running

```bash
uv run main.py
```

The `grimoire-engine` package ships:
- `libgrimoire.so` / `.dll` / `.dylib`
- `grimoire/__init__.py` (Python wrappers)
- `grimoire/assets.py` (texture/sound loaders)

### 5.2 Packaging

```bash
# Build a standalone executable (Python + .so embedded).
uv build --wheel
# Or use pyinstaller/shiv for distribution.
```

---

## 6. Memory Safety Model

| Rule | Implementation |
|------|---------------|
| **C owns the tree.** | `node_add_child` increments child refcount. `node_remove_child` decrements it. |
| **Python owns the script.** | `Node.__init__` stores `self` in `script_handle`. C never frees the PyObject. |
| **Cross-language refs.** | Python holds `Node` objects. When Python GC collects a `Node`, it calls `node_unref`. |
| **Dangling pointers.** | Never store `Node*` long-term. Store `instance_id` and resolve via `node_get_by_id()`. |
| **User data.** | `node_create` can allocate a POD struct into `user_data`. Freed by the `free` vtable callback. |

---

## 7. File Formats

### 7.1 Scene Format (`.grscene`)

Plain JSON. A scene is a tree of nodes. Each node specifies:
- `type`: Python module path (`player.Player`, `engine.Sprite`)
- `name`: node name in tree
- `transform`: `position`, `rotation`, `scale`
- `properties`: arbitrary JSON passed to the node's `ready()`
- `children`: recursive list

```json
{
  "name": "Main",
  "type": "engine.Node",
  "children": [
    {
      "name": "Player",
      "type": "player.Player",
      "position": [100, 200],
      "properties": { "speed": 300 }
    },
    {
      "name": "Background",
      "type": "engine.Sprite",
      "texture": "assets/bg.png",
      "z_index": -1
    }
  ]
}
```

### 7.2 Asset Pipeline

No import step. At runtime:
- Images → `stb_image` → OpenGL texture.
- Audio → `stb_vorbis` / `dr_wav` → miniaudio buffer.
- Fonts → `stb_truetype` → glyph cache texture.

---

## 8. Rendering Pipeline (2D)

1. **Collect:** Each frame, the engine walks the tree and calls `node->draw()` on visible nodes.
2. **Batch:** `render_submit()` calls are sorted by texture and z-index, then batched into a single draw call per texture.
3. **Post-process:** Optional fullscreen passes (CRT scanlines, chromatic aberration, bloom) via a second FBO.
4. **Present:** GLFW swap buffers.

**Coordinate system:** Y-down, pixels by default. Camera is a node with a `view_matrix` that transforms the root projection.

---

## 9. Example Game Catalog

Each game is a **single Python file** (~50–200 lines) demonstrating a specific engine capability. They are designed to be copy-pasted, run with `uv run`, and immediately impressive.

### 9.1 Core Capability Matrix

| Game | Sprites | Physics | Particles | Input | Audio | Shaders | Async |
|------|---------|---------|-----------|-------|-------|---------|-------|
| 01. Vectoroids | | ✓ | ✓ | ✓ | ✓ | | |
| 02. Slingshot Stack | ✓ | ✓ | | ✓ | ✓ | | |
| 03. Neon Snake | ✓ | | ✓ | ✓ | | ✓ | |
| 04. Cellular Rift | | | ✓ | ✓ | | ✓ | |
| 05. Cyber Pong | ✓ | | | ✓ | ✓ | ✓ | |
| 06. Cave Runner | ✓ | ✓ | ✓ | ✓ | ✓ | | ✓ |
| 07. Orbital Defense | ✓ | ✓ | ✓ | ✓ | ✓ | | |
| 08. Shadow Maze | ✓ | | | ✓ | | ✓ | |
| 09. Synth Bounce | ✓ | ✓ | | ✓ | ✓ | ✓ | |
| 10. Starfield Drift | ✓ | | ✓ | ✓ | | ✓ | ✓ |

---

### 9.2 The Games

#### 01. Vectoroids
*Asteroids clone with vector-style line rendering.*
- **Shows off:** `render_line()`, `render_circle()`, immediate-mode primitives, wrap-around world, simple rigid-body physics for asteroid chunks.
- **Twist:** Asteroids split into procedurally generated convex polygons. Screen shake on impact.
- **Lines of code:** ~120

#### 02. Slingshot Stack
*Angry Birds meets Tetris. Knock over a tower of blocks.*
- **Shows off:** 2D physics bodies (static, dynamic), mouse-drag input, collision callbacks, sprite-based blocks with different densities.
- **Twist:** Blocks are colored based on their material (wood, stone, glass). Glass shatters into particle shards on impact.
- **Lines of code:** ~150

#### 03. Neon Snake
*Classic snake with a glowing trail and smooth sub-grid movement.*
- **Shows off:** Sprite batching, particle trail system, post-processing glow shader, tweened movement.
- **Twist:** The snake leaves a fading neon trail behind each segment. Eating an apple triggers a chromatic aberration pulse.
- **Lines of code:** ~100

#### 04. Cellular Rift
*Conway's Game of Life where you paint cells with the mouse and watch ecosystems battle.*
- **Shows off:** Compute-style pixel manipulation (via texture lock/update), immediate-mode rect rendering, massive entity counts (10k+ cells).
- **Twist:** Three species (RGB) with different survival rules. Click to drop a "meteor" that kills in a radius.
- **Lines of code:** ~130

#### 05. Cyber Pong
*Pong with CRT scanlines, screen shake, and synthwave aesthetics.*
- **Shows off:** Post-processing CRT shader, audio synthesis (beep generation without asset files), basic AI opponent, tweened UI.
- **Twist:** The ball speeds up with each hit. At high speed, the ball leaves a motion-blur ghost trail.
- **Lines of code:** ~110

#### 06. Cave Runner
*Auto-running cave flyer. Hold space to rise, release to fall. Dodge stalactites.*
- **Shows off:** Parallax scrolling backgrounds, procedural level generation (perlin noise heightmap), coroutine-based tweens, sprite animation, looping audio.
- **Twist:** The cave narrows and widens to the music's BPM. Collectible orbs give temporary slow-motion.
- **Lines of code:** ~160

#### 07. Orbital Defense
*Missile Command on a planet. Click to launch counter-missiles from a rotating satellite.*
- **Shows off:** Polar coordinate math, particle explosions, collision circles, audio pitch randomization, scene composition (planet + cities + satellites).
- **Twist:** The satellite orbits automatically but can be nudged with arrow keys. Cities rebuild slowly via async timer.
- **Lines of code:** ~140

#### 08. Shadow Maze
*Top-down stealth game. You are the light source. Guards have vision cones.*
- **Shows off:** 2D lighting/shadow casting (ray marching from player), line-of-sight checks, tilemap rendering, A* pathfinding for guards.
- **Twist:** Standing still makes you invisible. Guards leave a "last known position" marker.
- **Lines of code:** ~180

#### 09. Synth Bounce
*Audio-visual physics toy. Balls bounce and synthesize notes on collision.*
- **Shows off:** Physics + audio synthesis integration, color-coded pitches, mouse spawning, gravity toggle.
- **Twist:** Balls are tuned to a pentatonic scale. More balls = richer chords. Background grid pulses to the total kinetic energy.
- **Lines of code:** ~90

#### 10. Starfield Drift
*Endless runner through an asteroid field. Dodge left/right.*
- **Shows off:** 3D-projected starfield (fake 3D in 2D), particle thrusters, async power-up timers, shader-based hyperspace warp effect.
- **Twist:** Hold shift for hyperspeed (warps stars into lines). Collisions trigger slow-motion bullet time for 2 seconds.
- **Lines of code:** ~150

---

## 10. Development Roadmap

| Phase | Deliverable |
|-------|-------------|
| **0.1** | Window, input, `Node` + vtables, sprite batching, single-file game support. |
| **0.2** | Box2D physics integration, audio (miniaudio), `.grscene` loader. |
| **0.3** | Post-processing shaders (CRT, bloom), particle system, font rendering. |
| **0.4** | Hot reload, `async` tween/coroutine library, tilemap support. |
| **0.5** | Asset streaming, threaded loading, profiler overlay. |

---

## 11. Anti-Goals (What We Will NOT Do)

- **No 3D.** The engine is 2D-only. 3D means a different architecture.
- **No visual editor.** Code is the editor. Scenes are JSON + Python.
- **No C++.** C99 only. If it compiles with `gcc --std=c99 -pedantic`, it ships.
- **No GDScript-like language.** Python is the scripting language. Full stop.
- **No universal Variant.** If a C function returns something, it returns a struct.
- **No generated bindings.** Hand-written `ctypes` wrappers. They are readable and debuggable.

---

*Specification version 0.1 — Grimoire Engine Project*
