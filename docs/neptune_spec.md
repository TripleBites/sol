# Modular Python Synthesizer — Project Specification

## 0. Purpose of this document

This is a spec to hand to a coding agent (or to build from yourself) to bootstrap
v1 of the project. It intentionally specifies **less code than it could** — the
goal is a small, readable core that a Python learner (comfortable in Unity C#)
can read top-to-bottom in an afternoon, then extend. Every extension point is
called out explicitly so growth has an obvious place to go, rather than
requiring a rewrite.

Optimize for: readability > cleverness, explicit > implicit, small interfaces >
big frameworks. Resist the urge to add abstraction the spec doesn't ask for —
the "modular" parts are modular on purpose; everything else should stay as
plain and flat as possible.

---

## 1. Background / prior art this builds on

This project extends the pattern from the "Making a Synth With Python" article
series (Oscillators → Modulators → Controllers):

- **Oscillators are iterators.** Each oscillator implements `__iter__` /
  `__next__`. `__iter__` resets/starts the note (called once on key-down);
  `__next__` produces the next sample (called repeatedly while the key is
  held). Oscillators track a fundamental value (e.g. `_freq`, set at
  instantiation / note-on) separately from a live value (e.g. `_f`) that
  modulation can move without losing the original.
- **Modulators wrap oscillator parameters.** A modulator (e.g. an ADSR
  envelope, an LFO) feeds a value each tick; a small modifier function
  combines the modulator's output with the oscillator's base parameter to
  produce the new live value.
- **A controller/synth layer reads note events (MIDI or keyboard) and drives
  oscillators + modulators together**, mixing multiple simultaneous voices
  (polyphony).

We are keeping this mental model (it's good — it's basically an ECS-flavored
signal graph) but reorganizing it into **explicit modular stages** with a
defined interface between each, so new stages can be dropped in without
touching existing code, and so the signal at any stage can be probed for
visualization.

---

## 2. High-level architecture

```
 ┌──────────────┐      ┌────────────────┐      ┌───────────────┐      ┌──────────┐
 │  Input Layer  │ ---> │  Voice Layer   │ ---> │  Mix / FX      │ ---> │  Output   │
 │ (MIDI / kbd)  │      │ (osc + mod per │      │  Pipeline      │      │ (speaker  │
 │               │      │  active note)  │      │ (chain of      │      │  via      │
 │               │      │                │      │  Nodes)        │      │ sounddevice)│
 └──────────────┘      └────────────────┘      └───────────────┘      └──────────┘
        |                                              |
        |                                              | (probe taps, optional)
        v                                              v
 ┌────────────────────────────────────────────────────────────────────────────┐
 │                     Engine / Control thread (GUI, optional)                 │
 │        reads probe data + pipeline structure, writes control messages       │
 └────────────────────────────────────────────────────────────────────────────┘
```

Two runtime modes, same core:
- **Headless mode**: Input Layer → Voice Layer → Mix/FX Pipeline → Output.
  No GUI, no engine thread. This is what she plays through on the Pi.
- **GUI mode**: same audio path running on its own thread, *plus* your Godot-
  like engine's thread running a UI that visualizes the pipeline and lets her
  hot-swap/probe it live.

The audio path never depends on the GUI being present. The GUI is a consumer
that attaches to a stable interface (Section 6) — this is the seam that keeps
"headless" and "GUI" the same underlying app instead of two apps.

---

## 3. Threading & real-time constraints (read this before writing any code)

This is the one part of the system where getting the boundary wrong causes
audible glitches, so the coding agent should treat this section as load-
bearing, not stylistic.

- **The audio callback (`sounddevice` calls this many times per second, e.g.
  every ~11ms at a 512-sample buffer / 44.1kHz) runs on a thread PortAudio
  owns, not your engine's thread.** This is not a design choice — it's how
  real-time audio works on every io.
- **The audio callback must never:** allocate significant memory, take a lock
  that the GUI/engine thread might hold for a while, do file or network I/O,
  or call into arbitrary Python objects it doesn't already own. Missing a
  deadline = an audible click/dropout.
- **The engine/GUI thread must never block on the audio thread.** It reads
  state the audio thread published, and it *requests* changes — it does not
  reach into the audio thread's objects and mutate them directly.
- **The only two crossings allowed between the threads:**
  1. **Control queue** (GUI/input → audio): a `queue.Queue` (or
     `collections.deque` with `maxlen`) of small immutable messages — "note
     on", "note off", "set param X to Y", "swap node at slot N". The audio
     callback drains it (non-blocking `get_nowait()` in a loop) at the top of
     each callback, before rendering.
  2. **Probe ring buffer** (audio → GUI): a fixed-size numpy ring buffer per
     probe point. Audio thread writes; GUI thread reads. Single-writer /
     single-reader on a preallocated fixed-size array needs no lock if
     writes are index-based (write pointer only ever advanced by the audio
     thread) — use `numpy` array + plain int index, not a `Queue`, since the
     GUI only needs "the last N samples," not delivery of every sample.
- **Hot-swapping a pipeline node must be crash-safe under this model too**:
  the audio thread should only ever see a fully-constructed replacement node
  (swap a reference atomically, e.g. replace an entry in a list the audio
  thread reads by index — Python list `__setitem__` is atomic under the GIL)
  — never partially-constructed state.

This whole boundary should live in **one small file** (`src/rtsafe.py` or
similar) with the control-queue and probe-ring-buffer implementations and
nothing else, so it's the one place to look to understand "how the two
threads talk," and it's short enough to actually read.

---

## 4. Project layout

```
synth/
├── pyproject.toml              # uv-managed
├── README.md                   # points here + quickstart
├── SYNTH_SPEC.md                # this file
├── src/
│   └── synth/
│       ├── __init__.py
│       ├── main.py             # entrypoint: --headless / --gui flag
│       ├── rtsafe.py           # control queue + probe ring buffer (Section 3)
│       ├── oscillators/
│       │   ├── __init__.py
│       │   ├── base.py         # Oscillator ABC (iterator protocol)
│       │   ├── sine.py
│       │   ├── square.py
│       │   └── saw.py
│       ├── modulators/
│       │   ├── __init__.py
│       │   ├── base.py         # Modulator ABC
│       │   └── adsr.py         # simple ADSR envelope
│       ├── voices/
│       │   ├── __init__.py
│       │   └── voice.py        # one active note = osc(s) + modulator(s)
│       ├── pipeline/
│       │   ├── __init__.py
│       │   ├── node.py         # Node ABC — the "modular pipeline stage" interface
│       │   ├── graph.py        # ordered list of Nodes + hot-swap + probes
│       │   └── nodes/
│       │       ├── mixer.py    # sums active voices into one buffer
│       │       └── gain.py     # trivial example transformer node
│       ├── input/
│       │   ├── __init__.py
│       │   ├── base.py         # InputSource ABC: yields NoteOn/NoteOff events
│       │   ├── computer_keyboard.py
│       │   └── midi.py
│       ├── io/
│       │   ├── __init__.py
│       │   └── audio_out.py    # sounddevice stream setup + the callback
│       ├── persistence/
│       │   ├── __init__.py
│       │   └── patch.py        # save/load pipeline+settings as a file (Section 8)
│       └── gui/
│           ├── __init__.py
│           └── app.py          # generic GUI interface (Section 9) — engine-agnostic
├── examples/
│   └── extending/
│       ├── my_first_oscillator.py   # copy-paste-and-modify starter
│       └── my_first_node.py         # same, for pipeline nodes
└── tests/
    ├── test_oscillators.py
    ├── test_pipeline.py
    └── test_rtsafe.py
```

Rationale for the flat, plural-folder-per-concept layout: it mirrors your
`Grimoire`-style modular workspace habits, but flattened for a Python
beginner — one obvious folder per kind of extensible thing, and each folder's
`base.py` is the contract new modules must satisfy.

---

## 5. Core interfaces (the contracts extensions implement)

Keep these as small as possible. Each is an `abc.ABC` with 1–3 required
methods, documented with a docstring showing a minimal example implementation
directly in the ABC's docstring (so she can read the interface and see how to
satisfy it in the same place).

