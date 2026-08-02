# Sol UI Engine Specification
## A Godot-Inspired Retained-Mode UI System for C99

> **TL;DR:** Retained-mode 2D UI in C99. Node tree with manual vtables + refcounting. Control = rect + anchors + size flags. Layout = two-pass (measure bottom-up, arrange top-down). DrawList = renderer-agnostic command buffer. SceneTree = process→layout→draw each frame. Python wrappers via ctypes.

**Version:** 1.0-draft  
**Date:** 2026-07-28  
**Status:** Prototype implemented at `src/engine/ui/`  
**Language:** C99 (ISO/IEC 9899:1999)  
**Target Wrapper:** Python (via ctypes)  

> **Implementation status (2026-07-30):** Core components prototyped and functional.
> See `examples/ui_example.py` for a Python demo. Implemented:
> - Node (vtable, refcounting, tree ops)
> - Control (rect, anchors, offsets, size_flags)
> - DrawList (filled rect, border, text, clip push/pop)
> - SceneTree (process → two-pass layout → draw)
> - ColorRect widget
> - VBoxContainer (SIZE_FILL, SIZE_EXPAND, SIZE_SHRINK_*, separation)
>
> Not yet implemented:
> - HBoxContainer, MarginContainer, CenterContainer
> - Button, Label, LineEdit, TextureRect
> - Signal system
> - Theme / StyleBox
> - Input events
> - Font rendering (stb_truetype)

---

## 1. Introduction & Goals

This document specifies the architecture and API of the Sol UI Engine (working title), a retained-mode 2D user-interface library written in C99. It is designed to be embedded in a game engine and wrapped for Python programmers.

### 1.1 Primary Goals
- **Godot-Parity**: Replicate the core UX of Godot's `Control` node system: anchors, containers, size flags, signals, and theme-driven rendering.
- **C99 Purity**: Compile under a strict C99 compiler with no C++ features, no RTTI, and minimal external dependencies.
- **Python-First Ergonomics**: The C API must be structured so that a Cython wrapper can expose a natural, class-based Python API with property access, method overrides, and signal connections.
- **Renderer Agnostic**: The library emits draw commands; it does not touch OpenGL, Vulkan, or DirectX directly.
- **Single-Threaded Simplicity**: The entire UI system runs on one thread. Thread-safety is the responsibility of the host application.

### 1.2 Non-Goals
- A declarative markup language (XML, JSON, YAML) is out of scope for v1.0.
- Accessibility APIs (screen readers, OS integration) are out of scope.
- 3D UI rendering is out of scope.

---

## 2. Design Principles

1. **Tree of Responsibility**: Every UI element is a node in a scene tree. Ownership, drawing, input, and layout are all tree-structured operations.
2. **Layout is Constraint Solving**: Layout is a two-pass constraint problem (measure bottom-up, arrange top-down). Containers are layout algorithms; non-container Controls are layout leaves.
3. **Separation of Concerns**:
   - **Node**: Lifetime, tree hierarchy, processing.
   - **Control**: Rect, layout, input hit-testing, theming.
   - **Widget**: Specialized behavior (Button, Label).
   - **DrawList**: Rendering abstraction.
4. **Explicit over Implicit**: Memory ownership, coordinate spaces, and event propagation must be obvious from the C API. No hidden global state.
5. **Refcount Everything**: All heap-allocated nodes and resources are reference-counted. This makes Python bridging safe and predictable.

---

## 3. Architecture Overview

