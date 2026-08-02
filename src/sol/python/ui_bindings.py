"""Python ctypes bindings for the Sol UI system.

Wraps the C99 retained-mode UI engine (Godot-inspired) at src/sol/scene/.
"""
import ctypes
from sol.bindings import _load as _load_engine

_lib = None


def _load():
    global _lib
    if _lib is not None:
        return _lib
    _lib = _load_engine()  # reuse the same libsol.so

    # Signal API
    _lib.node_add_signal.restype = ctypes.c_void_p
    _lib.node_add_signal.argtypes = [ctypes.c_void_p, ctypes.c_char_p]
    _lib.node_emit_signal.restype = None
    _lib.node_emit_signal.argtypes = [ctypes.c_void_p, ctypes.c_char_p,
                                       ctypes.c_void_p, ctypes.c_size_t]
    _lib.signal_connect.restype = ctypes.c_int
    _lib.signal_connect.argtypes = [ctypes.c_void_p, ctypes.c_void_p,
                                     ctypes.c_void_p, ctypes.c_int]
    _lib.signal_disconnect.restype = None
    _lib.signal_disconnect.argtypes = [ctypes.c_void_p, ctypes.c_int]

    # SceneTree input
    _lib.scene_tree_input.restype = None
    _lib.scene_tree_input.argtypes = [ctypes.c_void_p, ctypes.c_void_p]

    # Button
    _lib.button_new.restype = ctypes.c_void_p
    _lib.button_new.argtypes = []
    _lib.button_set_colors.restype = None
    _lib.button_set_colors.argtypes = [ctypes.c_void_p, Color, Color, Color]
    _lib.button_set_toggle_mode.restype = None
    _lib.button_set_toggle_mode.argtypes = [ctypes.c_void_p, ctypes.c_bool]
    _lib.button_set_toggled.restype = None
    _lib.button_set_toggled.argtypes = [ctypes.c_void_p, ctypes.c_bool]
    _lib.button_is_toggled.restype = ctypes.c_bool
    _lib.button_is_toggled.argtypes = [ctypes.c_void_p]

    # Label
    _lib.label_new.restype = ctypes.c_void_p
    _lib.label_new.argtypes = []
    _lib.label_set_text.restype = None
    _lib.label_set_text.argtypes = [ctypes.c_void_p, ctypes.c_char_p]
    _lib.label_set_font_size.restype = None
    _lib.label_set_font_size.argtypes = [ctypes.c_void_p, ctypes.c_float]
    _lib.label_set_align.restype = None
    _lib.label_set_align.argtypes = [ctypes.c_void_p, ctypes.c_uint32]
    _lib.label_set_font_color.restype = None
    _lib.label_set_font_color.argtypes = [ctypes.c_void_p, Color]

    # PanelContainer
    _lib.panel_container_new.restype = ctypes.c_void_p
    _lib.panel_container_new.argtypes = []

    # LineEdit
    _lib.line_edit_new.restype = ctypes.c_void_p
    _lib.line_edit_new.argtypes = []
    _lib.line_edit_set_text.restype = None
    _lib.line_edit_set_text.argtypes = [ctypes.c_void_p, ctypes.c_char_p]
    _lib.line_edit_get_text.restype = ctypes.c_char_p
    _lib.line_edit_get_text.argtypes = [ctypes.c_void_p]
    _lib.line_edit_set_placeholder.restype = None
    _lib.line_edit_set_placeholder.argtypes = [ctypes.c_void_p, ctypes.c_char_p]
    _lib.line_edit_set_max_length.restype = None
    _lib.line_edit_set_max_length.argtypes = [ctypes.c_void_p, ctypes.c_size_t]
    _lib.line_edit_set_secret.restype = None
    _lib.line_edit_set_secret.argtypes = [ctypes.c_void_p, ctypes.c_bool]
    _lib.line_edit_set_editable.restype = None
    _lib.line_edit_set_editable.argtypes = [ctypes.c_void_p, ctypes.c_bool]

    # Theme
    _lib.theme_create_default.restype = ctypes.c_void_p
    _lib.theme_create_default.argtypes = []

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
# Variant (for signal arguments)
# ---------------------------------------------------------------------------
VAR_NIL = 0
VAR_BOOL = 1
VAR_INT = 2
VAR_FLOAT = 3
VAR_STRING = 4
VAR_VEC2 = 5
VAR_RECT = 6
VAR_COLOR = 7


class Variant(ctypes.Structure):
    _fields_ = [
        ("type", ctypes.c_int),
        ("_b", ctypes.c_bool),
        ("_i", ctypes.c_int64),
        ("_f", ctypes.c_double),
        ("_s", ctypes.c_char_p),
        ("_v2", Vec2),
        ("_r", Rect),
        ("_c", Color),
    ]


