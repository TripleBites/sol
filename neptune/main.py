#!/usr/bin/env python3
"""Neptune — Playable Audio Synthesizer (Sol Engine)

QWERTY keyboard → MIDI notes → VoiceNode → ADSR → ALSA output.

  Z X C V B N M ,  = white keys (C4–C5)
  S D   G H J      = black keys (C#4–A#4)
  SPACE             = release all
  ESC               = quit

Usage:
    PYTHONPATH=src python3 neptune/main.py
"""
import time

from sol.audio_bindings import (
    AudioPipeline,
    MixerNode,
    VoiceNode,
    EnvelopeNode,
    OscillatorNode,
    GainNode,
)
from neptune.input.keyboard import KeyboardInput


def build_voice(sample_rate: int, note: int):
    """Build a VoiceNode subtree for one MIDI note."""
    voice = VoiceNode(note, velocity=0.8, sample_rate=sample_rate)
    env = EnvelopeNode.adsr(
        attack=0.02, decay=0.2, sustain=0.7, release=0.4, sr=sample_rate
    )
    osc = OscillatorNode.sine(VoiceNode.midi_to_freq(note), amp=0.5)
    voice.add_child(env)
    voice.add_child(osc)
    return voice


def main():
    print("=" * 50)
    print("  Neptune — Playable QWERTY Synth")
    print("=" * 50)
    print()
    print("  Z X C V B N M ,  = white keys")
    print("  S D   G H J      = black keys")
    print("  SPACE = release   ESC = quit")
    print()

    sample_rate = 44100
    pipeline = AudioPipeline(sample_rate=sample_rate, buffer_size=256)

    mixer = MixerNode()
    mixer.set_name("main")

    gain = GainNode(0.4)
    gain.set_name("master")

    gain.add_child(mixer)
    pipeline.set_root(gain)

    # Voice pool — create on demand
    voices = {}   # midi_note → (voice, VoiceNode)

    def on_note_on(note: int, velocity: float):
        if note in voices:
            return  # already playing
        voice = build_voice(sample_rate, note)
        mixer.add_child(voice)
        pipeline._ptr  # make sure pipeline has pointers

        # Re-register the note node (set_root was already called on gain,
        # but we added a child dynamically — need to register it)
        from sol.audio_bindings import _audio_pipeline_register_node
        voice_id = _audio_pipeline_register_node(pipeline._ptr, voice.ptr)
        voice._owned = True

        pipeline.send_note_on(voice_id, note, velocity)
        voices[note] = (voice_id, voice)
        print(f"\r  ♪ Note {note} ON  ", end="", flush=True)

    def on_note_off(note: int):
        if note not in voices:
            return
        voice_id, _ = voices.pop(note)
        pipeline.send_note_off(voice_id)
        print(f"\r  ♪ Note {note} OFF ", end="", flush=True)

    print("[neptune] Starting audio...")
    pipeline.start()

    kbd = KeyboardInput(on_note_on, on_note_off)
    kbd.start()

    print("[neptune] Ready — play with QWERTY keys!")
    print()

    try:
        while kbd._running:
            kbd.poll()
            time.sleep(0.005)  # 5ms poll = 200Hz
    except KeyboardInterrupt:
        pass
    finally:
        kbd.stop()
        pipeline.stop()

    print("\n[neptune] Done.")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