```
┌─────────────────────────────────────────────────────────────┐
│                    SceneTree                              │
│  ┌─────────────┐  ┌─────────────┐  ┌─────────────────────┐  │
│  │  Process    │  │   Input     │  │   Layout Solver     │  │
│  │  Loop       │  │   Router    │  │   (2-pass)          │  │
│  └──────┬──────┘  └──────┬──────┘  └──────────┬──────────┘  │
│         │                │                     │             │
│         ▼                ▼                     ▼             │
│  ┌──────────────────────────────────────────────────────┐   │
│  │              Node Tree (Scene)                     │   │
│  │   Control (rect, anchors, theme)                  │   │
│  │   ├── Container (layout algorithm)                │   │
│  │   │   ├── HBoxContainer                         │   │
│  │   │   ├── VBoxContainer                         │   │
│  │   │   └── GridContainer                         │   │
│  │   └── Widget (behavior)                         │   │
│  │       ├── Label                                 │   │
│  │       ├── Button                                │   │
│  │       └── LineEdit                              │   │
│  └──────────────────────────────────────────────────────┘   │
│                            │                                 │
│                            ▼                                 │
│                   ┌─────────────────┐                        │
│                   │   DrawList   │  ← renderer-agnostic   │
│                   │   (primitives)  │     command buffer     │
│                   └─────────────────┘                        │
└─────────────────────────────────────────────────────────────┘
```

---

## 4. The Object System

C99 has no inheritance. We simulate it via **type erasure with manual vtables**.

### 4.1 Base Node (`Node`)

Every object in the system is a `Node`. The first member of every derived struct MUST be `Node base;` (struct embedding).

```c
/* Opaque forward declarations */
typedef struct Node Node;
typedef struct NodeClass NodeClass;

/* Every node instance */
struct Node {
    const NodeClass *klass;      /* Virtual method table (read-only) */
    Node *parent;                /* Weak reference; NULL if root */
    Node **children;             /* Dynamic array of strong references */
    size_t child_count;
    size_t child_capacity;
    char *name;                     /* Debug/diagnostic name; UTF-8 */
    uint32_t flags;                 /* Visibility, process, etc. */
    void *user_data;                /* Reserved for language bindings */
    int refcount;                   /* Reference count; starts at 1 */
};
```

### 4.2 Virtual Method Table (`NodeClass`)

Each concrete type has one global, const `NodeClass` instance. All instances of a type share the same vtable.

```c
struct NodeClass {
    const char *type_name;
    size_t instance_size;           /* sizeof(DerivedStruct) */

    /* Lifecycle */
    void (*init)(Node *self);    /* Called once after allocation */
    void (*destroy)(Node *self); /* Called once before deallocation */
    void (*enter_tree)(Node *self);
    void (*exit_tree)(Node *self);
    void (*ready)(Node *self);   /* Called once, post-order, after enter_tree */

    /* Per-frame */
    void (*process)(Node *self, float delta);
    void (*draw)(Node *self, struct DrawList *dl);

    /* Layout */
    void (*get_minimum_size)(Node *self, Vec2 *out);

    /* Input */
    int (*handle_input)(Node *self, const struct InputEvent *ev);
};
```

**Rules:**
- A NULL vtable slot is legal and means "no operation" for that type.
- `init` MUST call `node_init(self, klass)` to set up the base `Node` fields before doing type-specific work.
- `destroy` MUST call `node_destroy(self)` after type-specific cleanup to free base fields.

### 4.3 Type Safety Macros

```c
#define NODE(obj)        ((Node*)(obj))
#define CONTROL(obj)     ((Control*)(obj))
#define IS_CONTROL(obj)  ((obj) && node_is_type((obj), "Control"))
```

Type names are compared by string for debug assertions only; runtime dispatch uses the vtable pointer.

---

## 5. The Scene Tree

### 5.1 SceneTree (`SceneTree`)

The `SceneTree` is the owner of the root node and the executor of the frame loop.

```c
typedef struct {
    Node *root;
    Node *focused_control;       /* Weak reference */
    Node **input_stack;          /* Nodes under cursor, top-down */
    size_t input_stack_count;
    uint8_t layout_dirty;           /* If true, run layout before draw */
    uint8_t tree_changing;          /* Reentrance guard */
} SceneTree;
```

### 5.2 Lifecycle Order

When a node is added to the tree:
1. **Enter Tree** — pre-order traversal from the new node downward.
2. **Ready** — post-order traversal from the deepest child upward. Fires exactly once per tree entry.

When a node is removed:
1. **Exit Tree** — post-order traversal from the deepest child upward.

Per frame:
1. **Process** — pre-order traversal. `delta` is in seconds.
2. **Layout** — if `layout_dirty` is set, two-pass solve (see §6).
3. **Draw** — pre-order traversal. Each node appends to a `DrawList`.

