"""Neptune TUI Mode — professional terminal synthesizer.

Uses the Sol engine's core init/update loop. Rendering is handled
by the TUI platform backend (SOL_BACKEND=tui). The Python layer
only manages audio, keyboard input, and a status overlay.

Run:
    PYTHONPATH=src SOL_BACKEND=tui python3 -m neptune.tui_mode
"""
import sys
import os
import time
import select
import termios
import tty
import shutil

_src = os.path.join(os.path.dirname(__file__), "..")
if _src not in sys.path:
    sys.path.insert(0, _src)

os.environ["SOL_BACKEND"] = "tui"

from sol.bindings import init, update, shutdown, _load
from sol.audio_bindings import (
    AudioPipeline, MixerNode, VoiceNode,
    EnvelopeNode, OscillatorNode, GainNode, _audio_pipeline_register_node,
)
from sol.ui_bindings import SceneTree, _scene_tree_set_root
from neptune.gui.synth_panel import SynthPanel

# ---------------------------------------------------------------------------
# Constants
# ---------------------------------------------------------------------------
VIRTUAL_W = 900
VIRTUAL_H = 600
SAMPLE_RATE = 44100
DT = 0.025

NOTE_NAMES = {
    60: "C4", 61: "C#4", 62: "D4", 63: "D#4", 64: "E4", 65: "F4",
    66: "F#4", 67: "G4", 68: "G#4", 69: "A4", 70: "A#4", 71: "B4",
    72: "C5", 74: "D5", 76: "E5", 77: "F5", 79: "G5", 81: "A5", 83: "B5", 84: "C6",
}

KEY_TO_NOTE = {
    'z': 60, 'x': 62, 'c': 64, 'v': 65, 'b': 67, 'n': 69, 'm': 71, ',': 72,
    's': 61, 'd': 63, 'g': 66, 'h': 68, 'j': 70,
    'q': 72, 'w': 74, 'e': 76, 'r': 77, 't': 79, 'y': 81, 'u': 83, 'i': 84,
    '2': 73, '3': 75, '5': 78, '6': 80, '7': 82,
}

WAVEFORM_NAMES = {0: "Sine", 1: "Square", 2: "Saw", 3: "Triangle", 4: "Noise"}
KNOB_NAMES = ["waveform", "freq", "amp", "attack", "decay", "sustain", "release", "volume"]


# ---------------------------------------------------------------------------
# Audio helpers
# ---------------------------------------------------------------------------
def build_voice(sr, note, velocity=0.8):
    voice = VoiceNode(note, velocity, sr)
    env = EnvelopeNode.adsr(attack=0.01, decay=0.3, sustain=0.6, release=0.8, sample_rate=sr)
    osc = OscillatorNode.sine(VoiceNode.midi_to_freq(note), amp=0.4, sample_rate=sr)
    voice.add_child(env)
    voice.add_child(osc)
    return voice


def build_patch(pipeline, sr):
    mixer = MixerNode()
    mixer.set_name("main")
    gain = GainNode(0.5)
    gain.set_name("master")
    gain.add_child(mixer)
    pipeline.set_root(gain)
    return {"mixer": mixer, "gain": gain}


# ---------------------------------------------------------------------------
# Raw terminal keyboard
# ---------------------------------------------------------------------------
class RawKeyboard:
    def __init__(self):
        self._old = None
        self._buf = b""

    def start(self):
        if not os.isatty(sys.stdin.fileno()):
            return False
        self._old = termios.tcgetattr(sys.stdin)
        tty.setraw(sys.stdin.fileno())
        import fcntl
        self._old_flags = fcntl.fcntl(sys.stdin, fcntl.F_GETFL)
        fcntl.fcntl(sys.stdin, fcntl.F_SETFL, self._old_flags | os.O_NONBLOCK)
        return True

    def stop(self):
        if self._old:
            termios.tcsetattr(sys.stdin, termios.TCSADRAIN, self._old)
            self._old = None
        if hasattr(self, '_old_flags'):
            import fcntl
            fcntl.fcntl(sys.stdin, fcntl.F_SETFL, self._old_flags)

    def poll(self):
        while True:
            ready, _, _ = select.select([sys.stdin], [], [], 0)
            if not ready:
                break
            try:
                chunk = os.read(sys.stdin.fileno(), 64)
                if not chunk:
                    break
                self._buf += chunk
            except (BlockingIOError, OSError):
                break
        keys = []
        i = 0
        while i < len(self._buf):
            b = self._buf[i]
            if b == 0x1B:
                if i + 2 < len(self._buf) and self._buf[i + 1] == 0x5B:
                    final = self._buf[i + 2]
                    if final in (0x41, 0x42, 0x43, 0x44):
                        keys.append(chr(0x1B) + '[' + chr(final))
                        i += 3
                        continue
                keys.append('\x1b')
                i += 1
                continue
            elif 0x20 <= b < 0x7F:
                keys.append(chr(b).lower())
                i += 1
            elif b == 0x20:
                keys.append('space')
                i += 1
            elif b == 0x09:
                keys.append('tab')
                i += 1
            elif b == 0x0D or b == 0x0A:
                keys.append('enter')
                i += 1
            else:
                i += 1
        self._buf = self._buf[i:] if i < len(self._buf) else b""
        return keys