### 5.1 `Oscillator` (src/synth/oscillators/base.py)
- Iterator protocol: `__iter__(self)` (called on note-on, resets phase),
  `__next__(self)` (returns one `float` sample in `[-1.0, 1.0]`).
- Constructor takes `freq: float`, `sample_rate: int`.
- Properties: `freq` (fundamental, immutable post-construction) vs an
  internal live frequency modulators can move.

### 5.2 `Modulator` (src/synth/modulators/base.py)
- `__iter__(self)`, `__next__(self) -> float`: yields the next modulation
  value (e.g. envelope amplitude 0.0–1.0).
- A modulator does not know what it's modulating — that composition happens
  in `Voice` or via a small `apply(base_value, mod_value) -> float` function
  passed in, per the original tutorial's `[param]_mod` pattern.

### 5.3 `Node` (src/synth/pipeline/node.py) — **the key modular extension point**
This is what "add new sound mixers and transformers over time" hooks into.
```python
class Node(ABC):
    """
    One stage in the audio pipeline. Given a numpy buffer of input samples
    (shape: (n_frames,) mono for v1), produce a buffer of output samples of
    the same shape. Must be safe to call from the audio thread: no
    allocation-heavy work, no locks, no I/O.
    """
    name: str  # for GUI display / probing labels

    @abstractmethod
    def process(self, buffer: np.ndarray) -> np.ndarray: ...

    def get_params(self) -> dict: ...       # for GUI to introspect/display
    def set_param(self, name, value): ...   # called via control queue only
```
A `Graph` (`pipeline/graph.py`) holds an ordered list of `Node`s, runs them in
sequence each callback, and exposes:
- `probe(node_index)` — registers a ring-buffer tap after that node's output
  (Section 3), used by the GUI.