### 5.3 Tree Mutation Rules

- A node may have **at most one parent**.
- `node_add_child(parent, child)`:
  - Increments `child->refcount`.
  - If `parent` is inside an active tree, triggers Enter Tree on `child` and its descendants.
  - Marks layout dirty on the nearest `Control` ancestor.
- `node_remove_child(parent, child)`:
  - Triggers Exit Tree on `child` and its descendants.
  - Removes `child` from the children array.
  - Decrements `child->refcount` (may free it).
- `node_free(node)`:
  - If `node` has a parent, calls `node_remove_child(parent, node)` first.
  - Decrements refcount. If it reaches zero, calls `destroy` vtable and frees memory.

---

## 6. Control & Layout

`Control` is the base class for all visible, interactive UI elements.

### 6.1 Data Structure

```c
typedef struct {
    Node base;

    /* Rects */
    Rect rect;                   /* Local rect; relative to parent */
    Rect global_rect;            /* Cached screen-space rect */

    /* Anchors (0.0 – 1.0, relative to parent rect) */
    struct {
        float left;
        float top;
        float right;
        float bottom;
    } anchor;

    /* Offsets (pixels, added after anchor multiplication) */
    struct {
        float left;
        float top;
        float right;
        float bottom;
    } offset;

    /* Sizing */
    Vec2 min_size;
    uint32_t size_flags_h;          /* SIZE_FILL, SIZE_EXPAND, etc. */
    uint32_t size_flags_v;

    /* Theming */
    Theme *theme;                /* Strong ref; NULL to inherit */

    /* Input */
    uint8_t mouse_filter;           /* PASS, STOP, IGNORE */
    uint8_t focus_mode;             /* NONE, CLICK, ALL */
} Control;
```

### 6.2 Coordinate System

- **Origin**: Top-left of the parent rect.
- **Y-axis**: Positive downward.
- **Units**: Pixels (logical; DPI scaling is handled by the renderer backend).

### 6.3 Anchor Layout (Free-form)

The final local rect of a Control is computed as:

```
rect.x = parent.x + parent.w * anchor.left  + offset.left
rect.y = parent.y + parent.h * anchor.top   + offset.top
rect.w = parent.w * (anchor.right - anchor.left) + offset.right - offset.left
rect.h = parent.h * (anchor.bottom - anchor.top) + offset.bottom - offset.top
```

After computation, `rect.w` and `rect.h` are clamped to `min_size`.

**Preset helpers** (convenience functions):
- `control_set_anchor_full_rect(c)` → anchors = {0,0,1,1}, offsets = {0,0,0,0}
- `control_set_anchor_top_left(c)` → anchors = {0,0,0,0}
- `control_set_anchor_top_right(c)` → anchors = {1,0,1,0}, offsets negative

### 6.4 Container Layout (Automatic)

A `Container` is a `Control` that ignores its children's `anchor`/`offset` values and overwrites their `rect` fields.

**Container base:**
```c
typedef struct {
    Control base;
} Container;
```

Containers implement a custom `layout` method (not in the public vtable; invoked internally by the solver). The default `Control` layout applies anchors; `Container` subclasses override this.

### 6.5 Size Flags

Children communicate layout preferences to their parent container via bitwise flags.

```c
#define SIZE_FILL          (1u << 0)  /* Occupy all available space */
#define SIZE_EXPAND        (1u << 1)  /* Claim extra space if available */
#define SIZE_SHRINK_BEGIN  (1u << 2)  /* Align to start */
#define SIZE_SHRINK_CENTER (1u << 3)  /* Align to center */
#define SIZE_SHRINK_END    (1u << 4)  /* Align to end */
```

A child may set both `FILL` and `EXPAND`. `SHRINK_*` are mutually exclusive.

### 6.6 The Two-Pass Layout Solver

This algorithm runs whenever `layout_dirty` is true.

**Pass 1: Measure (bottom-up post-order)**
```
function measure(node):
    if node is Container:
        for child in node.children:
            measure(child)
        node.min_size = container_compute_min_size(node)
    else if node is Control:
        if node.klass->get_minimum_size:
            node.klass->get_minimum_size(node, &node.min_size)
        else:
            node.min_size = node.explicit_min_size
    else:
        node.min_size = (0, 0)
```

