"""ctypes bindings for the Sol Audio system (AudioNode, AudioPipeline, etc.)."""
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
# Waveform enum
# ---------------------------------------------------------------------------
OSC_SINE     = 0
OSC_SQUARE   = 1
OSC_SAW      = 2
OSC_TRIANGLE = 3
OSC_NOISE    = 4

WAVEFORM_NAMES = {
    0: "SINE",
    1: "SQUARE",
    2: "SAW",
    3: "TRIANGLE",
    4: "NOISE",
}

WAVEFORM_FROM_NAME = {v: k for k, v in WAVEFORM_NAMES.items()}


# ---------------------------------------------------------------------------
# OscillatorNode
# ---------------------------------------------------------------------------
_osc_node_new = _load().osc_node_new
_osc_node_new.restype = ctypes.c_void_p
_osc_node_new.argtypes = [ctypes.c_int, ctypes.c_float, ctypes.c_float, ctypes.c_int]

_osc_node_set_freq = _load().osc_node_set_freq
_osc_node_set_freq.restype = None
_osc_node_set_freq.argtypes = [ctypes.c_void_p, ctypes.c_float]

_osc_node_set_amp = _load().osc_node_set_amp
_osc_node_set_amp.restype = None
_osc_node_set_amp.argtypes = [ctypes.c_void_p, ctypes.c_float]


# ---------------------------------------------------------------------------
# MixerNode
# ---------------------------------------------------------------------------
_mixer_node_new = _load().mixer_node_new
_mixer_node_new.restype = ctypes.c_void_p
_mixer_node_new.argtypes = []


# ---------------------------------------------------------------------------
# GainNode
# ---------------------------------------------------------------------------
_gain_node_new = _load().gain_node_new
_gain_node_new.restype = ctypes.c_void_p
_gain_node_new.argtypes = [ctypes.c_float]

_gain_node_set_level = _load().gain_node_set_level
_gain_node_set_level.restype = None
_gain_node_set_level.argtypes = [ctypes.c_void_p, ctypes.c_float]


# ---------------------------------------------------------------------------
# AudioNode (base — tree ops)
# ---------------------------------------------------------------------------
_audio_node_free = _load().audio_node_free
_audio_node_free.restype = None
_audio_node_free.argtypes = [ctypes.c_void_p]

_audio_node_set_id = _load().audio_node_set_id
_audio_node_set_id.restype = None
_audio_node_set_id.argtypes = [ctypes.c_void_p, ctypes.c_uint32]

_audio_node_get_id = _load().audio_node_get_id
_audio_node_get_id.restype = ctypes.c_uint32
_audio_node_get_id.argtypes = [ctypes.c_void_p]


# ---------------------------------------------------------------------------
# AudioPipeline
# ---------------------------------------------------------------------------
_audio_pipeline_new = _load().audio_pipeline_new
_audio_pipeline_new.restype = ctypes.c_void_p
_audio_pipeline_new.argtypes = [ctypes.c_int, ctypes.c_int]

_audio_pipeline_free = _load().audio_pipeline_free
_audio_pipeline_free.restype = None
_audio_pipeline_free.argtypes = [ctypes.c_void_p]

_audio_pipeline_set_root = _load().audio_pipeline_set_root
_audio_pipeline_set_root.restype = None
_audio_pipeline_set_root.argtypes = [ctypes.c_void_p, ctypes.c_void_p]

_audio_pipeline_start = _load().audio_pipeline_start
_audio_pipeline_start.restype = ctypes.c_bool
_audio_pipeline_start.argtypes = [ctypes.c_void_p, ctypes.c_void_p]

_audio_pipeline_stop = _load().audio_pipeline_stop
_audio_pipeline_stop.restype = None
_audio_pipeline_stop.argtypes = [ctypes.c_void_p]

_audio_pipeline_register_node = _load().audio_pipeline_register_node
_audio_pipeline_register_node.restype = ctypes.c_uint32
_audio_pipeline_register_node.argtypes = [ctypes.c_void_p, ctypes.c_void_p]

_audio_pipeline_send_set_param = _load().audio_pipeline_send_set_param
_audio_pipeline_send_set_param.restype = None
_audio_pipeline_send_set_param.argtypes = [ctypes.c_void_p, ctypes.c_uint32,
                                            ctypes.c_char_p, ctypes.c_float]

_audio_pipeline_add_probe = _load().audio_pipeline_add_probe
_audio_pipeline_add_probe.restype = ctypes.c_int
_audio_pipeline_add_probe.argtypes = [ctypes.c_void_p, ctypes.c_uint32, ctypes.c_int]

