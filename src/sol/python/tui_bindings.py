"""Python ctypes bindings for the Sol TUI debug renderer.

Enables headless UI testing by rendering a SceneTree's DrawList
to a terminal grid. Designed for LLM-driven development — the LLM
can read the rendered grid programmatically to verify UI layout
without a GPU.

Usage:
    from sol.tui_bindings import TuiGrid, render_scene_tree

    tree = SceneTree()
    tree.set_root(my_panel.get_root())
    tree.process(0.016)

    grid = render_scene_tree(tree)
    print(grid.to_string())  # human-readable ANSI output
    print(grid.cells)         # programmatic: list of (ch, fg, bg) tuples
"""
import ctypes
from sol.bindings import _load as _load_engine

_lib = None


def _load():
    global _lib
    if _lib is not None:
        return _lib
    _lib = _load_engine()

    # --- TUI Grid ---
    _lib.tui_grid_create.restype = ctypes.c_void_p
    _lib.tui_grid_create.argtypes = [ctypes.c_int, ctypes.c_int]

    _lib.tui_grid_free.restype = None
    _lib.tui_grid_free.argtypes = [ctypes.c_void_p]

    _lib.tui_grid_clear.restype = None
    _lib.tui_grid_clear.argtypes = [ctypes.c_void_p]

    _lib.tui_render_to_grid.restype = None
    _lib.tui_render_to_grid.argtypes = [ctypes.c_void_p, ctypes.c_void_p]

    _lib.tui_render_to_grid_scaled.restype = None
    _lib.tui_render_to_grid_scaled.argtypes = [
        ctypes.c_void_p, ctypes.c_void_p,
        ctypes.c_float, ctypes.c_float,
    ]

    _lib.tui_render_ansi.restype = ctypes.c_void_p
    _lib.tui_render_ansi.argtypes = [ctypes.c_void_p, ctypes.c_int, ctypes.c_int]

    _lib.tui_cell_to_ansi.restype = None
    _lib.tui_cell_to_ansi.argtypes = [ctypes.c_void_p, ctypes.c_char_p, ctypes.c_size_t]

    # --- TUI Input ---
    _lib.tui_input_init.restype = ctypes.c_bool
    _lib.tui_input_init.argtypes = []

    _lib.tui_input_shutdown.restype = None
    _lib.tui_input_shutdown.argtypes = []

    _lib.tui_input_poll.restype = TuiKeyEvent
    _lib.tui_input_poll.argtypes = []

    _lib.tui_mouse_init.restype = ctypes.c_bool
    _lib.tui_mouse_init.argtypes = []

    _lib.tui_mouse_shutdown.restype = None
    _lib.tui_mouse_shutdown.argtypes = []

    _lib.tui_mouse_poll.restype = TuiMouseEvent
    _lib.tui_mouse_poll.argtypes = []

    # --- TUI Platform ---
    _lib.sol_io_tui.restype = ctypes.c_void_p
    _lib.sol_io_tui.argtypes = []

    return _lib


# ---------------------------------------------------------------------------
# C struct types
# ---------------------------------------------------------------------------
class TuiCell(ctypes.Structure):
    _fields_ = [
        ("ch",   ctypes.c_uint32),
        ("fg_r", ctypes.c_uint8),
        ("fg_g", ctypes.c_uint8),
        ("fg_b", ctypes.c_uint8),
        ("bg_r", ctypes.c_uint8),
        ("bg_g", ctypes.c_uint8),
        ("bg_b", ctypes.c_uint8),
        ("bold", ctypes.c_uint8),   # C _Bool is 1 byte, use uint8 for safety
        ("dim",  ctypes.c_uint8),
    ]


class TuiKeyEvent(ctypes.Structure):
    _fields_ = [
        ("keycode",    ctypes.c_int),
        ("is_special", ctypes.c_bool),
        ("pressed",    ctypes.c_bool),
    ]


class TuiMouseEvent(ctypes.Structure):
    _fields_ = [
        ("x",       ctypes.c_int),
        ("y",       ctypes.c_int),
        ("button",  ctypes.c_int),
        ("pressed", ctypes.c_bool),
        ("moved",   ctypes.c_bool),
    ]


# ---------------------------------------------------------------------------
# Special key constants
# ---------------------------------------------------------------------------
KEY_UP        = 0x110000
KEY_DOWN      = 0x110001
KEY_LEFT      = 0x110002
KEY_RIGHT     = 0x110003
KEY_ESCAPE    = 0x110004
KEY_ENTER     = 0x110005
KEY_TAB       = 0x110006
KEY_BACKSPACE = 0x110007
KEY_SPACE     = 0x110008

_SPECIAL_NAMES = {
    KEY_UP:        "UP",
    KEY_DOWN:      "DOWN",
    KEY_LEFT:      "LEFT",
    KEY_RIGHT:     "RIGHT",
    KEY_ESCAPE:    "ESC",
    KEY_ENTER:     "ENTER",
    KEY_TAB:       "TAB",
    KEY_BACKSPACE: "BACKSPACE",
    KEY_SPACE:     "SPACE",
}