**Pass 2: Arrange (top-down pre-order)**
```
function arrange(node, parent_rect):
    if node is Control:
        if node.parent is Container:
            /* Rect was already set by the parent container */
            pass
        else:
            compute_rect_from_anchors(node, parent_rect)

        node.global_rect = transform_to_screen(node.rect, node.parent)

    if node is Container:
        container_arranchildren(node)  /* sets child rects directly */

    for child in node.children:
        arrange(child, node.rect)
```

**Dirty propagation:**
- Any change to `min_size`, `anchor`, `offset`, `size_flags`, or child list on a `Control` marks the nearest `Container` ancestor (or the root) as dirty.
- The tree runs at most one layout solve per frame, just before drawing.

### 6.7 Container Types (v1.0)

| Container | Behavior |
|-----------|----------|
| `MarginContainer` | Adds uniform or per-side padding around its single child. |
| `HBoxContainer` | Arranges children horizontally left-to-right. Respects `size_flags_h`. |
| `VBoxContainer` | Arranges children vertically top-to-bottom. Respects `size_flags_v`. |
| `CenterContainer` | Centers its single child within its own rect. |
| `PanelContainer` | Draws a background stylebox, then lays out its single child inside it. |

**Container arrangement contract:**
- A container MUST set the `rect` of every direct `Control` child.
- A container MUST NOT modify the `anchor`/`offset` of its children (it ignores them).
- A container SHOULD respect each child's `min_size`.

---

## 7. Rendering Abstraction

The UI library does not render directly. It builds a `DrawList` — an array of draw commands consumed by the host renderer.

### 7.1 Draw Commands

```c
typedef enum {
    CMD_RECT_FILLED,
    CMD_RECT_BORDER,
    CMD_TEXT,
    CMD_TEXTURE,
    CMD_CLIP_PUSH,       /* Scissor / clip rect */
    CMD_CLIP_POP,
    CMD_PATH_START,      /* For custom vector shapes (future) */
    CMD_PATH_END,
} DrawCmdType;

typedef struct {
    DrawCmdType type;
    Rect rect;
    Color color;

    /* For CMD_TEXT */
    Font *font;
    const char *text;
    size_t text_len;

    /* For CMD_TEXTURE */
    Texture *texture;
    Rect src_rect;       /* UV sub-rect */

    /* For rounded rects / styleboxes */
    float corner_radius;
    float border_width;

    /* For clip */
    int clip_index;         /* Nested clip tracking */
} DrawCmd;
```

### 7.2 DrawList API

```c
DrawList *draw_list_create(void);
void draw_list_clear(DrawList *dl);
void draw_list_destroy(DrawList *dl);

void draw_list_add_rect_filled(DrawList *dl, Rect r, Color c, float radius);
void draw_list_add_text(DrawList *dl, Font *f, Rect r, const char *text, Color c, uint32_t align);
void draw_list_add_texture(DrawList *dl, Texture *t, Rect dst, Rect src, Color modulate);
void draw_list_push_clip(DrawList *dl, Rect r);
void draw_list_pop_clip(DrawList *dl);
```

### 7.3 Rendering Rules

1. **Clip nesting**: The tree traversal pushes a clip rect equal to each Control's `global_rect` (intersected with parent's clip). Pops after children are drawn.
2. **Z-order**: Draw order is pre-order tree traversal. Later siblings draw on top of earlier siblings.
3. **Transparency**: The draw list does not sort by blend mode. The renderer backend should use premultiplied alpha blending for all UI passes.
4. **Backend contract**: The host engine must provide:
   - `Font` creation from a TTF file (or the UI library uses stb_truetype internally).
   - `Texture` creation from raw pixel data.
   - A function to submit a `DrawList` to the current frame.

---

## 8. Input & Events

### 8.1 Event Structure

