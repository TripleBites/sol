"""Python ctypes bindings for the Sol UI system.

Wraps the C99 retained-mode UI engine (Godot-inspired) at src/engine/ui/.
"""
import ctypes
from sol.bindings import _load as _load_engine

_lib = None


def _load():
    global _lib
    if _lib is not None:
        return _lib
    _lib = _load_engine()  # reuse the same libsol.so
    return _lib


# ---------------------------------------------------------------------------
# Math types
# ---------------------------------------------------------------------------
class Vec2(ctypes.Structure):
    _fields_ = [("x", ctypes.c_float), ("y", ctypes.c_float)]


class Rect(ctypes.Structure):
    _fields_ = [
        ("x", ctypes.c_float),
        ("y", ctypes.c_float),
        ("w", ctypes.c_float),
        ("h", ctypes.c_float),
    ]


class Color(ctypes.Structure):
    _fields_ = [
        ("r", ctypes.c_float),
        ("g", ctypes.c_float),
        ("b", ctypes.c_float),
        ("a", ctypes.c_float),
    ]


# ---------------------------------------------------------------------------
# Node
# ---------------------------------------------------------------------------
_node_new = _load().node_new
_node_new.restype = ctypes.c_void_p
_node_new.argtypes = [ctypes.c_void_p]

_node_ref = _load().node_ref
_node_ref.restype = ctypes.c_void_p
_node_ref.argtypes = [ctypes.c_void_p]

_node_unref = _load().node_unref
_node_unref.restype = None
_node_unref.argtypes = [ctypes.c_void_p]

_node_add_child = _load().node_add_child
_node_add_child.restype = None
_node_add_child.argtypes = [ctypes.c_void_p, ctypes.c_void_p]

_node_set_name = _load().node_set_name
_node_set_name.restype = None
_node_set_name.argtypes = [ctypes.c_void_p, ctypes.c_char_p]


# ---------------------------------------------------------------------------
# Control
# ---------------------------------------------------------------------------
_control_new = _load().control_new
_control_new.restype = ctypes.c_void_p
_control_new.argtypes = [ctypes.c_void_p]

_control_set_anchor = _load().control_set_anchor
_control_set_anchor.restype = None
_control_set_anchor.argtypes = [
    ctypes.c_void_p,
    ctypes.c_float,
    ctypes.c_float,
    ctypes.c_float,
    ctypes.c_float,
]

_control_set_offset = _load().control_set_offset
_control_set_offset.restype = None
_control_set_offset.argtypes = [
    ctypes.c_void_p,
    ctypes.c_float,
    ctypes.c_float,
    ctypes.c_float,
    ctypes.c_float,
]

_control_set_min_size = _load().control_set_min_size
_control_set_min_size.restype = None
_control_set_min_size.argtypes = [ctypes.c_void_p, ctypes.c_float, ctypes.c_float]

_control_set_size_flags = _load().control_set_size_flags
_control_set_size_flags.restype = None
_control_set_size_flags.argtypes = [ctypes.c_void_p, ctypes.c_uint32, ctypes.c_uint32]

# Size flag constants
SIZE_FILL = 1
SIZE_EXPAND = 2
SIZE_SHRINK_BEGIN = 4
SIZE_SHRINK_CENTER = 8
SIZE_SHRINK_END = 16


# ---------------------------------------------------------------------------
# ColorRect
# ---------------------------------------------------------------------------
_color_rect_new = _load().color_rect_new
_color_rect_new.restype = ctypes.c_void_p
_color_rect_new.argtypes = []

_color_rect_set_color = _load().color_rect_set_color
_color_rect_set_color.restype = None
_color_rect_set_color.argtypes = [ctypes.c_void_p, Color]


# ---------------------------------------------------------------------------
# VBoxContainer
# ---------------------------------------------------------------------------
_vbox_container_new = _load().vbox_container_new
_vbox_container_new.restype = ctypes.c_void_p
_vbox_container_new.argtypes = []

_vbox_container_set_separation = _load().vbox_container_set_separation
_vbox_container_set_separation.restype = None
_vbox_container_set_separation.argtypes = [ctypes.c_void_p, ctypes.c_float]


# ---------------------------------------------------------------------------
# SceneTree
# ---------------------------------------------------------------------------
_scene_tree_create = _load().scene_tree_create
_scene_tree_create.restype = ctypes.c_void_p
_scene_tree_create.argtypes = []

_scene_tree_destroy = _load().scene_tree_destroy
_scene_tree_destroy.restype = None
_scene_tree_destroy.argtypes = [ctypes.c_void_p]

_scene_tree_set_root = _load().scene_tree_set_root
_scene_tree_set_root.restype = None
_scene_tree_set_root.argtypes = [ctypes.c_void_p, ctypes.c_void_p]

_scene_tree_process = _load().scene_tree_process
_scene_tree_process.restype = None
_scene_tree_process.argtypes = [ctypes.c_void_p, ctypes.c_float]

_scene_tree_get_draw_list = _load().scene_tree_get_draw_list
_scene_tree_get_draw_list.restype = ctypes.c_void_p
_scene_tree_get_draw_list.argtypes = [ctypes.c_void_p]

_scene_tree_print = _load().scene_tree_print
_scene_tree_print.restype = None
_scene_tree_print.argtypes = [ctypes.c_void_p]


# ---------------------------------------------------------------------------
# DrawList
# ---------------------------------------------------------------------------
class DrawCmd(ctypes.Structure):
    _fields_ = [
        ("type", ctypes.c_int),
        ("rect", Rect),
        ("color", Color),
        ("corner_radius", ctypes.c_float),
        ("border_width", ctypes.c_float),
        ("text", ctypes.c_char_p),
        ("text_len", ctypes.c_size_t),
        ("align", ctypes.c_uint32),
        ("clip_index", ctypes.c_int),
    ]


