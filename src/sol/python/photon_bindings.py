"""ctypes bindings for the Sol 2D batch renderer (Render2D).

Usage:
    from sol.photon_bindings import Render2D

    r = Render2D()
    r.begin(800, 600)
    r.draw_rect(0, 0, 800, 600, (0.1, 0.1, 0.3, 1.0))
    r.draw_line(0, 0, 800, 600, (1, 1, 1, 0.5), 2.0)
    r.draw_circle(400, 300, 50, (0.2, 1, 0.5, 1), filled=True)
    buf, n = r.flush()  # returns packed vertex data
"""
import ctypes
from sol.bindings import _load as _load_engine

_lib = None

def _load():
    global _lib
    if _lib is not None:
        return _lib
    _lib = _load_engine()
    return _lib

# ---------------------------------------------------------------------------
# Render2D type (opaque — allocated and managed in C)
# ---------------------------------------------------------------------------

# render2d_new (allocates + inits)
_render2d_new = _load().render2d_new
_render2d_new.restype = ctypes.c_void_p
_render2d_new.argtypes = []

# render2d_free (frees)
_render2d_free = _load().render2d_free
_render2d_free.restype = None
_render2d_free.argtypes = [ctypes.c_void_p]

# render2d_init (for externally-allocated, e.g. inside SolVulkan)

# render2d_begin
_render2d_begin = _load().render2d_begin
_render2d_begin.restype = None
_render2d_begin.argtypes = [ctypes.c_void_p, ctypes.c_float, ctypes.c_float]

# render2d_set_z
_render2d_set_z = _load().render2d_set_z
_render2d_set_z.restype = None
_render2d_set_z.argtypes = [ctypes.c_void_p, ctypes.c_float]

# render2d_draw_rect
_render2d_draw_rect = _load().render2d_draw_rect
_render2d_draw_rect.restype = None
_render2d_draw_rect.argtypes = [
    ctypes.c_void_p,
    ctypes.c_float, ctypes.c_float, ctypes.c_float, ctypes.c_float,
    ctypes.c_float, ctypes.c_float, ctypes.c_float, ctypes.c_float,
    ctypes.c_float,  # corner_radius
]

# render2d_draw_rect_simple
_render2d_draw_rect_simple = _load().render2d_draw_rect_simple
_render2d_draw_rect_simple.restype = None
_render2d_draw_rect_simple.argtypes = [
    ctypes.c_void_p,
    ctypes.c_float, ctypes.c_float, ctypes.c_float, ctypes.c_float,
    ctypes.c_float, ctypes.c_float, ctypes.c_float, ctypes.c_float,
]

# render2d_draw_rect_border
_render2d_draw_rect_border = _load().render2d_draw_rect_border
_render2d_draw_rect_border.restype = None
_render2d_draw_rect_border.argtypes = [
    ctypes.c_void_p,
    ctypes.c_float, ctypes.c_float, ctypes.c_float, ctypes.c_float,
    ctypes.c_float, ctypes.c_float, ctypes.c_float, ctypes.c_float,
    ctypes.c_float,  # border_width
]

# render2d_draw_line
_render2d_draw_line = _load().render2d_draw_line
_render2d_draw_line.restype = None
_render2d_draw_line.argtypes = [
    ctypes.c_void_p,
    ctypes.c_float, ctypes.c_float, ctypes.c_float, ctypes.c_float,
    ctypes.c_float, ctypes.c_float, ctypes.c_float, ctypes.c_float,
    ctypes.c_float,  # thickness
]

# render2d_draw_circle
_render2d_draw_circle = _load().render2d_draw_circle
_render2d_draw_circle.restype = None
_render2d_draw_circle.argtypes = [
    ctypes.c_void_p,
    ctypes.c_float, ctypes.c_float, ctypes.c_float,
    ctypes.c_float, ctypes.c_float, ctypes.c_float, ctypes.c_float,
    ctypes.c_bool,  # filled
]

# render2d_flush
_render2d_flush = _load().render2d_flush
_render2d_flush.restype = None
_render2d_flush.argtypes = [
    ctypes.c_void_p,
    ctypes.POINTER(ctypes.c_void_p),   # out_vertices
    ctypes.POINTER(ctypes.c_uint32),   # out_vertex_count
]


# ---------------------------------------------------------------------------
# Python wrapper
# ---------------------------------------------------------------------------

class Render2D:
    """Immediate-mode 2D batch renderer.

    Usage per frame:
        r = Render2D()
        r.begin(800, 600)
        r.draw_rect(10, 10, 100, 100, (1, 0, 0, 1))
        r.draw_line(0, 0, 800, 600, (1, 1, 1, 0.5), thickness=2.0)
        vertices, count = r.flush()
    """

    def __init__(self):
        self._ptr = _render2d_new()

    def begin(self, screen_w: float, screen_h: float):
        """Start a new frame. Clears all accumulated draw data."""
        _render2d_begin(self._ptr, screen_w, screen_h)

    def set_z(self, z: float):
        """Set z-index for subsequent draws. Higher z = on top."""
        _render2d_set_z(self._ptr, z)

    def draw_rect(self, x: float, y: float, w: float, h: float,
                  color: tuple, corner_radius: float = 0.0):
        """Draw a filled rectangle with optional rounded corners."""
        r, g, b, a = color[0:4]
        _render2d_draw_rect(self._ptr, x, y, w, h,
                            r, g, b, a, corner_radius)

    def draw_rect_border(self, x: float, y: float, w: float, h: float,
                         color: tuple, border_width: float = 1.0):
        """Draw a rectangle outline."""
        r, g, b, a = color[0:4]
        _render2d_draw_rect_border(self._ptr, x, y, w, h,
                                    r, g, b, a, border_width)

    def draw_line(self, x1: float, y1: float, x2: float, y2: float,
                  color: tuple, thickness: float = 1.0):
        """Draw a line from (x1,y1) to (x2,y2)."""
        r, g, b, a = color[0:4]
        _render2d_draw_line(self._ptr, x1, y1, x2, y2,
                            r, g, b, a, thickness)

    def draw_circle(self, cx: float, cy: float, radius: float,
                    color: tuple, filled: bool = True):
        """Draw a filled or outlined circle."""
        r, g, b, a = color[0:4]
        _render2d_draw_circle(self._ptr, cx, cy, radius,
                              r, g, b, a, filled)

    def flush(self) -> tuple:
        """Sort by z, generate vertex data, return (bytes, vertex_count)."""
        out_verts = ctypes.c_void_p()
        out_count = ctypes.c_uint32()
        _render2d_flush(self._ptr,
                        ctypes.byref(out_verts),
                        ctypes.byref(out_count))

        # Read packed vertex data: each vertex = 6 floats (pos2 + color4)
        count = out_count.value
        FloatArray = ctypes.c_float * (count * 6)
        data = FloatArray.from_address(out_verts.value)
        # Copy to Python list to avoid dangling pointer issues
        return list(data), count

    def __del__(self):
        if self._ptr:
            _render2d_free(self._ptr)
            self._ptr = None