```c
typedef enum {
    EV_MOUSE_MOTION,
    EV_MOUSE_BUTTON,
    EV_MOUSE_SCROLL,
    EV_KEY,
    EV_TEXT,
    EV_FOCUS_ENTER,
    EV_FOCUS_EXIT,
} EventType;

typedef struct {
    EventType type;
    uint32_t device_id;         /* For multi-touch future-proofing */

    /* Mouse / touch */
    Vec2 pos;                /* Screen-space coordinates */
    Vec2 delta;
    int button;                 /* 1=left, 2=right, 3=middle */
    int pressed;                /* 1=pressed, 0=released */
    int double_click;

    /* Keyboard */
    int keycode;                /* Platform-agnostic keycode enum */
    int physical_keycode;
    int unicode;                /* For EV_TEXT */
    int echo;
    int alt, shift, ctrl, meta;
} InputEvent;
```

### 8.2 Event Propagation

1. **Hit Test**: On mouse motion or button press, the tree performs a top-down hit test against `global_rect` of `Control` nodes. The topmost (last drawn) control under the cursor wins.
2. **Enter/Leave**: If the hit result changes between frames, `EV_MOUSE_LEAVE` is sent to the old node and `EV_MOUSE_ENTER` to the new node.
3. **Dispatch**: The event is sent to the target control's `handle_input` vtable.
4. **Bubbling**: If `handle_input` returns `0` (unhandled), the event is sent to `parent`. This continues until handled or the root is reached.
5. **Focus**: Keyboard events are sent to `tree->focused_control`. If unhandled, they bubble.

### 8.3 Mouse Filters

```c
#define MOUSE_FILTER_STOP    0  /* Consume event; do not bubble */
#define MOUSE_FILTER_PASS    1  /* Handle then bubble if unhandled */
#define MOUSE_FILTER_IGNORE  2  /* Do not receive; bubble immediately */
```

### 8.4 Focus

- `focus_mode`:
  - `FOCUS_NONE`: Cannot receive focus.
  - `FOCUS_CLICK`: Receives focus when clicked.
  - `FOCUS_ALL`: Receives focus on click or Tab navigation.
- Focus navigation (Tab / arrow keys) walks the tree in visual order (pre-order). The exact algorithm is implementation-defined in v1.0 but MUST be deterministic.

---

## 9. Signals

Signals are the primary decoupling mechanism between UI widgets and game logic.

### 9.1 C API

```c
typedef struct Signal Signal;
typedef void (*SignalCallback)(Node *emitter, void *userdata, const Variant *args, size_t arg_count);

Signal *signal_new(const char *name);
void signal_free(Signal *sig);

/* Connection */
int signal_connect(Signal *sig, SignalCallback cb, void *userdata, int flags);
void signal_disconnect(Signal *sig, int connection_id);

/* Emission */
void signal_emit(Signal *sig, Node *emitter, const Variant *args, size_t arg_count);
```

### 9.2 Connection Flags

```c
#define CONNECT_NORMAL   0  /* Persistent connection */
#define CONNECT_ONE_SHOT 1  /* Auto-disconnect after first fire */
#define CONNECT_DEFERRED 2  /* Queue for next process frame (not immediate) */
```

### 9.3 Variant Type

Signals pass a small number of arguments via a discriminated union:

```c
typedef enum {
    VAR_NIL,
    VAR_BOOL,
    VAR_INT,
    VAR_FLOAT,
    VAR_STRING,
    VAR_VEC2,
    VAR_RECT,
    VAR_NODE,    /* Increments refcount when stored */
} VariantType;

typedef struct {
    VariantType type;
    union {
        int b;
        int64_t i;
        double f;
        char *s;    /* Owned; freed by variant cleanup */
        Vec2 v2;
        Rect r;
        Node *n; /* Weak or strong depending on context */
    };
} Variant;
```

### 9.4 Python Bridging Requirement

The Cython wrapper MUST be able to connect a Python callable to any signal. The recommended implementation:
- A generic C callback `py_signal_bridge` is registered with `signal_connect`.
- `userdata` holds a `PyObject*` to the Python callable.
- `py_signal_bridge` converts `Variant` args to Python objects and invokes the callable.
- When the Python callable is garbage-collected, the connection MUST be disconnected to avoid dangling `userdata`.

---

## 10. Theming

### 10.1 Theme Resource

A `Theme` is a reference-counted key-value store.

