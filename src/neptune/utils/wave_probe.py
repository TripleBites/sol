import collections

class WaveProbe:
    """
    Wraps an oscillator or any audio node.
    Yields the exact same values downstream, but stores a rolling window for the GUI.
    """
    def __init__(self, source, buffer_size=1024, name="Probe"):
        self.source = source
        self.name = name
        # Pre-fill buffer with zeros so the UI doesn't crash on startup
        self.buffer = collections.deque([0.0] * buffer_size, maxlen=buffer_size)

    def __iter__(self):
        iter(self.source)
        return self

    def __next__(self):
        val = next(self.source)       # Pull from upstream
        self.buffer.append(val)       # Tap the signal for the UI
        return val                    # Push downstream