# ---------------------------------------------------------------------------
# UIInputEvent
# ---------------------------------------------------------------------------
UI_EV_MOUSE_MOTION = 0
UI_EV_MOUSE_BUTTON = 1
UI_EV_MOUSE_SCROLL = 2
UI_EV_KEY = 3
UI_EV_TEXT = 4
UI_EV_FOCUS_ENTER = 5
UI_EV_FOCUS_EXIT = 6


class UIInputEvent(ctypes.Structure):
    _fields_ = [
        ("type", ctypes.c_int),
        ("pos", Vec2),
        ("delta", Vec2),
        ("button", ctypes.c_int),
        ("pressed", ctypes.c_bool),
        ("keycode", ctypes.c_int),
        ("unicode", ctypes.c_int),
        ("alt", ctypes.c_bool),
        ("shift", ctypes.c_bool),
        ("ctrl", ctypes.c_bool),
        ("meta", ctypes.c_bool),
    ]


# ---------------------------------------------------------------------------
# Signal callback type (ctypes function pointer)
# ---------------------------------------------------------------------------
# We use a module-level trampoline (not a closure) to avoid GC issues.
# Python callables are stored by integer ID; userdata carries the ID.
_signal_callbacks = {}   # id → callable
_signal_next_id = 0


def _signal_trampoline(emitter_ptr, args_ptr, arg_count, userdata):
    """C-callable trampoline. userdata is an integer ID into _signal_callbacks."""
    cb = _signal_callbacks.get(userdata)
    if cb:
        cb()


SIGNAL_CALLBACK = ctypes.CFUNCTYPE(
    None,
    ctypes.c_void_p, ctypes.c_void_p, ctypes.c_size_t,
    ctypes.c_void_p,              # userdata as void* (carries int ID)
)

_c_signal_trampoline = SIGNAL_CALLBACK(_signal_trampoline)


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
# HBoxContainer
# ---------------------------------------------------------------------------
_hbox_container_new = _load().hbox_container_new
_hbox_container_new.restype = ctypes.c_void_p
_hbox_container_new.argtypes = []

_hbox_container_set_separation = _load().hbox_container_set_separation
_hbox_container_set_separation.restype = None
_hbox_container_set_separation.argtypes = [ctypes.c_void_p, ctypes.c_float]


# ---------------------------------------------------------------------------
# MarginContainer
# ---------------------------------------------------------------------------
_margin_container_new = _load().margin_container_new
_margin_container_new.restype = ctypes.c_void_p
_margin_container_new.argtypes = []

_margin_container_set_margin = _load().margin_container_set_margin
_margin_container_set_margin.restype = None
_margin_container_set_margin.argtypes = [
    ctypes.c_void_p, ctypes.c_float, ctypes.c_float,
    ctypes.c_float, ctypes.c_float
]


# ---------------------------------------------------------------------------
# CenterContainer
# ---------------------------------------------------------------------------
_center_container_new = _load().center_container_new
_center_container_new.restype = ctypes.c_void_p
_center_container_new.argtypes = []


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
_hbox_container_class = _get_vtable("hbox_container_class")
_margin_container_class = _get_vtable("margin_container_class")
_center_container_class = _get_vtable("center_container_class")
_button_class = _get_vtable("button_class")
_label_class = _get_vtable("label_class")
_panel_container_class = _get_vtable("panel_container_class")
_line_edit_class = _get_vtable("line_edit_class")

# ---------------------------------------------------------------------------
# High-level Python wrappers
# ---------------------------------------------------------------------------


class Signal:
    """A signal on a Node. Created via node.add_signal() or node.get_signal()."""
    __slots__ = ("_ptr",)

    def __init__(self, ptr):
        self._ptr = ptr

    def connect(self, callback):
        """Connect a Python callable. Returns an integer connection ID.
        The callback receives no arguments for now."""
        global _signal_next_id
        _signal_next_id += 1
        ref_id = _signal_next_id
        _signal_callbacks[ref_id] = callback

        conn_id = _load().signal_connect(
            self._ptr, _c_signal_trampoline, ref_id, 0)
        return conn_id

    def disconnect(self, conn_id: int):
        """Disconnect a previously connected callback."""
        _load().signal_disconnect(self._ptr, conn_id)