- `swap_node(index, new_node)` — hot-swap, via the control queue, never
  direct mutation from another thread.
- `insert_node`, `remove_node` — same swap-safety rules.

This gives her a single, small, well-documented base class to inherit from
for *any* new pipeline stage — a new mixer, a filter, a distortion effect,
whatever — without needing to understand oscillators or MIDI at all.

### 5.4 `InputSource` (src/synth/input/base.py)
- Something that yields `NoteOn(pitch, velocity)` / `NoteOff(pitch)` events
  (simple dataclasses) — either by polling or via a background thread that
  pushes into the control queue.
- Two v1 implementations:
  - `ComputerKeyboardInput` — maps a few QWERTY keys to notes (piano-style
    row), zero extra hardware needed to test with.
  - `MidiInput` — wraps a MIDI library (Section 7) and translates MIDI
    note-on/off/velocity into the same `NoteOn`/`NoteOff` events, so
    everything downstream is input-source-agnostic.

---

## 6. The audio thread ↔ GUI interface (the seam your engine plugs into)

Since the GUI itself is out of scope for the coding agent right now (you're
wiring your own engine's UI system in afterward), the spec defines a small,
concrete, **engine-agnostic** data interface. Whatever UI toolkit consumes
this only needs to know this interface — nothing about `sounddevice`,
oscillators, or threading:

```python
class SynthHandle:
    """
    The one object a GUI needs. Constructed once; safe to poll from any
    thread that isn't the audio callback itself.
    """
    def get_pipeline_description(self) -> list[NodeInfo]: ...
        # name, type, current params, per node — for drawing the pipeline

    def get_probe_data(self, node_index: int, n_samples: int) -> np.ndarray:
        # last N samples at that probe point, for waveform drawing

    def send_control(self, message: ControlMessage) -> None:
        # enqueue a set-param / swap-node / note-on-off request

    def list_probe_points(self) -> list[str]: ...
```

Your engine's UI code then just needs to: call `get_pipeline_description()`
to draw boxes for each node, call `get_probe_data()` on a timer to draw a
waveform, and call `send_control()` on button/knob interaction. This keeps
your engine integration to "one adapter file that imports `SynthHandle`,"
written after this bootstrap, not as part of it.

---

## 7. MIDI input: tradeoffs (per your question)

Two realistic options for reading a USB MIDI keyboard on a Pi:

- **`mido` + `python-rtmidi`** — the standard choice. `mido` gives a clean,
  Pythonic message API (`msg.type`, `msg.note`, `msg.velocity`); `rtmidi` is
  the C++ backend that actually talks to ALSA/CoreMIDI/etc. This is what
  almost every Python MIDI tutorial and project uses; well documented, well
  maintained, works out of the box on Raspberry Pi OS (needs `librtmidi-dev`
  at the system level — one `apt` line — plus the two `uv add` packages).
  **Recommended** — it's the path of least resistance and most tutorials/
  Stack Overflow answers assume it.
- **`pygame.midi`** — works, but pygame's MIDI support is a thinner wrapper,
  less actively maintained, and pulls in all of pygame as a dependency for a
  feature you're only using a sliver of. Not recommended here.

Go with **`mido` + `python-rtmidi`**. A USB keyboard that identifies as a
class-compliant MIDI device will just show up as a MIDI port on Linux with no
driver install — `mido.get_input_names()` lists it.

---

## 8. Patch / pipeline persistence (Section for later, scope it now)

You mentioned this will "probably require" a save system — spec it now so
the `Node` interface is designed to support it from day one, even if the
save/load *feature* is v2.

Approach: since every `Node` already exposes `get_params()` (Section 5.3),
a patch file is just:
```json
{
  "pipeline": [
    {"type": "Mixer", "params": {}},
    {"type": "Gain", "params": {"level": 0.8}}
  ],
  "input_source": "midi",
  "sample_rate": 44100
}
```
`persistence/patch.py` provides `save_patch(graph, path)` /
`load_patch(path) -> Graph`, using a simple registry (`dict[str, Type[Node]]`)
that maps the `"type"` string to the actual class — new `Node` subclasses
register themselves (a one-line decorator: `@register_node("Gain")`), so
saving/loading automatically picks up modules she's written herself as long
as they're imported before load. This is the natural place a "modules and
extensions" system grows into without inventing a plugin framework up front.

**v1 scope**: implement the registry and `get_params()`/`set_param()` on
every node now (cheap, and it's also what the GUI introspection needs). Wire
up actual `save_patch`/`load_patch` as a fast-follow, not blocking v1.

---

## 9. GUI mode (generic interface only — see Section 6)

For this bootstrap, do **not** build a GUI. Build:
1. The `SynthHandle` interface (Section 6), fully implemented and tested
   against the headless audio path.
2. A **minimal** reference consumer as a `tests/`-style script — e.g. a
   script that polls `get_probe_data()` and dumps it to a `.wav` or prints
   RMS levels — just enough to prove the interface works end-to-end, without
   building real UI. This is the seam I'll help you wire your engine into
   once this is running.

---

## 10. Dependencies (uv)

```
uv add numpy sounddevice mido python-rtmidi
uv add --dev pytest
```
Raspberry Pi note: `python-rtmidi` needs system ALSA/JACK dev headers —
`sudo apt install librtmidi-dev libasound2-dev` before `uv sync` on the Pi.
`sounddevice` needs PortAudio — `sudo apt install libportaudio2`.

---

## 11. v1 acceptance criteria (what "bootstrap done" means)

1. `uv run synth --headless` opens an audio stream and lets you play notes
   via `ComputerKeyboardInput` (no MIDI hardware required to test) with a
   single sine oscillator voice, hearing sound with reasonable latency and
   no glitching, on both a normal dev machine and a Pi.
2. `--midi` flag switches input source to a connected USB MIDI device with
   no other code changes (input source is fully swappable via `InputSource`).
3. The pipeline is exactly two nodes (`Mixer` → `Gain`) to prove the `Node`
   interface and hot-swap path work, not because two is architecturally
   special.
4. `SynthHandle` is implemented and covered by a test that: starts the audio
   path, sends a note-on via `send_control`, asserts `get_probe_data`
   returns nonzero samples, sends a `set_param` to change gain, asserts the
   probed waveform amplitude changed.
5. `examples/extending/my_first_oscillator.py` and `my_first_node.py` exist,
   are short (<40 lines each), and are referenced from the README as "start
   here to add your own sound."
6. Every ABC (`Oscillator`, `Modulator`, `Node`, `InputSource`) has a
   docstring with a minimal worked example inline.
7. `tests/test_rtsafe.py` specifically tests the control-queue and
   ring-buffer under concurrent access (e.g. a background thread hammering
   writes while the main thread reads), since this is the one part of the
   system where a subtle bug would be genuinely hard to debug later.

---

## 12. Explicit non-goals for v1 (so the agent doesn't over-build)

- No plugin auto-discovery / dynamic import scanning — explicit imports +
  the `@register_node` decorator is enough for now.
- No stereo, effects sends, or multi-output routing — mono, single output.
- No actual GUI implementation — interface only (Section 9).
- No polyphony voice-stealing logic beyond "cap at N voices, drop oldest" —
  fine for v1, worth revisiting later.
- No packaging/distribution beyond `uv run` from source.