_audio_pipeline_probe_read = _load().audio_pipeline_probe_read
_audio_pipeline_probe_read.restype = ctypes.c_int
_audio_pipeline_probe_read.argtypes = [ctypes.c_void_p, ctypes.c_int,
                                        ctypes.POINTER(ctypes.c_float), ctypes.c_int]

# Platform constructors (from io)
_sol_io_headless = _load().sol_io_headless
_sol_io_headless.restype = ctypes.c_void_p
_sol_io_headless.argtypes = []

# ---------------------------------------------------------------------------
# EnvelopeNode
# ---------------------------------------------------------------------------
_envelope_node_new = _load().envelope_node_new
_envelope_node_new.restype = ctypes.c_void_p
_envelope_node_new.argtypes = [ctypes.c_float, ctypes.c_float, ctypes.c_float,
                                ctypes.c_float, ctypes.c_int]

_envelope_node_trigger = _load().envelope_node_trigger
_envelope_node_trigger.restype = None
_envelope_node_trigger.argtypes = [ctypes.c_void_p, ctypes.c_float]

_envelope_node_release = _load().envelope_node_release
_envelope_node_release.restype = None
_envelope_node_release.argtypes = [ctypes.c_void_p]

# ---------------------------------------------------------------------------
# VoiceNode
# ---------------------------------------------------------------------------
_voice_node_new = _load().voice_node_new
_voice_node_new.restype = ctypes.c_void_p
_voice_node_new.argtypes = [ctypes.c_int, ctypes.c_float, ctypes.c_int]

_voice_node_note_on = _load().voice_node_note_on
_voice_node_note_on.restype = None
_voice_node_note_on.argtypes = [ctypes.c_void_p, ctypes.c_int, ctypes.c_float, ctypes.c_int]

_voice_node_note_off = _load().voice_node_note_off
_voice_node_note_off.restype = None
_voice_node_note_off.argtypes = [ctypes.c_void_p]

_midi_to_freq = _load().midi_to_freq
_midi_to_freq.restype = ctypes.c_float
_midi_to_freq.argtypes = [ctypes.c_int]


# ---------------------------------------------------------------------------
# AudioNode tree ops (AudioNode-specific, not Node vtable)
# ---------------------------------------------------------------------------
_audio_node_add_child = _load().audio_node_add_child
_audio_node_add_child.restype = None
_audio_node_add_child.argtypes = [ctypes.c_void_p, ctypes.c_void_p]

# ---------------------------------------------------------------------------
# Node name — reused from scene Node
# ---------------------------------------------------------------------------
_node_set_name = _load().node_set_name
_node_set_name.restype = None
_node_set_name.argtypes = [ctypes.c_void_p, ctypes.c_char_p]


# ---------------------------------------------------------------------------
# High-level Python wrappers
# ---------------------------------------------------------------------------

class AudioNode:
    """Python wrapper for a C AudioNode pointer."""

    __slots__ = ("_ptr", "_owned")

    def __init__(self, ptr):
        self._ptr = ptr
        self._owned = False

    @property
    def ptr(self):
        return self._ptr

    @property
    def node_id(self):
        """Query node ID from C side (assigned by pipeline on set_root)."""
        return _audio_node_get_id(self._ptr)

    def add_child(self, child: "AudioNode"):
        _audio_node_add_child(self._ptr, child._ptr)
        # Child is now owned by the tree — Python must not double-free
        child._owned = True

    def set_name(self, name: str):
        _node_set_name(self._ptr, name.encode("utf-8"))

    def __del__(self):
        if self._ptr and not self._owned:
            _audio_node_free(self._ptr)
            self._ptr = None


class OscillatorNode(AudioNode):
    """A waveform generator."""

    def __init__(self, waveform=OSC_SINE, freq=440.0, amp=1.0, sample_rate=44100):
        ptr = _osc_node_new(waveform, freq, amp, sample_rate)
        super().__init__(ptr)

    def set_freq(self, hz: float):
        _osc_node_set_freq(self._ptr, hz)

    def set_amp(self, amp: float):
        _osc_node_set_amp(self._ptr, amp)

    @classmethod
    def sine(cls, freq=440.0, amp=1.0, sample_rate=44100):
        return cls(OSC_SINE, freq, amp, sample_rate)

    @classmethod
    def square(cls, freq=440.0, amp=1.0, sample_rate=44100):
        return cls(OSC_SQUARE, freq, amp, sample_rate)

    @classmethod
    def saw(cls, freq=440.0, amp=1.0, sample_rate=44100):
        return cls(OSC_SAW, freq, amp, sample_rate)

    @classmethod
    def triangle(cls, freq=440.0, amp=1.0, sample_rate=44100):
        return cls(OSC_TRIANGLE, freq, amp, sample_rate)

    @classmethod
    def noise(cls, amp=0.3, sample_rate=44100):
        return cls(OSC_NOISE, 440.0, amp, sample_rate)


