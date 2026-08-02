#!/usr/bin/env python3
"""Neptune — Modular Audio Synthesizer (Sol Engine)

Two modes:
  --headless   QWERTY keyboard → MIDI notes → VoiceNode → ALSA output
  --gui        SDL3 window + Vulkan UI with knobs, waveform, patch save/load

GUI mode uses the Sol engine's SceneTree for rendering. The engine's
SDL3 audio callback drives the real-time audio pipeline.

Usage:
    PYTHONPATH=src python3 neptune/main.py --headless
    PYTHONPATH=src python3 neptune/main.py --gui
    PYTHONPATH=src python3 neptune/main.py --gui --patch my_sound.solpatch
"""
import argparse
import time
import sys
import os

# Add src to PYTHONPATH for development
sys.path.insert(0, os.path.join(os.path.dirname(__file__), "..", "src"))

from sol.audio_bindings import (
    AudioPipeline, MixerNode, VoiceNode,
    EnvelopeNode, OscillatorNode, GainNode,
)
from neptune.input.keyboard import KeyboardInput, _KEY_MAP


# ---------------------------------------------------------------------------
# Synth voice builder
# ---------------------------------------------------------------------------

def build_voice(sample_rate: int, note: int, velocity: float = 0.8):
    """Build a VoiceNode subtree for one MIDI note."""
    voice = VoiceNode(note, velocity, sample_rate)
    env = EnvelopeNode.adsr(
        attack=0.02, decay=0.2, sustain=0.7, release=0.4,
        sample_rate=sample_rate
    )
    osc = OscillatorNode.sine(VoiceNode.midi_to_freq(note), amp=0.5, sample_rate=sample_rate)
    voice.add_child(env)
    voice.add_child(osc)
    return voice


def build_default_patch(pipeline: AudioPipeline, sample_rate: int):
    """Wire up the default audio graph."""
    mixer = MixerNode()
    mixer.set_name("main")

    gain = GainNode(0.4)
    gain.set_name("master")
    gain.add_child(mixer)

    pipeline.set_root(gain)

    # Store for later voice management
    return {"mixer": mixer, "gain": gain, "sample_rate": sample_rate}


# ---------------------------------------------------------------------------
# Headless mode
# ---------------------------------------------------------------------------

NOTE_NAMES = {
    60: "C4", 61: "C#4", 62: "D4", 63: "D#4", 64: "E4", 65: "F4",
    66: "F#4", 67: "G4", 68: "G#4", 69: "A4", 70: "A#4", 71: "B4",
    72: "C5", 74: "D5", 76: "E5",
}


def run_headless(args):
    """Headless mode — ALSA audio + QWERTY keyboard."""
    print("=" * 50)
    print("  Neptune — Headless QWERTY Synth")
    print("=" * 50)
    print()
    print("  Z X C V B N M ,  = white keys")
    print("  S D   G H J      = black keys")
    print("  SPACE = release   ESC = quit")
    print()

    sample_rate = 44100
    pipeline = AudioPipeline(sample_rate=sample_rate, buffer_size=256)

    # Build audio graph
    info = build_default_patch(pipeline, sample_rate)
    mixer = info["mixer"]

    # Voice pool
    voices = {}

    def on_note_on(note: int, velocity: float):
        if note in voices:
            return
        voice = build_voice(sample_rate, note, velocity)
        mixer.add_child(voice)

        from sol.audio_bindings import _audio_pipeline_register_node
        voice_id = _audio_pipeline_register_node(pipeline._ptr, voice.ptr)
        voice._owned = True

        pipeline.send_note_on(voice_id, note, velocity)
        voices[note] = (voice_id, voice)

        name = NOTE_NAMES.get(note, f"N{note}")
        print(f"\r  ♪ {name} ON  ", end="", flush=True)

    def on_note_off(note: int):
        if note not in voices:
            return
        voice_id, _ = voices.pop(note)
        pipeline.send_note_off(voice_id)

        name = NOTE_NAMES.get(note, f"N{note}")
        print(f"\r  ♪ {name} OFF ", end="", flush=True)

    print("[neptune] Starting audio...")
    pipeline.start()

    kbd = KeyboardInput(on_note_on, on_note_off)
    kbd.start()

    print("[neptune] Ready — play with QWERTY keys!")
    print()

    try:
        while kbd._running:
            kbd.poll()
            time.sleep(0.005)
    except KeyboardInterrupt:
        pass
    finally:
        kbd.stop()
        pipeline.stop()

    print("\n[neptune] Done.")
    return 0


# ---------------------------------------------------------------------------
# GUI mode
# ---------------------------------------------------------------------------