# ---------------------------------------------------------------------------
# High-level Python TuiGrid
# ---------------------------------------------------------------------------
class TuiGrid:
    """A 2D grid of TuiCell for terminal rendering.

    Wraps the C TuiGrid. Can be created from a SceneTree's DrawList
    or directly for custom rendering.
    """

    def __init__(self, cols: int = 80, rows: int = 40, ptr=None):
        if ptr:
            self._ptr = ptr
            self._owned = False
        else:
            self._ptr = _load().tui_grid_create(cols, rows)
            self._owned = True
        self._cols = cols
        self._rows = rows
        # Read the cells pointer from the C struct (first field)
        self._cells_ptr = ctypes.cast(self._ptr, ctypes.POINTER(ctypes.c_void_p))[0]
        if not self._cells_ptr:
            self._cells_ptr = 0

    @property
    def ptr(self):
        return self._ptr

    @property
    def cols(self) -> int:
        return self._cols

    @property
    def rows(self) -> int:
        return self._rows

    def clear(self):
        """Clear all cells."""
        _load().tui_grid_clear(self._ptr)

    def cell(self, x: int, y: int) -> dict | None:
        """Get cell at (x, y) as a dict. Returns None if out of bounds."""
        if x < 0 or x >= self._cols or y < 0 or y >= self._rows:
            return None
        if not self._cells_ptr:
            return None
        offset = (y * self._cols + x) * ctypes.sizeof(TuiCell)
        c = TuiCell.from_address(self._cells_ptr + offset)
        return self._cell_to_dict(c)

    @staticmethod
    def _cell_to_dict(c: TuiCell) -> dict:
        """Convert a TuiCell to a readable dict."""
        ch_code = c.ch
        if ch_code == 0:
            ch_str = " "
        elif ch_code < 0x80:
            ch_str = chr(ch_code)
        elif ch_code < 0x800:
            ch_str = chr(0xC0 | (ch_code >> 6)) + chr(0x80 | (ch_code & 0x3F))
        elif ch_code < 0x10000:
            ch_str = (chr(0xE0 | (ch_code >> 12))
                      + chr(0x80 | ((ch_code >> 6) & 0x3F))
                      + chr(0x80 | (ch_code & 0x3F)))
        else:
            ch_str = "?"
        return {
            "char": ch_str,
            "codepoint": ch_code,
            "fg": (c.fg_r, c.fg_g, c.fg_b),
            "bg": (c.bg_r, c.bg_g, c.bg_b),
            "bold": bool(c.bold),
            "dim": bool(c.dim),
        }

    def to_dicts(self) -> list[list[dict | None]]:
        """Convert the entire grid to a 2D list of cell dicts.

        This is the primary API for LLM programmatic inspection.
        Empty cells are None.
        """
        result = []
        if not self._cells_ptr:
            return result
        size = ctypes.sizeof(TuiCell)
        base = self._cells_ptr
        for y in range(self._rows):
            row = []
            for x in range(self._cols):
                c = TuiCell.from_address(base + (y * self._cols + x) * size)
                if c.ch == 0:
                    row.append(None)
                else:
                    row.append(self._cell_to_dict(c))
            result.append(row)
        return result

    def to_compact_string(self) -> str:
        """Render the grid as a compact ASCII representation.

        Non-empty cells show their char; empty cells show space.
        No ANSI escapes — suitable for LLM context windows.
        """
        if not self._cells_ptr:
            return ""
        lines = []
        size = ctypes.sizeof(TuiCell)
        base = self._cells_ptr
        for y in range(self._rows):
            line = ""
            for x in range(self._cols):
                c = TuiCell.from_address(base + (y * self._cols + x) * size)
                if c.ch == 0:
                    line += " "
                elif c.ch <= 0x7F:
                    line += chr(c.ch)
                elif c.ch < 0x800:
                    line += chr(0xC0 | (c.ch >> 6)) + chr(0x80 | (c.ch & 0x3F))
                elif c.ch < 0x10000:
                    line += (chr(0xE0 | (c.ch >> 12))
                             + chr(0x80 | ((c.ch >> 6) & 0x3F))
                             + chr(0x80 | (c.ch & 0x3F)))
                else:
                    line += "?"
            lines.append(line.rstrip())
        return "\n".join(lines)

    def to_ansi_string(self) -> str:
        """Render the grid with ANSI color escapes for terminal display.

        Uses run-length encoding: only emits SGR codes when colors change.
        Produces compact output suitable for direct terminal printing.
        """
        if not self._cells_ptr:
            return ""

        parts = ["\033[H"]  # Home cursor (no clear to avoid flicker)
        size = ctypes.sizeof(TuiCell)
        base = self._cells_ptr

        prev_style = None

        for y in range(self._rows):
            for x in range(self._cols):
                c = TuiCell.from_address(base + (y * self._cols + x) * size)

                if c.ch == 0:
                    # Empty cell: space with default style
                    cur = (0, 0, 0, 0, 0, 0, 0, 0)  # all zeros = default
                    if cur != prev_style:
                        parts.append("\033[0m")
                        prev_style = cur
                    parts.append(" ")
                    continue

                # Build style tuple
                cur = (c.fg_r, c.fg_g, c.fg_b, c.bg_r, c.bg_g, c.bg_b,
                       int(c.bold), int(c.dim))

                if cur != prev_style:
                    bold = ";1" if c.bold else ""
                    dim = ";2" if c.dim else ""
                    parts.append(
                        f"\033[38;2;{c.fg_r};{c.fg_g};{c.fg_b};"
                        f"48;2;{c.bg_r};{c.bg_g};{c.bg_b}{bold}{dim}m"
                    )
                    prev_style = cur

                # UTF-8 encode the character
                ch = c.ch
                if ch < 0x80:
                    parts.append(chr(ch))
                elif ch < 0x800:
                    parts.append(chr(0xC0 | (ch >> 6)))
                    parts.append(chr(0x80 | (ch & 0x3F)))
                elif ch < 0x10000:
                    parts.append(chr(0xE0 | (ch >> 12)))
                    parts.append(chr(0x80 | ((ch >> 6) & 0x3F)))
                    parts.append(chr(0x80 | (ch & 0x3F)))
                else:
                    parts.append("?")  # Skip 4-byte emoji

            parts.append("\r\n")
            prev_style = None  # Reset style each row

        parts.append("\033[0m")
        return "".join(parts)

    def __del__(self):
        if self._ptr and self._owned:
            _load().tui_grid_free(self._ptr)
            self._ptr = None