class Node:
    """Python wrapper for a C Node pointer."""

    __slots__ = ("_ptr", "_owned")

    def __init__(self, ptr, owned=True):
        self._ptr = ptr
        self._owned = owned

    @property
    def ptr(self):
        return self._ptr

    def add_child(self, child: "Node"):
        _node_add_child(self._ptr, child._ptr)

    def set_name(self, name: str):
        _node_set_name(self._ptr, name.encode("utf-8"))

    def add_signal(self, name: str) -> Signal:
        """Register a signal on this node. Returns a Signal object."""
        sig_ptr = _load().node_add_signal(self._ptr, name.encode("utf-8"))
        return Signal(sig_ptr)

    def get_signal(self, name: str) -> Signal | None:
        """Get a Signal by name. Returns None if not found."""
        sig_ptr = _load().node_add_signal(self._ptr, name.encode("utf-8"))
        if sig_ptr:
            return Signal(sig_ptr)
        return None

    def emit_signal(self, name: str):
        """Emit a signal by name. All connections are called immediately."""
        _load().node_emit_signal(self._ptr, name.encode("utf-8"), None, 0)

    def __del__(self):
        if self._ptr and self._owned:
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


class HBoxContainer(Control):
    """Horizontal box container."""

    def __init__(self):
        ptr = _hbox_container_new()
        Node.__init__(self, ptr)

    def set_separation(self, px: float):
        _hbox_container_set_separation(self._ptr, px)


class MarginContainer(Control):
    """Container that adds padding/margin around a single child."""

    def __init__(self):
        ptr = _margin_container_new()
        Node.__init__(self, ptr)

    def set_margin(self, left: float, top: float, right: float, bottom: float):
        _margin_container_set_margin(self._ptr, left, top, right, bottom)


class CenterContainer(Control):
    """Container that centers its child."""

    def __init__(self):
        ptr = _center_container_new()
        Node.__init__(self, ptr)


class Button(Control):
    """A clickable button with signals: pressed, toggled, button_down, button_up."""

    def __init__(self):
        ptr = _load().button_new()
        Node.__init__(self, ptr)
        # Signal handles cached for convenience
        self.on_pressed = self.get_signal("pressed")
        self.on_toggled = self.get_signal("toggled")
        self.on_button_down = self.get_signal("button_down")
        self.on_button_up = self.get_signal("button_up")

    def set_colors(self, normal: Color, hover: Color, pressed: Color):
        _load().button_set_colors(self._ptr, normal, hover, pressed)

    def set_toggle_mode(self, enabled: bool):
        _load().button_set_toggle_mode(self._ptr, enabled)

    def set_toggled(self, toggled: bool):
        _load().button_set_toggled(self._ptr, toggled)

    def is_toggled(self) -> bool:
        return _load().button_is_toggled(self._ptr)


class Label(Control):
    """A text label widget.
    
    Font rendering requires stb_truetype (Phase 3). For now, labels
    emit DRAW_CMD_TEXT commands that the renderer backend handles.
    """

    # Alignment constants
    ALIGN_LEFT   = 0
    ALIGN_CENTER = 1
    ALIGN_RIGHT  = 2
    ALIGN_TOP    = 0
    ALIGN_MIDDLE = 4
    ALIGN_BOTTOM = 8

    def __init__(self, text: str = ""):
        ptr = _load().label_new()
        Node.__init__(self, ptr)
        if text:
            self.set_text(text)

    def set_text(self, text: str):
        _load().label_set_text(self._ptr, text.encode("utf-8"))

    def set_font_size(self, size: float):
        _load().label_set_font_size(self._ptr, size)

    def set_align(self, align: int):
        _load().label_set_align(self._ptr, align)

    def set_font_color(self, r: float, g: float, b: float, a: float = 1.0):
        c = Color(r, g, b, a)
        _load().label_set_font_color(self._ptr, c)


class PanelContainer(Control):
    """Container that draws a StyleBox background and arranges its
    single child inside the StyleBox's inner area."""

    def __init__(self):
        ptr = _load().panel_container_new()
        Node.__init__(self, ptr)


class LineEdit(Control):
    """Single-line text input field.

    Signals: text_changed, text_submitted
    """

    def __init__(self, text: str = ""):
        ptr = _load().line_edit_new()
        Node.__init__(self, ptr)
        self.on_text_changed = self.get_signal("text_changed")
        self.on_text_submitted = self.get_signal("text_submitted")
        if text:
            self.set_text(text)

    def set_text(self, text: str):
        _load().line_edit_set_text(self._ptr, text.encode("utf-8"))

    def get_text(self) -> str:
        result = _load().line_edit_get_text(self._ptr)
        return result.decode("utf-8") if result else ""

    def set_placeholder(self, text: str):
        _load().line_edit_set_placeholder(self._ptr, text.encode("utf-8"))

    def set_max_length(self, max_len: int):
        _load().line_edit_set_max_length(self._ptr, max_len)

    def set_secret(self, secret: bool):
        _load().line_edit_set_secret(self._ptr, secret)

    def set_editable(self, editable: bool):
        _load().line_edit_set_editable(self._ptr, editable)


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

    def input(self, ev: UIInputEvent):
        """Route an input event through the UI tree."""
        _load().scene_tree_input(self._ptr, ctypes.byref(ev))

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