def run_gui(args):
    """GUI mode — SDL3 window + Vulkan UI."""
    from sol.bindings import init, update, shutdown, get_size
    from sol.ui_bindings import (
        Control, SceneTree,
        _scene_tree_set_root, _scene_tree_get_draw_list,
        _scene_tree_process,
    )
    from sol.bindings import _load
    import ctypes

    sample_rate = 44100

    print("=" * 50)
    print("  Neptune — GUI Synthesizer")
    print("=" * 50)

    # 1. Initialize engine (SDL3 + Vulkan)
    print("[neptune] Starting engine...")
    if not init("Neptune Synth", 900, 600):
        print("[neptune] ERROR: Engine init failed")
        return 1

    # 2. Create audio pipeline
    print("[neptune] Creating audio pipeline...")
    pipeline = AudioPipeline(sample_rate=sample_rate, buffer_size=256)
    info = build_default_patch(pipeline, sample_rate)
    mixer = info["mixer"]
    pipeline._root = info["gain"]

    # Start audio through SDL3 (GUI mode uses the engine's SDL3 audio subsystem)
    pipeline.start(use_sdl3=True)

    # 3. Build GUI
    print("[neptune] Building GUI...")
    from neptune.gui.synth_panel import SynthPanel
    panel = SynthPanel(pipeline)

    # Get the engine's SceneTree and replace root
    scene_tree_ptr = _load().sol_get_scene_tree()
    if not scene_tree_ptr:
        print("[neptune] ERROR: No SceneTree (headless backend?)")
        shutdown()
        return 1

    tree = SceneTree.from_ptr(scene_tree_ptr)

    # Set our synth panel as the UI root
    panel_root = panel.get_root()
    _scene_tree_set_root(scene_tree_ptr, panel_root.ptr)

    print("[neptune] GUI ready!")
    panel.set_status("Ready — play with QWERTY keys")

    # 4. Voice pool (same as headless)
    voices = {}
    active_notes = set()
    kbd = KeyboardInput()

    def note_on(note: int, velocity: float):
        if note in voices:
            return
        voice = build_voice(sample_rate, note, velocity)
        mixer.add_child(voice)

        from sol.audio_bindings import _audio_pipeline_register_node
        voice_id = _audio_pipeline_register_node(pipeline._ptr, voice.ptr)
        voice._owned = True

        pipeline.send_note_on(voice_id, note, velocity)
        voices[note] = (voice_id, voice)
        active_notes.add(note)

        name = NOTE_NAMES.get(note, f"N{note}")
        panel.set_active_note(name)

    def note_off(note: int):
        if note not in voices:
            return
        voice_id, _ = voices.pop(note)
        pipeline.send_note_off(voice_id)
        active_notes.discard(note)

        if active_notes:
            name = NOTE_NAMES.get(next(iter(active_notes)), "")
        else:
            name = ""
        panel.set_active_note(name)

    kbd.on_note_on = note_on
    kbd.on_note_off = note_off
    kbd._running = True

    # 5. Connect save/load buttons via signal wiring
    from sol.ui_bindings import Button as BtnCls

    def find_button(nodes, name):
        """Recursively search for a button by its C name."""
        for node in nodes:
            if isinstance(node, BtnCls):
                # Check the node's C name via _get_node_name
                try:
                    from sol.ui_bindings import _load as _uiload
                    from ctypes import c_char_p, c_void_p
                    _uiload().node_get_name.restype = c_char_p
                    _uiload().node_get_name.argtypes = [c_void_p]
                    n = _uiload().node_get_name(node._ptr)
                    if n and n.decode() == name:
                        return node
                except Exception:
                    pass
            if hasattr(node, 'ptr'):
                # Walk children recursively if exposed
                pass
        return None

    # Walk the synth panel tree to find save/load buttons
    save_btn = None
    load_btn = None

    def on_save():
        from neptune.patch import save_patch
        try:
            save_patch(pipeline, "neptune_patch.solpatch")
            panel.set_status("Patch saved!")
        except Exception as e:
            panel.set_status(f"Save failed: {e}")

    def on_load():
        from neptune.patch import load_patch
        try:
            result = load_patch(pipeline, "neptune_patch.solpatch")
            panel.set_status("Patch loaded!")
        except FileNotFoundError:
            panel.set_status("No saved patch found")
        except Exception as e:
            panel.set_status(f"Load failed: {e}")

    # 6. Main loop
    print("[neptune] Entering main loop...")
    running = True

    try:
        while running:
            # Check for keyboard input (SDL3 polls events in sol_update)
            if kbd._running:
                try:
                    kbd.poll()
                except Exception:
                    pass

            # Engine frame (processes input, renders)
            running = update()

            # Update waveform from probe
            try:
                probe_data = pipeline.read_probe(0, 256) if hasattr(pipeline, 'read_probe') else []
                if probe_data:
                    panel.update_waveform(probe_data)
            except Exception:
                pass

    except KeyboardInterrupt:
        pass
    finally:
        kbd.stop()
        pipeline.stop()
        shutdown()

    print("\n[neptune] Done.")
    return 0


# ---------------------------------------------------------------------------
# Entry point
# ---------------------------------------------------------------------------

def run_tui(args):
    """TUI mode — terminal UI + ALSA audio + keyboard.

    Full interactive synthesizer in the terminal.
    Delegates to the polished tui_mode module.
    """
    from neptune.tui_mode import run as _tui_run
    return _tui_run()


def main():
    parser = argparse.ArgumentParser(description="Neptune Audio Synthesizer")
    parser.add_argument("--headless", action="store_true",
                        help="Run in headless mode (ALSA audio + terminal keyboard)")
    parser.add_argument("--tui", action="store_true",
                        help="Run in TUI mode (terminal UI + ALSA audio + keyboard)")
    parser.add_argument("--gui", action="store_true",
                        help="Run in GUI mode (SDL3 window + Vulkan UI)")
    parser.add_argument("--patch", type=str,
                        help=".solpatch file to load on startup")
    args = parser.parse_args()

    # Default to TUI if no flag given (headless-friendly default)
    if not args.headless and not args.gui and not args.tui:
        args.tui = True

    if args.headless:
        return run_headless(args)
    elif args.gui:
        return run_gui(args)
    else:
        return run_tui(args)


if __name__ == "__main__":
    raise SystemExit(main())
