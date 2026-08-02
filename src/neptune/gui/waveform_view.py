"""Python wrapper for the C WaveformView widget.

The C WaveformView draws a line strip from a float sample buffer.
We provide a Python-friendly interface that manages the sample data.
"""
import ctypes
from sol.ui_bindings import Node, Control, _load


class WaveformView(Control):
    """Displays an audio waveform as a line strip.

    Usage:
        wv = WaveformView()
        wv.set_data([0.1, 0.2, -0.1, ...])  # update each frame
    """

    def __init__(self):
        ptr = _load().waveform_view_new()
        Node.__init__(self, ptr)
        self._sample_array = None
        self._sample_ptr = None

    def set_data(self, samples: list):
        """Update the waveform with new sample data.

        Args:
            samples: List of float values in [-1.0, 1.0].
        """
        if not samples:
            return

        n = len(samples)

        # Reuse or allocate ctypes float array
        if self._sample_array is None or len(self._sample_array) < n:
            self._sample_array = (ctypes.c_float * n)()

        for i, s in enumerate(samples):
            self._sample_array[i] = max(-1.0, min(1.0, s))

        _load().waveform_view_set_data(
            self._ptr,
            ctypes.cast(self._sample_array, ctypes.POINTER(ctypes.c_float)),
            n,
        )

    def set_colors(self, line_color, bg_color):
        """Set line and background colors.

        Args:
            line_color: (r, g, b, a) tuple for the waveform line.
            bg_color: (r, g, b, a) tuple for the background.
        """
        from sol.ui_bindings import Color
        lc = Color(*line_color) if len(line_color) == 4 else Color(*line_color, 1.0)
        bc = Color(*bg_color) if len(bg_color) == 4 else Color(*bg_color, 1.0)
        _load().waveform_view_set_colors(self._ptr, lc, bc)
