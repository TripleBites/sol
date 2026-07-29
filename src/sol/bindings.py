import ctypes
import os
import platform
import sys

def _find_library():
    """Locate libsol.so across Linux, Android, macOS."""
    name = "libsol.so"
    if platform.system() == "Darwin":
        name = "libsol.dylib"
    elif platform.system() == "Windows":
        name = "sol.dll"

    # 1. Same directory as this file
    script_dir = os.path.dirname(os.path.abspath(__file__))
    candidates = [os.path.join(script_dir, name)]

    # 2. Adjacent to Sol/ source (dev layout)
    candidates.append(os.path.join(script_dir, "..", "..", "sol", name))

    # 3. Android: libs are in the app lib path, often already in LD path
    #    On Android, plain CDLL("libsol.so") often works if bundled.

    for path in candidates:
        if os.path.exists(path):
            return path
    return name  # Let system loader resolve it


_lib = None

def _load():
    global _lib
    if _lib is not None:
        return _lib

    path = _find_library()
    _lib = ctypes.CDLL(path)

    # --- Bind functions -------------------------------------------------
    _lib.sol_init.restype = ctypes.c_bool 
    _lib.sol_init.argtypes = [ctypes.c_char_p, ctypes.c_int, ctypes.c_int]

    _lib.sol_shutdown.restype = None
    _lib.sol_shutdown.argtypes = []

    return _lib


def init():
    if _load().sol_init() != 0:
        raise RuntimeError("sol_init failed")


def shutdown():
    _load().sol_shutdown()
