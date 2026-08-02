"""ctypes bindings for the Sol Input system (Godot-style InputState).

Usage:
    from sol.input_bindings import (
        is_key_pressed, is_key_just_pressed,
        get_mouse_pos, is_mouse_pressed,
        poll_event, push_midi_event,
        KEY_A, KEY_W, KEY_SPACE, ...
    )
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
# Keycode constants
# ---------------------------------------------------------------------------
KEY_UNKNOWN    = 0
KEY_A, KEY_B, KEY_C, KEY_D, KEY_E, KEY_F, KEY_G, KEY_H, KEY_I = range(4, 13)
KEY_J, KEY_K, KEY_L, KEY_M, KEY_N, KEY_O, KEY_P, KEY_Q, KEY_R = range(13, 22)
KEY_S, KEY_T, KEY_U, KEY_V, KEY_W, KEY_X, KEY_Y, KEY_Z = range(22, 30)
KEY_1, KEY_2, KEY_3, KEY_4, KEY_5 = range(30, 35)
KEY_6, KEY_7, KEY_8, KEY_9, KEY_0 = range(35, 40)
KEY_RETURN    = 40
KEY_ESCAPE    = 41
KEY_BACKSPACE = 42
KEY_TAB       = 43
KEY_SPACE     = 44
KEY_LEFT      = 80
KEY_RIGHT     = 79
KEY_UP        = 82
KEY_DOWN      = 81
KEY_LSHIFT    = 225
KEY_RSHIFT    = 229
KEY_LCTRL     = 224
KEY_RCTRL     = 228
KEY_LALT      = 226
KEY_RALT      = 230

MOUSE_LEFT   = 1
MOUSE_MIDDLE = 2
MOUSE_RIGHT  = 3


# ---------------------------------------------------------------------------
# Event types
# ---------------------------------------------------------------------------
EV_NONE          = 0
EV_MIDI_NOTE_ON  = 1
EV_MIDI_NOTE_OFF = 2
EV_MIDI_CC       = 3
EV_DEVICE_ADDED  = 4
EV_DEVICE_REMOVED = 5
EV_WINDOW_RESIZE = 6
EV_QUIT          = 7


class SolEvent(ctypes.Structure):
    _fields_ = [
        ("type", ctypes.c_int),
        ("timestamp_us", ctypes.c_uint64),
        ("_pad", ctypes.c_int * 4),  # placeholder for union
    ]


# ---------------------------------------------------------------------------
# Query functions
# ---------------------------------------------------------------------------
_is_key_pressed = _load().input_is_key_pressed
_is_key_pressed.restype = ctypes.c_bool
_is_key_pressed.argtypes = [ctypes.c_int]

_is_key_just_pressed = _load().input_is_key_just_pressed
_is_key_just_pressed.restype = ctypes.c_bool
_is_key_just_pressed.argtypes = [ctypes.c_int]

_is_key_just_released = _load().input_is_key_just_released
_is_key_just_released.restype = ctypes.c_bool
_is_key_just_released.argtypes = [ctypes.c_int]

_get_mouse_x = _load().input_mouse_x
_get_mouse_x.restype = ctypes.c_float
_get_mouse_x.argtypes = []

_get_mouse_y = _load().input_mouse_y
_get_mouse_y.restype = ctypes.c_float
_get_mouse_y.argtypes = []

_is_mouse_pressed = _load().input_is_mouse_pressed
_is_mouse_pressed.restype = ctypes.c_bool
_is_mouse_pressed.argtypes = [ctypes.c_int]

_should_quit = _load().input_should_quit
_should_quit.restype = ctypes.c_bool
_should_quit.argtypes = []

_poll_event = _load().input_poll_event
_poll_event.restype = ctypes.c_bool
_poll_event.argtypes = [ctypes.POINTER(SolEvent)]

_push_event = _load().sol_push_event
_push_event.restype = ctypes.c_bool
_push_event.argtypes = [ctypes.POINTER(SolEvent)]


# ---------------------------------------------------------------------------
# Python-friendly wrappers
# ---------------------------------------------------------------------------

def is_key_pressed(keycode: int) -> bool:
    return _is_key_pressed(keycode)

def is_key_just_pressed(keycode: int) -> bool:
    return _is_key_just_pressed(keycode)

def is_key_just_released(keycode: int) -> bool:
    return _is_key_just_released(keycode)

def get_mouse_pos() -> tuple:
    return (_get_mouse_x(), _get_mouse_y())

def is_mouse_pressed(button: int = 1) -> bool:
    return _is_mouse_pressed(button)

def should_quit() -> bool:
    return _should_quit()

def poll_event():
    """Poll a one-shot event (MIDI, device, quit). Returns None if empty."""
    ev = SolEvent()
    if _poll_event(ctypes.byref(ev)):
        return ev
    return None

def push_midi_event(ev_type: int, note: int, velocity: float = 1.0,
                     channel: int = 0) -> bool:
    """Push a MIDI event from any thread (e.g. mido callback)."""
    # Build a minimal C-compatible event struct
    class MidiEvent(ctypes.Structure):
        _fields_ = [
            ("type", ctypes.c_int),
            ("timestamp_us", ctypes.c_uint64),
            ("note", ctypes.c_int),
            ("velocity", ctypes.c_float),
            ("channel", ctypes.c_int),
        ]
    ev = MidiEvent()
    ev.type = ev_type
    ev.note = note
    ev.velocity = velocity
    ev.channel = channel
    # Cast to SolEvent for the C call
    return _push_event(ctypes.cast(ctypes.byref(ev), ctypes.POINTER(SolEvent)))