# ---------------------------------------------------------------------------
# Render a SceneTree to a TuiGrid
# ---------------------------------------------------------------------------
def render_scene_tree(tree, cols: int = 80, rows: int = 40,
                      source_w: int = 0, source_h: int = 0) -> TuiGrid:
    """Render a SceneTree's draw list to a TuiGrid.

    Call after scene_tree_process() to get the latest frame.

    Args:
        tree: A SceneTree instance (from sol.ui_bindings).
        cols: Terminal width in columns.
        rows: Terminal height in rows.
        source_w: Source pixel width. If 0, auto-detects from tree root.
        source_h: Source pixel height. If 0, auto-detects.

    Returns:
        TuiGrid with the rendered frame.
    """
    grid = TuiGrid(cols, rows)

    dl_ptr = tree.get_draw_list()
    if dl_ptr:
        if source_w > 0 and source_h > 0:
            _load().tui_render_to_grid_scaled(dl_ptr, grid._ptr,
                                                float(source_w), float(source_h))
        else:
            # Try to detect size from the first draw command's clip rect
            _load().tui_render_to_grid(dl_ptr, grid._ptr)

    return grid


def render_draw_list_to_string(dl_ptr, cols: int = 80, rows: int = 40) -> str:
    """Render a DrawList pointer to an ANSI string.

    Args:
        dl_ptr: Raw DrawList pointer from scene_tree_get_draw_list().
        cols: Terminal width.
        rows: Terminal height.

    Returns:
        ANSI-escaped string suitable for terminal output.
    """
    result_ptr = _load().tui_render_ansi(dl_ptr, cols, rows)
    if not result_ptr:
        return ""
    result = ctypes.cast(result_ptr, ctypes.c_char_p).value.decode("utf-8", errors="replace")
    # Free the C string
    from sol.bindings import _load as _eng
    _eng().free.restype = None
    _eng().free.argtypes = [ctypes.c_void_p]
    _eng().free(result_ptr)
    return result


# ---------------------------------------------------------------------------
# Keyboard input (raw terminal mode)
# ---------------------------------------------------------------------------
class TuiInput:
    """Non-blocking terminal keyboard input.

    Usage:
        inp = TuiInput()
        inp.start()
        while True:
            ev = inp.poll()
            if ev:
                print(f"Key: {ev}")
    """

    def __init__(self):
        self._active = False

    def start(self) -> bool:
        """Switch terminal to raw mode."""
        if self._active:
            return True
        ok = _load().tui_input_init()
        if ok:
            self._active = True
        return ok

    def stop(self):
        """Restore terminal settings."""
        if self._active:
            _load().tui_input_shutdown()
            self._active = False

    def poll(self) -> dict | None:
        """Check for pending key events. Returns a dict or None."""
        ev = _load().tui_input_poll()
        if ev.keycode == 0:
            return None

        if ev.is_special:
            name = _SPECIAL_NAMES.get(ev.keycode, f"SPECIAL({ev.keycode:06X})")
        elif ev.keycode < 0x80:
            name = chr(ev.keycode)
        else:
            name = f"U+{ev.keycode:04X}"

        return {
            "keycode": ev.keycode,
            "name": name,
            "is_special": ev.is_special,
            "pressed": ev.pressed,
        }

    def __enter__(self):
        self.start()
        return self

    def __exit__(self, *args):
        self.stop()
