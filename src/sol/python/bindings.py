"""ctypes bindings to the Sol C99 engine library."""
import ctypes
import os
import platform as _platform


def _find_library():
    """Locate the sol shared library."""
    system = _io.system()
    if system == "Linux":
        name = "libsol.so"
    elif system == "Darwin":
        name = "libsol.dylib"
    elif system == "Windows":
        name = "sol.dll"
    else:
        name = "libsol.so"

    # Search paths
    script_dir = os.path.dirname(os.path.abspath(__file__))
    candidates = [
        os.path.join(script_dir, name),                         # alongside bindings.py
        os.path.join(script_dir, "..", "..", "build", name),    # setuptools build dir
        os.path.join(script_dir, "..", "engine", name),         # dev: src/sol -> src/engine
    ]

    for path in candidates:
        if os.path.exists(path):
            return path
    return name  # fall back to system loader


_lib = None


def _load():
    global _lib
    if _lib is not None:
        return _lib

    path = _find_library()
    _lib = ctypes.CDLL(path)

    # sol_init
    _lib.sol_init.restype = ctypes.c_bool
    _lib.sol_init.argtypes = [ctypes.c_char_p, ctypes.c_int, ctypes.c_int]

    # sol_update (returns false when window should close)
    _lib.sol_update.restype = ctypes.c_bool
    _lib.sol_update.argtypes = []

    # sol_shutdown
    _lib.sol_shutdown.restype = None
    _lib.sol_shutdown.argtypes = []

    # sol_get_size
    _lib.sol_get_size.restype = None
    _lib.sol_get_size.argtypes = [ctypes.POINTER(ctypes.c_int), ctypes.POINTER(ctypes.c_int)]

    return _lib


def init(title: str = "Sol Engine", width: int = 800, height: int = 600) -> bool:
    """Initialize the engine. Returns True on success."""
    return _load().sol_init(title.encode("utf-8"), width, height)


def update() -> bool:
    """Pump events and render a frame. Returns False if the window should close."""
    return _load().sol_update()


def shutdown() -> None:
    """Shut down the engine and clean up resources."""
    _load().sol_shutdown()


def get_size() -> tuple[int, int]:
    """Get the current framebuffer size in pixels."""
    w = ctypes.c_int()
    h = ctypes.c_int()
    _load().sol_get_size(ctypes.byref(w), ctypes.byref(h))
    return (w.value, h.value)
