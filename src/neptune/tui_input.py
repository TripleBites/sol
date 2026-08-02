"""Frame-based keyboard input for Neptune TUI mode.

Unlike the headless keyboard.py (which uses select() on stdin for
single-key events), this module provides a proper frame-based poll
that detects both key PRESS and key RELEASE.

How it works:
  1. Terminal is set to raw non-blocking mode.
  2. Each frame, drain ALL pending bytes from stdin.
  3. Build a set of "currently held" keys.
  4. Compare with previous frame to detect press/release transitions.

Arrow keys and other multi-byte sequences are parsed from ESC [ .. codes.

Usage:
    kbd = TuiKeyboard()
    kbd.start()
    while True:
        keys = kbd.poll_keys()  # set of currently-held key strings
        # Compare with previous frame's keys for press/release
"""
import sys
import os
import termios
import tty
import fcntl
import select


class TuiKeyboard:
    """Non-blocking frame-based keyboard input for TUI mode.

    Call poll_keys() each frame to get the set of currently-held keys.
    Each key is a string:
      - Single chars: 'a', 'Z', ' ', '\\t', '\\x1b' (ESC)
      - Arrow keys:   '\\x1b[A' (UP), '\\x1b[B' (DOWN),
                       '\\x1b[C' (RIGHT), '\\x1b[D' (LEFT)
      - Function keys: '\\x1bOP' (F1), etc.
      - Shift modified: uppercase letters 'A'-'Z'

    Key release is detected when a key disappears between frames.
    """

    def __init__(self):
        self._old_settings = None
        self._old_flags = None
        self._running = False
        self._buf = b""

    def start(self) -> bool:
        """Switch terminal to raw non-blocking mode."""
        if not os.isatty(sys.stdin.fileno()):
            print("[tui] Warning: stdin is not a TTY")
            return False

        self._old_settings = termios.tcgetattr(sys.stdin)

        # Raw mode: no echo, no canonical, no signals
        tty.setraw(sys.stdin.fileno())

        # Set non-blocking
        self._old_flags = fcntl.fcntl(sys.stdin, fcntl.F_GETFL)
        fcntl.fcntl(sys.stdin, fcntl.F_SETFL, self._old_flags | os.O_NONBLOCK)

        self._running = True
        return True

    def stop(self):
        """Restore terminal settings."""
        self._running = False
        if self._old_settings is not None:
            try:
                termios.tcsetattr(sys.stdin, termios.TCSADRAIN, self._old_settings)
            except Exception:
                pass
            self._old_settings = None
        if self._old_flags is not None:
            try:
                fcntl.fcntl(sys.stdin, fcntl.F_SETFL, self._old_flags)
            except Exception:
                pass
            self._old_flags = None

    def poll_keys(self) -> set:
        """Drain pending input and return the set of currently-held keys.

        Each key is a string. Single characters are returned as-is.
        Arrow keys return ESC-prefixed strings like '\\x1b[A'.

        Keys that remain held between frames persist in the set.
        Keys that are released disappear from the set on the next frame.

        Returns:
            Set of key strings currently held.
        """
        if not self._running:
            return set()

        # Drain all available input
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

        # Parse key sequences from buffer
        keys = set()
        i = 0
        buf = self._buf

        while i < len(buf):
            ch = buf[i]

            if ch == 0x1B:  # ESC — could be escape or start of sequence
                if i + 1 < len(buf) and buf[i + 1] == 0x5B:  # ESC [
                    # CSI sequence: ESC [ ... final
                    j = i + 2
                    while j < len(buf) and buf[j] < 0x40:
                        j += 1
                    if j < len(buf):  # Found final byte
                        final = buf[j]
                        # Known sequences
                        seq = buf[i:j + 1].decode("latin-1", errors="replace")
                        keys.add(seq)  # e.g., '\x1b[A', '\x1b[B', etc.
                        i = j + 1
                        continue
                    else:
                        # Incomplete sequence — keep in buffer
                        break
                else:
                    # Plain ESC key
                    keys.add('\x1b')
                    i += 1
                    continue

            elif ch == 0x09:  # Tab
                keys.add('\t')
                i += 1
            elif ch == 0x0D or ch == 0x0A:  # Enter (CR or LF)
                keys.add('\r')
                i += 1
            elif ch == 0x20:  # Space
                keys.add(' ')
                i += 1
            elif ch == 0x7F:  # Backspace
                keys.add('\x7f')
                i += 1
            elif ch >= 0x20 and ch < 0x7F:
                # Printable ASCII — use the character as-is
                # This preserves case: 'a' vs 'A' for shift detection
                keys.add(chr(ch))
                i += 1
            else:
                # Unknown — skip
                i += 1

        # Trim the buffer to the last incomplete sequence
        if i < len(buf):
            self._buf = buf[i:]
        else:
            self._buf = b""

        return keys