```c
typedef struct Theme Theme;

Theme *theme_new(void);
void theme_set_color(Theme *t, const char *name, const char *type_class, Color c);
void theme_set_font(Theme *t, const char *name, const char *type_class, Font *f);
void theme_set_stylebox(Theme *t, const char *name, const char *type_class, StyleBox *sb);

Color theme_get_color(const Theme *t, const char *name, const char *type_class);
```

### 10.2 Theme Lookup Chain

When a `Control` needs a theme value (e.g., `font_color` for a `Label`):
1. Check the Control's own theme override (if any).
2. Check the Control's `theme` property (strong ref to a specific theme).
3. Walk up the parent chain, checking each ancestor's `theme`.
4. Fall back to a global default theme.

### 10.3 StyleBox

A `StyleBox` defines how to draw a rectangular background/border:

```c
typedef struct {
    Color bg_color;
    Color border_color;
    float border_width;
    float corner_radius;
    Texture *texture;        /* 9-slice texture; NULL for flat color */
    Rect texture_margins;    /* Left, top, right, bottom slice margins */
} StyleBox;
```

---

## 11. Widget Taxonomy (v1.0)

### 11.1 Base Widgets

| Widget | Inherits | Key Properties | Signals |
|--------|----------|----------------|---------|
| `Control` | `Node` | `rect`, `anchor`, `offset`, `min_size`, `size_flags_h/v`, `theme`, `mouse_filter`, `focus_mode` | — |
| `Container` | `Control` | — | `child_entered_tree`, `child_exited_tree` |
| `Label` | `Control` | `text`, `font`, `font_size`, `align_h`, `align_v`, `autowrap`, `clip_text` | — |
| `Button` | `Control` | `text`, `icon`, `toggle_mode`, `pressed` | `pressed`, `toggled`, `button_down`, `button_up` |
| `LineEdit` | `Control` | `text`, `placeholder`, `max_length`, `secret`, `editable` | `text_changed`, `text_submitted` |
| `TextureRect` | `Control` | `texture`, `stretch_mode`, `flip_h`, `flip_v` | — |
| `ColorRect` | `Control` | `color` | — |

### 11.2 Container Widgets

| Container | Layout Behavior |
|-----------|-----------------|
| `MarginContainer` | Single child; insets by `margin_left/top/right/bottom`. |
| `HBoxContainer` | Multiple children; horizontal flow. `separation` pixel gap. |
| `VBoxContainer` | Multiple children; vertical flow. `separation` pixel gap. |
| `CenterContainer` | Single child; centers it, optionally ignoring child's `min_size`. |
| `PanelContainer` | Single child; draws a `StyleBox` background, then lays out child inside inner rect. |

### 11.3 Future Widgets (v1.1+)

- `GridContainer`
- `ScrollContainer`
- `TabContainer`
- `Slider`
- `ProgressBar`
- `CheckBox`
- `OptionButton` (dropdown)

---

## 12. Memory Management

### 12.1 Reference Counting Rules

- Every object allocated by `*_new()` starts with `refcount == 1`.
- `node_ref(Node*)` increments.
- `node_unref(Node*)` decrements; if zero, calls `destroy` vtable and frees.
- **Parent-child**: Adding a child refs it; removing unrefs it. A child does NOT ref its parent (weak pointer).
- **Signals**: Connecting a callback does NOT ref the emitter or receiver. It is the caller's responsibility to disconnect before either is freed.
- **Theme**: A Control that sets a custom theme refs it. Clearing the theme unrefs it.

### 12.2 Ownership Summary

| Relationship | Owner | Pointer Type |
|--------------|-------|--------------|
| Parent → Child | Parent | Strong (refcounted) |
| Child → Parent | — | Weak (raw) |
| Control → Theme | Control | Strong (refcounted) |
| Signal → Connection | Signal | Strong (internal array) |
| Connection → Callback | — | Weak (function pointer) |
| Connection → Userdata | Caller | Depends on caller |

### 12.3 Python Interop