# ---------------------------------------------------------------------------
# Main loop
# ---------------------------------------------------------------------------
def run():
    # -- Build audio pipeline --
    pipeline = AudioPipeline(sample_rate=SAMPLE_RATE, buffer_size=256)
    info = build_patch(pipeline, SAMPLE_RATE)
    mixer = info["mixer"]
    pipeline._root = info["gain"]
    mixer_id = _audio_pipeline_register_node(pipeline._ptr, mixer.ptr)
    mixer._owned = True
    probe_idx = pipeline.add_probe(mixer_id, 1024)

    # -- Build synth panel --
    panel = SynthPanel(pipeline)
    panel_root = panel.get_root()

    # -- Init engine --
    if not init("Neptune Synth", VIRTUAL_W, VIRTUAL_H):
        print("ERROR: Engine init failed")
        return 1

    tree = SceneTree.from_ptr(_load().sol_get_scene_tree())
    _scene_tree_set_root(tree._ptr, panel_root.ptr)
    panel_root.set_rect(0, 0, VIRTUAL_W, VIRTUAL_H)
    panel.set_status("Ready")

    # -- Start audio (uses TUI platform which has ALSA audio) --
    pipeline.start(platform="tui")

    # -- Knob state --
    knob_idx = 0
    knobs = panel._knobs

    def adj_knob(d):
        k = knobs.get(KNOB_NAMES[knob_idx])
        if k:
            k.set_value(k.get_value() + d)

    def next_knob():
        nonlocal knob_idx
        knob_idx = (knob_idx + 1) % len(KNOB_NAMES)

    # -- Voice pool --
    voices = {}
    note_times = {}
    active_note = ""
    AUTO_RELEASE = 0.4

    def note_on(note, vel=0.8):
        nonlocal active_note
        if note in voices:
            return
        v = build_voice(SAMPLE_RATE, note, vel)
        mixer.add_child(v)
        vid = _audio_pipeline_register_node(pipeline._ptr, v.ptr)
        v._owned = True
        pipeline.send_note_on(vid, note, vel)
        voices[note] = (vid, v)
        note_times[note] = 0.0
        active_note = NOTE_NAMES.get(note, f"N{note}")
        panel.set_active_note(active_note)

    def note_off(note):
        nonlocal active_note
        if note not in voices:
            return
        vid, _ = voices.pop(note)
        pipeline.send_note_off(vid)
        note_times.pop(note, None)
        if not voices:
            active_note = ""
            panel.set_active_note("")

    # -- Keyboard --
    kbd = RawKeyboard()
    kbd.start()

    # -- Terminal --
    term_cols = shutil.get_terminal_size((80, 24)).columns
    print("\033[?25l\033[2J\033[H")  # hide cursor, clear

    held = set()
    running = True

    try:
        while running:
            # -- Poll keyboard --
            new_keys = set(kbd.poll())

            for key in new_keys - held:
                if key in ('\x1b', 'q'):
                    running = False
                    break

                if key in KEY_TO_NOTE:
                    note_on(KEY_TO_NOTE[key])
                elif key == 'space':
                    for n in list(voices):
                        note_off(n)
                    panel.set_status("Released")
                elif key == 'tab':
                    next_knob()
                elif key == '\x1b[A':
                    adj_knob(0.02)
                elif key == '\x1b[B':
                    adj_knob(-0.02)
                elif key == '\x1b[C':
                    adj_knob(0.05)
                elif key == '\x1b[D':
                    adj_knob(-0.05)
                elif key == '1':
                    if "waveform" in knobs:
                        wf = int(knobs["waveform"].get_value())
                        wf = (wf + 1) % 5
                        knobs["waveform"].set_value(float(wf))
                        panel.set_status(f"Wave: {WAVEFORM_NAMES[wf]}")
                        if "osc" in panel._node_ids:
                            pipeline.send_set_param(panel._node_ids["osc"], "waveform", float(wf))
                elif key == 'k':
                    from neptune.patch import save_patch
                    try:
                        save_patch(pipeline, "neptune_tui.solpatch")
                        panel.set_status("Saved!")
                    except Exception as e:
                        panel.set_status(f"Save: {e}")
                elif key == 'l':
                    from neptune.patch import load_patch
                    try:
                        load_patch(pipeline, "neptune_tui.solpatch")
                        panel.set_status("Loaded!")
                    except FileNotFoundError:
                        panel.set_status("No saved patch")
                    except Exception as e:
                        panel.set_status(f"Load: {e}")

            held = new_keys

            # -- Auto-release --
            for n in list(note_times):
                note_times[n] += DT
                if note_times[n] > AUTO_RELEASE:
                    note_off(n)

            # -- Waveform --
            try:
                data = pipeline.read_probe(probe_idx, 256)
                if data:
                    panel.update_waveform(data)
            except Exception:
                pass

            # -- Engine frame: SceneTree process + TUI render --
            panel_root.set_rect(0, 0, VIRTUAL_W, VIRTUAL_H)
            update()  # renders via platform->render (ANSI output)

            # -- Status bar overlay (after TUI frame) --
            kn = KNOB_NAMES[knob_idx]
            kv = knobs[kn].get_value() if kn in knobs else 0
            status = (
                f" [{kn}={kv:.3f}]"
                f"  Voices:{len(voices)}"
                f"  Note:{active_note}"
                f"  | arrows:adjust tab:next 1:wave K:save L:load space:release Q:quit"
            )
            # Print status on the line after the TUI frame
            print(f"\033[0m\033[7m{status.ljust(term_cols)}\033[0m")
            # Keyboard guide
            guide = (
                " z x c v b n m ,   = C D E F G A B C  |"
                " q w e r t y u i = C D E F G A B C  |"
                " s d   g h j / 2 3   5 6 7 = sharps"
            )
            print(f"\033[2m{guide[:term_cols]}\033[0m")

            time.sleep(DT)

    except KeyboardInterrupt:
        pass
    finally:
        kbd.stop()
        pipeline.stop()
        shutdown()
        print("\033[0m\033[?25h\033[2J\033[H")

    print("Goodbye!")
    return 0


if __name__ == "__main__":
    raise SystemExit(run())
