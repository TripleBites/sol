# Sol Engine — Architecture Specification

**Version:** 2.0  
**Language:** C99 (engine), Python 3.10+ (bindings/host)  
**Graphics:** SDL3 + Vulkan 1.0+  

---

## 1. Layered Architecture

```
┌──────────────────────────────────────────────┐
│  Python Game Code (game/main.py)            │
├──────────────────────────────────────────────┤
│  sol package (src/sol/)                      │
│  ├── bindings.py   ctypes → libsol.so       │
│  ├── ui_bindings.py  ctypes → UI system     │
│  └── sol.py        public API               │
├──────────────────────────────────────────────┤
│  libsol.so  (C99 shared library)             │
│                                              │
│  ┌─ engine.c         dispatch               │
│  ├─ io/                               │
│  │  ├─ io_sdl3.c    SDL window/input  │
│  │  ├─ io_vulkan.c  Vulkan renderer   │
│  │  └─ io_headless.c                  │
│  └─ ui/                 UI system           │
│     ├─ node.c            base object        │
│     ├─ control.c         layout             │
│     ├─ draw_list.c       draw commands      │
│     ├─ scene_tree.c      frame orchestrator │
│     ├─ color_rect.c      widget             │
│     └─ vbox_container.c  container          │
└──────────────────────────────────────────────┘
```

## 2. Engine Layer (`src/engine/`)

### 2.1 Public API (`engine.h`)

```c
bool sol_init(const char* title, int width, int height);
bool sol_update(void);          // returns false when window should close
void sol_shutdown(void);
void sol_get_size(int* w, int* h);
```

### 2.2 Platform Abstraction (`io/io.h`)

Each platform backend implements the `SolPlatform` struct:

```c
typedef struct SolPlatform {
    bool (*init)(const char* title, int width, int height);
    void (*shutdown)(void);
    bool (*update)(void);
    void (*get_size)(int* w, int* h);
} SolPlatform;
```

Registered via `sol_io_sdl3()` / `sol_io_headless()`.

### 2.3 Vulkan Renderer (`io/io_vulkan.h`)

The Vulkan backend is a standalone module that the SDL3 platform hands off to after creating the instance, surface, and device:

```c
bool sol_vulkan_init(SolVulkan* vk, VkInstance, VkPhysicalDevice, VkDevice,
                     VkSurfaceKHR, uint32_t queue_family, VkQueue, int w, int h);
bool sol_vulkan_frame(SolVulkan* vk);       // acquire → draw → submit → present
void sol_vulkan_signal_resize(SolVulkan* vk);
void sol_vulkan_shutdown(SolVulkan* vk);
```

This separation allows swapping in alternative renderers (OpenGL, Metal) behind the same io.

### 2.4 SDL3 Platform (`io/io_sdl3.c`)

Responsibilities:
- SDL window creation with `SDL_WINDOW_VULKAN`
- Vulkan instance creation (extensions from `SDL_Vulkan_GetInstanceExtensions`)
- Surface creation via `SDL_Vulkan_CreateSurface`
- Physical device selection + logical device creation
- Event processing (quit, resize → forwarded to Vulkan module)
- Delegates all rendering to `SolVulkan`

## 3. UI System (`src/engine/ui/`)

### 3.1 Design

Godot-inspired retained-mode UI. The system builds a tree of `Node` objects, runs a two-pass layout solver, and produces a `DrawList` of renderer-agnostic commands.

### 3.2 Object Model

C99 has no inheritance. We use **manual vtables** (`NodeClass`) with struct embedding:

```
NodeClass (vtable)
  ├── type_name, instance_size
  ├── init / destroy / enter_tree / exit_tree / ready
  ├── process / draw
  ├── get_minimum_size / arrange_children
  └── handle_input

Node (base struct)
  ├── klass*, parent*, children[]
  ├── name, flags, refcount
  └── user_data

Control extends Node
  ├── rect, global_rect
  ├── anchor, offset
  ├── min_size, explicit_min_size
  ├── size_flags_h, size_flags_v
  └── theme, mouse_filter, focus_mode
```

### 3.3 SceneTree Frame Loop

```
scene_tree_process(delta)
  ├── process_pass(root, delta)       // pre-order
  ├── if layout_dirty:
  │     ├── measure_pass(root)        // post-order: children → parent
  │     └── arrange_pass(root, rect)   // pre-order: parent → children
  └── draw_pass(root, draw_list)       // pre-order → DrawList
```

### 3.4 Two-Pass Layout

**Measure (bottom-up):** Container min_size is computed from children.

**Arrange (top-down):** Containers set child rects directly. Non-container Controls compute rect from anchors + offsets.

### 3.5 DrawList

The `DrawList` is an array of `DrawCmd`:
- `DRAW_CMD_RECT_FILLED` — filled rectangle
- `DRAW_CMD_RECT_BORDER` — bordered rectangle  
- `DRAW_CMD_TEXT` — text string
- `DRAW_CMD_TEXTURE` — textured quad
- `DRAW_CMD_CLIP_PUSH/POP` — scissor rect nesting

The renderer backend is responsible for consuming these commands.

## 4. Python Bindings (`src/sol/`)

### 4.1 Engine Bindings (`bindings.py`)

Loads `libsol.so` via `ctypes.CDLL`, configures function signatures, and exposes:

```python
sol.init(title, width, height) → bool
sol.update() → bool
sol.shutdown()
sol.get_size() → (int, int)
```

Library search path includes the package directory and common build output locations.

### 4.2 UI Bindings (`ui_bindings.py`)

Wraps the C UI system with Python classes:

```python
tree = SceneTree()
root = Control()
sky = ColorRect()        # colored rectangle
panel = VBoxContainer()  # vertical layout

panel.add_child(header)
panel.add_child(body)    # SIZE_EXPAND | SIZE_FILL
panel.add_child(footer)

tree.set_root(root)
tree.process(0.016)      # run one frame
tree.print_commands()    # inspect DrawList
```

## 5. Build System

- **`scripts/build_engine.py`** — assembles SPIR-V shaders via `spirv-as`, compiles all `.c` → `libsol.so`
- **`setup.py`** — setuptools integration (custom `build_ext` calls the build script)
- **`pyproject.toml`** — project metadata for `uv`
- **`buildozer.spec`** — Android APK packaging