- Each Python wrapper object holds **exactly one strong reference** to the C node.
- When the Python object is garbage-collected, it calls `node_unref`.
- If C code frees a node while Python still holds it, the Python wrapper MUST become a "zombie" (all operations raise an exception). This is managed by invalidating the wrapper in `destroy` via a weak callback.

---

## 13. Python Binding Interface

### 13.1 Cython Wrapper Requirements

The Cython layer (`ui.pyx`) MUST expose:

1. **Classes**: Every `*` type has a corresponding Python class inheriting from a base `Node` class.
2. **Properties**: `rect`, `anchor`, `min_size`, etc. are Python properties reading/writing C structs.
3. **Method Overrides**: Python subclasses of `Node` MUST be able to override `process`, `draw`, `ready`, `handle_input`, and `get_minimum_size`.
4. **Signals**: Python callables can be connected to any signal with `obj.signal_name.connect(callable)`.
5. **Type Safety**: Python `isinstance` checks work against the C type hierarchy.

### 13.2 Virtual Method Override Mechanism

Because C vtables cannot point to Python functions directly, the Cython wrapper MUST:

1. Allocate a **per-instance vtable copy** (or use a trampoline) for any Python-subclassed node.
2. The trampoline checks a Python-side dict for an override. If found, it calls the Python method and returns. If not, it calls the default C implementation.
3. This adds one indirection per virtual call for Python instances, which is acceptable for UI code.

Alternative (simpler): All vtable slots for Python-wrapped types point to a generic trampoline that dispatches to `PyObject_CallMethod`.

### 13.3 Signal Bridging

```cython
# Pseudocode for Cython signal connection
cdef class Signal:
    cdef Signal *_ptr
    cdef dict _py_connections  # id -> PyObject*

    def connect(self, object callable):
        cdef int conn_id = signal_connect(
            self._ptr, 
            <SignalCallback>py_signal_bridge,
            <void*>callable,
            CONNECT_NORMAL
        )
        self._py_connections[conn_id] = callable
        return conn_id
```

### 13.4 Python API Example (Target UX)

```python
from sol.ui import Control, VBoxContainer, Button, Label, Anchor, SizeFlags

class MainMenu(Control):
    def _ready(self):
        vbox = VBoxContainer()
        vbox.anchor = Anchor.FULL_RECT
        self.add_child(vbox)

        title = Label(text="Sol Engine")
        title.size_flags_h = SizeFlags.SHRINK_CENTER
        vbox.add_child(title)

        start_btn = Button(text="Start Game")
        start_btn.on_pressed.connect(self._on_start)
        vbox.add_child(start_btn)

    def _on_start(self):
        self.emit_signal("start_game")
```

---

## 15. Threading & Concurrency

- The entire UI system is **single-threaded**.
- All API functions MUST be called from the thread that owns the `SceneTree`.
- The host engine may run physics or AI on other threads, but it MUST marshal UI mutations (e.g., updating a label) to the UI thread via a queue.
- Internal data structures (dynamic arrays, hash tables) do not need locks.

---

## 16. Error Handling

### 16.1 Strategy

C99 lacks exceptions. Use the following pattern:

1. **Programming errors** (NULL dereference, invalid enum value): Use `assert()` in debug builds. These are non-recoverable.
2. **Runtime errors** (out of memory, file not found): Return `NULL` or `0` and set a thread-local error string.
3. **Validation errors** (negative size, invalid anchor value): Clamp to valid range and optionally log a warning.

### 16.2 Error API

```c
const char *get_last_error(void);  /* Thread-local; NULL if no error */
void clear_error(void);
```

All public functions that can fail SHOULD document their error conditions.

---

## 17. Build Requirements

### 17.1 Compiler
- C99-compliant compiler (GCC, Clang, MSVC with `/std:c11` fallback).
- No C++ linkage required.

### 17.2 Dependencies
- **stb_truetype.h** + **stb_rect_pack.h** (single-header, public domain) for font atlas generation.
- **stb_image.h** (optional) for texture loading if the host engine does not provide it.
- **Python 3.9+** and **Cython 3.0+** for the Python wrapper.

### 17.3 No Dependencies On
- std::vector, std::string, or any C++ standard library.
- OS windowing APIs (SDL, GLFW, Win32) — the host engine provides input events.
- OpenGL, Vulkan, Metal, or DirectX — the host engine consumes draw lists.