class MixerNode(AudioNode):
    """Sums all children into the output."""

    def __init__(self):
        ptr = _mixer_node_new()
        super().__init__(ptr)


class GainNode(AudioNode):
    """Multiplies the buffer by a level scalar."""

    def __init__(self, level=1.0):
        ptr = _gain_node_new(level)
        super().__init__(ptr)

    def set_level(self, level: float):
        _gain_node_set_level(self._ptr, level)


class EnvelopeNode(AudioNode):
    """ADSR amplitude envelope."""

    def __init__(self, attack=0.01, decay=0.15, sustain=0.7, release=0.3,
                 sample_rate=44100):
        ptr = _envelope_node_new(attack, decay, sustain, release, sample_rate)
        super().__init__(ptr)

    def trigger(self, velocity: float = 1.0):
        _envelope_node_trigger(self._ptr, velocity)

    def release(self):
        _envelope_node_release(self._ptr)

    @classmethod
    def adsr(cls, a=0.01, d=0.15, s=0.7, r=0.3, sr=44100):
        return cls(a, d, s, r, sr)


class VoiceNode(AudioNode):
    """One active MIDI note — owns an oscillator + envelope.

    Tree convention:
        child[0] = EnvelopeNode
        child[1] = OscillatorNode
    """

    def __init__(self, midi_note=60, velocity=0.8, sample_rate=44100):
        ptr = _voice_node_new(midi_note, velocity, sample_rate)
        super().__init__(ptr)

    def note_on(self, note: int, velocity: float = 0.8, sample_rate: int = 44100):
        _voice_node_note_on(self._ptr, note, velocity, sample_rate)

    def note_off(self):
        _voice_node_note_off(self._ptr)

    @staticmethod
    def midi_to_freq(note: int) -> float:
        return _midi_to_freq(note)


class AudioPipeline:
    """Real-time audio processing graph.

    Usage:
        ap = AudioPipeline(sample_rate=44100)
        mixer = MixerNode()
        osc = OscillatorNode.sine(freq=261.6)
        mixer.add_child(osc)
        ap.set_root(mixer)
        ap.start()   # begins audio on headless (ALSA) or SDL3
        ...
        ap.stop()
    """

    def __init__(self, sample_rate=44100, buffer_size=512):
        self._ptr = _audio_pipeline_new(sample_rate, buffer_size)
        self.sample_rate = sample_rate
        self._running = False

    @property
    def ptr(self):
        return self._ptr

    def set_root(self, node: AudioNode):
        _audio_pipeline_set_root(self._ptr, node._ptr)
        # Pipeline owns the tree — Python must not free nodes
        node._owned = True

    def start(self):
        """Start audio processing (headless ALSA mode)."""
        if self._running:
            return
        platform_ptr = _sol_io_headless()
        ok = _audio_pipeline_start(self._ptr, platform_ptr)
        if ok:
            self._running = True
        return ok

    def stop(self):
        if not self._running:
            return
        _audio_pipeline_stop(self._ptr)
        self._running = False

    def send_set_param(self, node_id: int, param: str, value: float):
        _audio_pipeline_send_set_param(self._ptr, node_id,
                                        param.encode("utf-8"), value)

    def send_note_on(self, voice_id: int, note: int, velocity: float):
        """Send a note-on to a VoiceNode (packed as set_param)."""
        _audio_pipeline_send_set_param(self._ptr, voice_id,
                                        "note_on".encode("utf-8"), float(note))

    def send_note_off(self, voice_id: int):
        """Send a note-off to a VoiceNode."""
        _audio_pipeline_send_set_param(self._ptr, voice_id,
                                        "note_off".encode("utf-8"), 0.0)

    def add_probe(self, node_id: int, buf_size: int = 1024) -> int:
        """Add a probe at the given node. Returns probe index or -1."""
        return _audio_pipeline_add_probe(self._ptr, node_id, buf_size)

    def read_probe(self, probe_index: int, max_samples: int = 1024):
        """Read probe data into a Python list of floats."""
        buf = (ctypes.c_float * max_samples)()
        n = _audio_pipeline_probe_read(self._ptr, probe_index, buf, max_samples)
        return list(buf[:n])

    def __del__(self):
        if self._ptr:
            self.stop()
            # audio_pipeline_free frees the C node tree —
            # Python wrappers are marked _owned=True so they won't double-free
            _audio_pipeline_free(self._ptr)
            self._ptr = None
