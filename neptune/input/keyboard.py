"""Keyboard-to-MIDI input for Neptune (headless mode).

Maps QWERTY keys to MIDI notes using a piano-style layout:
  Z=X row = white keys (C4-B4)
  S=D row = black keys (C#4-A#4)

Polls stdin in raw mode for key events. Pushes note_on/note_off
callbacks when keys are pressed/released.

Usage:
    from neptune.input.keyboard import KeyboardInput
    kbd = KeyboardInput(on_note_on, on_note_off)
    kbd.start()
    ...
    kbd.poll()  # call each frame
"""
import sys
import os
import termios
import tty
import select
import threading


# Piano keyboard layout on QWERTY
_KEY_MAP = {
    # Lower row (white keys)
    "z": 60,  # C4
    "x": 62,  # D4
    "c": 64,  # E4
    "v": 65,  # F4
    "b": 67,  # G4
    "n": 69,  # A4
    "m": 71,  # B4
    ",": 72,  # C5

    # Upper row (black keys)
    "s": 61,  # C#4
    "d": 63,  # D#4
    "g": 66,  # F#4
    "h": 68,  # G#4
    "j": 70,  # A#4

    # Extra
    "q": 72,  # C5
    "w": 74,  # D5
    "e": 76,  # E5
}


class KeyboardInput:
    """Non-blocking keyboard input for headless mode."""

    def __init__(self, on_note_on=None, on_note_off=None):
        self.on_note_on = on_note_on
        self.on_note_off = on_note_off
        self._active_notes = {}  # key → midi_note
        self._old_settings = None
        self._running = False

    def start(self):
        """Switch terminal to raw mode for per-keystroke input."""
        if not os.isatty(sys.stdin.fileno()):
            return
        self._old_settings = termios.tcgetattr(sys.stdin)
        tty.setcbreak(sys.stdin.fileno())
        self._running = True

    def stop(self):
        """Restore terminal settings."""
        self._running = False
        if self._old_settings:
            termios.tcsetattr(sys.stdin, termios.TCSADRAIN, self._old_settings)
            self._old_settings = None

    def poll(self):
        """Check for pending key events. Call every frame."""
        if not self._running:
            return

        while True:
            # Non-blocking check for stdin
            ready, _, _ = select.select([sys.stdin], [], [], 0)
            if not ready:
                break

            ch = sys.stdin.read(1)
            if not ch:
                break

            key = ch.lower()

            # Check if it's a known note key
            if key in _KEY_MAP:
                note = _KEY_MAP[key]
                if key not in self._active_notes:
                    self._active_notes[key] = note
                    if self.on_note_on:
                        self.on_note_on(note, 0.8)

            # Any other key releases all active notes
            elif ch == " ":
                # Space releases all
                for key, note in list(self._active_notes.items()):
                    if self.on_note_off:
                        self.on_note_off(note)
                self._active_notes.clear()

            elif ch == "\x1b":  # ESC
                for key, note in list(self._active_notes.items()):
                    if self.on_note_off:
                        self.on_note_off(note)
                self._active_notes.clear()
                self._running = False

    def handle_key_up(self, key: str):
        """Simulate key release (for GUI mode where SDL3 provides events)."""
        key = key.lower()
        if key in self._active_notes:
            note = self._active_notes.pop(key)
            if self.on_note_off:
                self.on_note_off(note)

    def handle_key_down(self, key: str):
        """Simulate key press (for GUI mode)."""
        key = key.lower()
        if key in _KEY_MAP and key not in self._active_notes:
            note = _KEY_MAP[key]
            self._active_notes[key] = note
            if self.on_note_on:
                self.on_note_on(note, 0.8)
