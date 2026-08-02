# Neptune Audio Synthesizer — Specification v2.0
## Python Composition Layer over Sol Audio Engine

> **Neptune is a thin Python composition layer.** It creates `AudioNode` trees from the Sol C99 engine, routes input (keyboard, MIDI) to `AudioPipeline` control messages, reads probe ring buffers for GUI waveform display, and serializes/deserializes patches. The hot audio path is C. The logic and composition is Python.

---

## 0. Relationship to Sol Engine

Neptune does NOT implement oscillators, mixers, envelopes, or real-time audio I/O. Those live in `src/sol/audio/` and `src/sol/io/` as C99 code.

Neptune DOES:
- Instantiate and wire together `AudioNode` subclasses into an `AudioPipeline`
- Translate QWERTY keystrokes / MIDI messages into `NoteOn`/`NoteOff` control queue messages
- Poll `AudioPipeline` probe ring buffers and render waveforms via Sol's `DrawList`
- Build a synth GUI panel using Sol's `Control` widgets
- Serialize/deserialize `AudioNode` trees to/from `.solpatch` JSON files

```
┌──────────────────────────────────────────────────────────┐
│  Neptune (Python, ~500 lines)                           │
│                                                          │
│  main.py ─── creates AudioPipeline, wires AudioNodes    │
│  gui/     ─── Sol Control tree for synth panel          │
│  input/   ─── keyboard.py, midi.py → control queue      │
│  patch.py ─── JSON save/load of AudioNode tree          │
├──────────────────────────────────────────────────────────┤
│  Sol Engine (C99, libsol.so)                            │
│                                                          │
│  audio/   ─── AudioNode, OscillatorNode, VoiceNode,     │
│              EnvelopeNode, MixerNode, GainNode,          │
│              AudioPipeline (control queue, probes)       │
│  io/      ─── SDL3 audio callback, ALSA audio callback  │
│  scene/   ─── Control, DrawList (for GUI rendering)     │
└──────────────────────────────────────────────────────────┘
```

---

## 1. Project Layout

```
neptune/
├── __init__.py
├── main.py                   # entry point: --headless / --midi / --gui
├── gui/
│   ├── __init__.py
│   └── synth_panel.py        # Sol Control tree consuming probe data
├── input/
│   ├── __init__.py
│   ├── keyboard.py           # ComputerKeyboardInput → NoteOn/NoteOff
│   └── midi.py               # MidiInput → NoteOn/NoteOff
├── patch.py                  # save_patch / load_patch (JSON)
└── examples/
    └── extending/
        ├── my_first_voice.py     # copy-paste-and-modify starter
        └── my_first_effect.py    # same, for AudioNode extensions
```

---

## 2. Entry Point (`main.py`)

```python
"""Neptune — Audio Synthesizer powered by Sol Engine."""
import argparse
from sol.audio_bindings import AudioPipeline, MixerNode, VoiceNode, OscillatorNode, EnvelopeNode, GainNode

def build_default_patch(pipeline: AudioPipeline) -> None:
    mixer = MixerNode(name="main")
    gain = GainNode(level=0.8, name="output")

    voice = VoiceNode(note=60, velocity=0.8, name="lead")
    voice.set_oscillator(OscillatorNode.sine(freq=261.6))
    voice.set_envelope(EnvelopeNode.adsr(attack=0.01, decay=0.15, sustain=0.7, release=0.3))

    mixer.add_child(voice)
    mixer.add_child(gain)
    pipeline.set_root(mixer)

def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("--headless", action="store_true")
    parser.add_argument("--midi", action="store_true")
    parser.add_argument("--gui", action="store_true")
    parser.add_argument("--patch", type=str, help=".solpatch file to load")
    args = parser.parse_args()

    pipeline = AudioPipeline(sample_rate=44100, buffer_size=512)

    if args.patch:
        from neptune.patch import load_patch
        load_patch(pipeline, args.patch)
    else:
        build_default_patch(pipeline)

    pipeline.start()

    if args.gui:
        from neptune.gui.synth_panel import SynthPanel
        panel = SynthPanel(pipeline)
        panel.show()
    else:
        # Headless — keyboard input
        from neptune.input.keyboard import ComputerKeyboardInput
        kbd = ComputerKeyboardInput()
        kbd.on_note_on = pipeline.send_note_on
        kbd.on_note_off = pipeline.send_note_off

        try:
            while True:
                kbd.poll()
        except KeyboardInterrupt:
            pass

    pipeline.stop()
```