---

## Appendix A: Data Structures Reference

### A.1 Math Types

```c
typedef struct { float x, y; } Vec2;
typedef struct { float x, y, w, h; } Rect;
typedef struct { float r, g, b, a; } Color;
```

### A.2 Helper Macros

```c
#define RECT_LEFT(r)   ((r).x)
#define RECT_RIGHT(r)  ((r).x + (r).w)
#define RECT_TOP(r)    ((r).y)
#define RECT_BOTTOM(r) ((r).y + (r).h)
#define RECT_CENTER(r) ((Vec2){(r).x + (r).w/2, (r).y + (r).h/2})
#define COLOR_RGBA(r,g,b,a) ((Color){(r)/255.0f,(g)/255.0f,(b)/255.0f,(a)/255.0f})
```

---

## Appendix B: Lifecycle Diagram

```
[Engine] calls scene_tree_add_child(root, new_node)
         │
         ▼
    [Enter Tree] ──pre-order──► new_node.enter_tree()
         │                          │
         │                          ▼
         │                     [children enter_tree]
         │                          │
         ▼                          ▼
      [Ready] ──post-order──► deepest_child.ready()
                                    │
                                    ▼
                              [parents ready]
                                    │
         ◄──────────────────────────┘
         │
    [Per Frame]
         │
    ┌────┴────┐
    ▼         ▼
 [Process]  [Draw]
 pre-order  pre-order
    │         │
    ▼         ▼
 [Layout]  [DrawList]
 (if dirty) submitted
    │         │
    ▼         ▼
 [Input]   [Renderer]
 top-down  consumes
    │       commands
    ▼
[Exit Tree] ◄── node_remove_child() or node_free()
 post-order
```

---

## Appendix C: Layout Algorithm Pseudocode

```
procedure SolveLayout(root):
    Measure(root)
    Arrange(root, root.rect)
    root.tree.layout_dirty = false

procedure Measure(node):
    if node.type == CONTAINER:
        for child in node.children:
            Measure(child)
        node.min_size = node.compute_min_size()
    else if node.type == CONTROL:
        if node.vtable.get_minimum_size:
            node.vtable.get_minimum_size(node, &node.min_size)
        else:
            node.min_size = node.explicit_min_size
    else:
        node.min_size = (0, 0)

procedure Arrange(node, parent_rect):
    if node.type == CONTROL:
        if node.parent.type == CONTAINER:
            /* Rect already set by parent container */
            pass
        else:
            node.rect = ComputeFromAnchors(node.anchor, node.offset, parent_rect)
        node.rect.w = max(node.rect.w, node.min_size.x)
        node.rect.h = max(node.rect.h, node.min_size.y)
        node.global_rect = ToGlobal(node.rect, node.parent)

    if node.type == CONTAINER:
        node.arranchildren()

    for child in node.children:
        Arrange(child, node.rect)

function ComputeFromAnchors(a, o, p):
    return Rect(
        x = p.x + p.w * a.left  + o.left,
        y = p.y + p.h * a.top   + o.top,
        w = p.w * (a.right - a.left)  + o.right  - o.left,
        h = p.h * (a.bottom - a.top)  + o.bottom - o.top
    )
```

---

## Appendix D: Naming Conventions

| Entity | Convention | Example |
|--------|------------|---------|
| Public types | `PascalCase` with `` prefix | `Control`, `Vec2` |
| Public functions | `snake_case` with `` prefix | `node_add_child` |
| Private functions | `snake_case` with `_` prefix | `_layout_measure` |
| Macros / constants | `UPPER_SNAKE_CASE` | `SIZE_EXPAND` |
| Struct members | `snake_case` | `min_size`, `child_count` |
| Enum values | `UPPER_SNAKE_CASE` with prefix | `EV_MOUSE_BUTTON` |
| File names | `snake_case` | `control.c` |

---

## Appendix E: Version History

| Version | Date | Author | Changes |
|---------|------|--------|---------|
| 1.0-draft | 2026-07-28 | Project Lead | Initial specification. |

---

*End of Specification*