DRAW_CMD_RECT_FILLED = 0
DRAW_CMD_RECT_BORDER = 1
DRAW_CMD_TEXT = 2
DRAW_CMD_TEXTURE = 3
DRAW_CMD_CLIP_PUSH = 4
DRAW_CMD_CLIP_POP = 5

_DRAW_CMD_NAMES = {
    0: "RECT_FILLED",
    1: "RECT_BORDER",
    2: "TEXT",
    3: "TEXTURE",
    4: "CLIP_PUSH",
    5: "CLIP_POP",
}

_draw_list_cmd_count = _load().draw_list_cmd_count
_draw_list_cmd_count.restype = ctypes.c_size_t
_draw_list_cmd_count.argtypes = [ctypes.c_void_p]

_draw_list_get_cmd = _load().draw_list_get_cmd
_draw_list_get_cmd.restype = DrawCmd
_draw_list_get_cmd.argtypes = [ctypes.c_void_p, ctypes.c_size_t]


# ---------------------------------------------------------------------------
# VTable pointers (needed for node_new / control_new)
# ---------------------------------------------------------------------------
# The vtables are global const symbols. We can look them up by name.
def _get_vtable(name):
    """Get a vtable pointer by symbol name."""
    try:
        addr = ctypes.cast(getattr(_load(), name), ctypes.c_void_p)
        return addr
    except AttributeError:
        raise RuntimeError(f"VTable symbol '{name}' not found in libsol.so")


# Pre-load common vtables
_control_class = _get_vtable("control_class")
_color_rect_class = _get_vtable("color_rect_class")
_vbox_container_class = _get_vtable("vbox_container_class")

# ---------------------------------------------------------------------------
# High-level Python wrappers
# ---------------------------------------------------------------------------


class Node:
    """Python wrapper for a C Node pointer."""

    __slots__ = ("_ptr",)

    def __init__(self, ptr):
        self._ptr = ptr

    @property
    def ptr(self):
        return self._ptr

    def add_child(self, child: "Node"):
        _node_add_child(self._ptr, child._ptr)

    def set_name(self, name: str):
        _node_set_name(self._ptr, name.encode("utf-8"))

    def __del__(self):
        if self._ptr:
            _node_unref(self._ptr)
            self._ptr = None


class Control(Node):
    """Python wrapper for a Control."""

    def __init__(self):
        ptr = _control_new(_control_class)
        super().__init__(ptr)

    def set_anchor(self, left: float, top: float, right: float, bottom: float):
        _control_set_anchor(self._ptr, left, top, right, bottom)

    def set_margin(self, left: float, top: float, right: float, bottom: float):
        """Set offset margins (pixels)."""
        _control_set_offset(self._ptr, left, top, right, bottom)

    def set_min_size(self, w: float, h: float):
        _control_set_min_size(self._ptr, w, h)

    def set_size_flags(self, h: int, v: int):
        _control_set_size_flags(self._ptr, h, v)


class ColorRect(Control):
    """A colored rectangle widget."""

    def __init__(self):
        ptr = _color_rect_new()
        # Don't call super().__init__() — we already have a ptr
        Node.__init__(self, ptr)

    def set_color(self, r: float, g: float, b: float, a: float = 1.0):
        c = Color(r, g, b, a)
        _color_rect_set_color(self._ptr, c)


class VBoxContainer(Control):
    """Vertical box container."""

    def __init__(self):
        ptr = _vbox_container_new()
        Node.__init__(self, ptr)

    def set_separation(self, px: float):
        _vbox_container_set_separation(self._ptr, px)


class SceneTree:
    """Orchestrates the UI frame loop."""

    def __init__(self):
        self._ptr = _scene_tree_create()

    @property
    def ptr(self):
        return self._ptr

    def set_root(self, node: Node):
        _scene_tree_set_root(self._ptr, node._ptr)

    def process(self, delta: float = 0.016):
        """Run one frame: process → layout → draw."""
        _scene_tree_process(self._ptr, delta)

    def get_draw_list(self):
        """Get the raw DrawList pointer (for inspection)."""
        return _scene_tree_get_draw_list(self._ptr)

    def print_commands(self):
        """Print all draw commands (debug)."""
        dl = self.get_draw_list()
        n = _draw_list_cmd_count(dl)
        print(f"Draw list ({n} commands):")
        for i in range(n):
            cmd = _draw_list_get_cmd(dl, i)
            name = _DRAW_CMD_NAMES.get(cmd.type, "???")
            if cmd.type in (DRAW_CMD_CLIP_PUSH, DRAW_CMD_CLIP_POP):
                print(
                    f"  [{i}] {name} "
                    f"rect={{{cmd.rect.x:.0f},{cmd.rect.y:.0f},"
                    f"{cmd.rect.w:.0f},{cmd.rect.h:.0f}}}"
                )
            elif cmd.type == DRAW_CMD_RECT_FILLED:
                print(
                    f"  [{i}] {name} "
                    f"rect={{{cmd.rect.x:.0f},{cmd.rect.y:.0f},"
                    f"{cmd.rect.w:.0f},{cmd.rect.h:.0f}}} "
                    f"color={{{cmd.color.r:.2f},{cmd.color.g:.2f},"
                    f"{cmd.color.b:.2f},{cmd.color.a:.2f}}}"
                )
            else:
                print(f"  [{i}] {name}")

    def __del__(self):
        if self._ptr:
            _scene_tree_destroy(self._ptr)
            self._ptr = None