---

## 3. Keyboard Input (`input/keyboard.py`)

Maps QWERTY keys to MIDI notes (piano-style: Z=X keyboard row = white keys, S=D etc = black keys). Yields `NoteOn(pitch, velocity)` / `NoteOff(pitch)` via callbacks into `AudioPipeline.send_note_on()` / `send_note_off()`.

No threading — polls from the main loop. The callbacks push into `AudioPipeline`'s control queue, which the audio thread drains at the top of each callback.

---

## 4. MIDI Input (`input/midi.py`)

Uses `mido` + `python-rtmidi`. Opens a background thread that blocks on `port.receive()` and pushes `NoteOn`/`NoteOff` into the `AudioPipeline` control queue.

---

## 5. GUI Panel (`gui/synth_panel.py`)

A `Control` tree rendered by Sol's `SceneTree`:

```
Panel (VBoxContainer)
├── Header (Label: "Neptune Synth")
├── Pipeline View (HBoxContainer)
│   ├── OscillatorBox (ColorRect + Label + Knob)
│   ├── FilterBox    (ColorRect + Label + Knob)
│   └── GainBox      (ColorRect + Label + Knob)
└── Waveform View (custom Control polling probe data)
```

The waveform view calls `pipeline.get_probe_data(node_index, n_samples)` at ~30Hz and draws waveform lines via `DrawList` primitives (or direct vertex buffer updates).

Knob widgets call `pipeline.send_set_param(node_name, param, value)` on mouse drag — which enqueues a control message for the audio thread.

---

## 6. Patch Persistence (`patch.py`)

```python
def save_patch(pipeline: AudioPipeline, path: str) -> None:
    """Serialize the AudioNode tree to a .solpatch JSON file."""
    ...

def load_patch(pipeline: AudioPipeline, path: str) -> None:
    """Deserialize a .solpatch JSON file into an AudioNode tree."""
    ...
```

Format:

```json
{
  "sample_rate": 44100,
  "root": {
    "type": "MixerNode",
    "name": "main",
    "params": {},
    "children": [
      {
        "type": "VoiceNode",
        "name": "lead",
        "params": { "note": 60, "velocity": 0.8 },
        "children": [
          { "type": "OscillatorNode",
            "params": { "waveform": "SINE", "freq": 261.63, "amp": 1.0 }},
          { "type": "EnvelopeNode",
            "params": { "attack": 0.01, "decay": 0.15, "sustain": 0.7, "release": 0.3 }}
        ]
      },
      { "type": "GainNode", "params": { "level": 0.8 }}
    ]
  }
}
```

Each `AudioNode` subclass exposes `get_params() -> dict` and `set_param(name, value)` — also used for GUI introspection. A registry (`dict[str, Type[AudioNode]]`) maps `"type"` strings to constructors. New node types register via a one-line decorator: `@register_audio_node("MyEffect")`.

---

## 7. Dependencies

```
uv add mido python-rtmidi
```

Raspberry Pi: `sudo apt install librtmidi-dev libasound2-dev` before `uv sync`.

`sounddevice` is NOT a dependency — audio I/O is handled by Sol's C engine via SDL3 or ALSA.

---

## 8. v2 Acceptance Criteria

1. `uv run neptune/main.py --headless` opens audio via ALSA. QWERTY keys play sine tones with no glitching.
2. `--midi` flag switches input source to USB MIDI device. Same audio path, different input source.
3. The pipeline is `MixerNode → VoiceNode(OscillatorNode + EnvelopeNode) → GainNode` and produces polyphonic sound.
4. `--gui` flag opens a Sol-rendered window with probe waveform display and parameter knobs.
5. `--patch my_sound.solpatch` loads a serialized AudioNode tree.
6. `examples/extending/my_first_voice.py` is <40 lines and produces sound when run.

---

## 9. Explicit Non-Goals for v2

- No plugin auto-discovery — explicit imports + `@register_audio_node` decorator.
- No stereo or multi-output routing — mono, single output.
- No custom DSP language or visual patcher — Python code is the patcher.
- No packaging/distribution beyond `uv run` from source.

---

*Specification version 2.0 — Neptune Synth (Sol Audio Engine composition layer)*
