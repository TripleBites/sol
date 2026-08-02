"""Neptune Synth Panel — Sol UI Control tree for the synthesizer.

Layout:
┌────────────────────────────────────────────┐
│  Neptune Synth                  [Save][Load]│
├──────────┬──────────┬──────────────────────┤
│ OSC      │ Envelope │ Master               │
│ [Knob]   │ [Knob]   │ [Knob] Volume        │
│ Waveform │ Attack   │                      │
│ [Knob]   │ [Knob]   │                      │
│ Freq     │ Decay    │                      │
│ [Knob]   │ [Knob]   │                      │
│ Amp      │ Sustain  │                      │
│          │ [Knob]   │                      │
│          │ Release  │                      │
├──────────┴──────────┴──────────────────────┤
│  ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~  │  ← Waveform View
│  ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~  │
├────────────────────────────────────────────┤
│  Status: Ready                     ♪ C4 ON │
└────────────────────────────────────────────┘
"""
from sol.ui_bindings import (
    Control, ColorRect, VBoxContainer, HBoxContainer,
    Label, Button, Knob, SceneTree,
    SIZE_FILL, SIZE_EXPAND, SIZE_SHRINK_CENTER,
    Color,
)
from sol.audio_bindings import AudioPipeline


def _make_section(title: str) -> VBoxContainer:
    """Create a labeled section with a dark background."""
    box = VBoxContainer()
    box.set_separation(4)

    header = Label(title)
    header.set_font_size(13)
    header.set_size_flags(SIZE_SHRINK_CENTER, 0)

    box.add_child(header)
    return box


def _make_knob_row(label_text: str, knob: Knob,
                   min_val: float, max_val: float,
                   default_val: float, step: float = 0.0) -> VBoxContainer:
    """Create a labeled knob row."""
    row = VBoxContainer()
    row.set_separation(2)

    knob.set_range(min_val, max_val, step)
    knob.set_value(default_val)
    knob.set_min_size(48, 48)

    lbl = Label(label_text)
    lbl.set_font_size(10)
    lbl.set_size_flags(SIZE_SHRINK_CENTER, 0)

    row.add_child(knob)
    row.add_child(lbl)
    return row


class SynthPanel:
    """Builds and manages the Neptune synth GUI."""

    def __init__(self, pipeline: AudioPipeline):
        self.pipeline = pipeline
        self._knobs = {}  # name → Knob
        self._node_ids = {}  # param_name → node_id for send_set_param
        self._register_pipeline_nodes()
        self._build()
        self._wire_knobs()

    def _register_pipeline_nodes(self):
        """Walk the pipeline tree and register node IDs for parameter control.

        Maps parameter names to node_ids so knob callbacks can call
        pipeline.send_set_param()."""
        if not hasattr(self.pipeline, '_root'):
            return

        root = self.pipeline._root
        # Register from the C side to get stable IDs
        from sol.audio_bindings import _audio_pipeline_register_node

        def walk(node, prefix=""):
            if node is None:
                return
            node_id = _audio_pipeline_register_node(self.pipeline._ptr, node.ptr)
            node._owned = True

            t = type(node).__name__
            if node_id != 0xFFFFFFFF:
                if t == "GainNode":
                    self._node_ids["volume"] = node_id
                elif t == "OscillatorNode":
                    self._node_ids["osc"] = node_id
                elif t == "EnvelopeNode":
                    self._node_ids["envelope"] = node_id

            if hasattr(node, '_children'):
                for child in node._children:
                    walk(child, prefix)

        walk(root)

    def _build(self):
        """Construct the full UI tree."""
        # Root wrapper — plain Control, not a container, so it doesn't
        # grow beyond the window size. The engine sets root.rect each frame.
        self.root = Control()
        self.root.set_name("synth_root")
        self.root.set_anchor(0, 0, 0, 0)
        self.root.set_margin(0, 0, 0, 0)

        # Main container
        self.main = VBoxContainer()
        self.main.set_name("synth_panel")
        self.main.set_anchor(0, 0, 1, 1)
        self.main.set_margin(0, 0, 0, 0)
        self.main.set_separation(6)

        # --- Title bar ---
        title_bar = HBoxContainer()
        title_bar.set_separation(8)
        title_bar.set_min_size(0, 32)
        title_bar.set_size_flags(SIZE_FILL, 0)

        title = Label("Neptune Synth")
        title.set_font_size(18)
        title.set_size_flags(SIZE_EXPAND | SIZE_FILL, SIZE_FILL)
        title_bar.add_child(title)

        save_btn = Button()
        save_btn.set_name("save_btn")
        save_btn.set_min_size(60, 0)
        save_btn.set_size_flags(0, SIZE_FILL)
        save_lbl = Label("Save"); save_lbl.set_font_size(11)
        save_btn.add_child(save_lbl)
        title_bar.add_child(save_btn)

        load_btn = Button()
        load_btn.set_name("load_btn")
        load_btn.set_min_size(60, 0)
        load_btn.set_size_flags(0, SIZE_FILL)
        load_lbl = Label("Load"); load_lbl.set_font_size(11)
        load_btn.add_child(load_lbl)
        title_bar.add_child(load_btn)

        self.main.add_child(title_bar)

        # --- Control sections ---
        controls = HBoxContainer()
        controls.set_separation(8)
        controls.set_size_flags(SIZE_FILL, SIZE_EXPAND | SIZE_FILL)

        # OSC section
        osc_section = _make_section("OSC")
        osc_section.set_size_flags(SIZE_FILL, SIZE_EXPAND | SIZE_FILL)

        self.knob_waveform = Knob()
        self.knob_waveform.set_range(0, 4, 1)  # 0=sine, 1=square, 2=saw, 3=tri, 4=noise
        self.knob_waveform.set_value(0)
        self._knobs["waveform"] = self.knob_waveform

        self.knob_freq = Knob()
        self._knobs["freq"] = self.knob_freq

        self.knob_amp = Knob()
        self._knobs["amp"] = self.knob_amp

        osc_section.add_child(_make_knob_row("Wave", self.knob_waveform, 0, 4, 0, 1))
        osc_section.add_child(_make_knob_row("Freq", self.knob_freq, 20, 2000, 440))
        osc_section.add_child(_make_knob_row("Amp", self.knob_amp, 0, 1, 0.5, 0.01))
        controls.add_child(osc_section)

        # Envelope section
        env_section = _make_section("ENV")
        env_section.set_size_flags(SIZE_FILL, SIZE_EXPAND | SIZE_FILL)

        self.knob_attack = Knob()
        self._knobs["attack"] = self.knob_attack

        self.knob_decay = Knob()
        self._knobs["decay"] = self.knob_decay

        self.knob_sustain = Knob()
        self._knobs["sustain"] = self.knob_sustain

        self.knob_release = Knob()
        self._knobs["release"] = self.knob_release

        env_section.add_child(_make_knob_row("Atk", self.knob_attack, 0.001, 2.0, 0.02, 0.001))
        env_section.add_child(_make_knob_row("Dec", self.knob_decay, 0.001, 2.0, 0.2, 0.001))
        env_section.add_child(_make_knob_row("Sus", self.knob_sustain, 0, 1, 0.7, 0.01))
        env_section.add_child(_make_knob_row("Rel", self.knob_release, 0.001, 3.0, 0.4, 0.001))
        controls.add_child(env_section)

        # Master section
        master_section = _make_section("Master")
        master_section.set_size_flags(SIZE_FILL, SIZE_EXPAND | SIZE_FILL)

        self.knob_volume = Knob()
        self._knobs["volume"] = self.knob_volume
        master_section.add_child(_make_knob_row("Vol", self.knob_volume, 0, 1, 0.5, 0.01))
        controls.add_child(master_section)

        self.main.add_child(controls)

        # --- Waveform view ---
        from neptune.gui.waveform_view import WaveformView
        self.waveform = WaveformView()
        self.waveform.set_min_size(0, 100)
        self.waveform.set_size_flags(SIZE_FILL, 0)
        self.main.add_child(self.waveform)

        # --- Status bar ---
        status_bar = HBoxContainer()
        status_bar.set_min_size(0, 24)
        status_bar.set_size_flags(SIZE_FILL, 0)
        status_bar.set_separation(8)

        self.status_label = Label("Ready")
        self.status_label.set_font_size(11)
        self.status_label.set_size_flags(SIZE_EXPAND | SIZE_FILL, SIZE_FILL)
        status_bar.add_child(self.status_label)

        self.note_label = Label("")
        self.note_label.set_font_size(11)
        self.note_label.set_size_flags(0, SIZE_FILL)
        status_bar.add_child(self.note_label)

        self.main.add_child(status_bar)

        # Add main container to root
        self.root.add_child(self.main)

    def get_root(self) -> Control:
        """Return the root Control for adding to a SceneTree."""
        return self.root

    def _wire_knobs(self):
        """Connect knob value_changed signals to pipeline parameters."""
        # OSC knobs
        if "osc" in self._node_ids:
            osc_id = self._node_ids["osc"]
            self.knob_freq.on_value_changed.connect(
                lambda: self.pipeline.send_set_param(osc_id, "freq", self.knob_freq.get_value())
            )
            self.knob_amp.on_value_changed.connect(
                lambda: self.pipeline.send_set_param(osc_id, "amp", self.knob_amp.get_value())
            )
            # Waveform knob uses integer steps
            self.knob_waveform.on_value_changed.connect(
                lambda: self.pipeline.send_set_param(osc_id, "waveform", self.knob_waveform.get_value())
            )

        # Envelope knobs
        if "envelope" in self._node_ids:
            env_id = self._node_ids["envelope"]
            self.knob_attack.on_value_changed.connect(
                lambda: self.pipeline.send_set_param(env_id, "attack", self.knob_attack.get_value())
            )
            self.knob_decay.on_value_changed.connect(
                lambda: self.pipeline.send_set_param(env_id, "decay", self.knob_decay.get_value())
            )
            self.knob_sustain.on_value_changed.connect(
                lambda: self.pipeline.send_set_param(env_id, "sustain", self.knob_sustain.get_value())
            )
            self.knob_release.on_value_changed.connect(
                lambda: self.pipeline.send_set_param(env_id, "release", self.knob_release.get_value())
            )

        # Master volume
        if "volume" in self._node_ids:
            vol_id = self._node_ids["volume"]
            self.knob_volume.on_value_changed.connect(
                lambda: self.pipeline.send_set_param(vol_id, "level", self.knob_volume.get_value())
            )

    def connect_save_load_buttons(self, on_save, on_load):
        """Wire the Save and Load button signals to callbacks.

        Called from main.py after the tree is added to the SceneTree
        (signals fire only after the node enters the tree)."""
        self.save_btn = on_save
        self.load_btn = on_load

        # Find save/load buttons by name in the tree
        def find_btn_by_name(node, name):
            if hasattr(node, 'ptr'):
                # Check via C name access
                pass
            return None

        # Walk tree to find buttons
        def walk_and_connect(node):
            from sol.ui_bindings import Button as BtnCls
            if isinstance(node, BtnCls):
                try:
                    btn_name = node._ptr  # we need to get the name from C
                except Exception:
                    btn_name = None

            if hasattr(node, 'ptr'):
                # Use Node vtable to check name
                pass

        # For now, buttons are connected in main.py's run_gui()

    def set_active_note(self, note_name: str = ""):
        """Show the currently playing note."""
        self.note_label.set_text(note_name)

    def set_status(self, text: str):
        """Update the status bar."""
        self.status_label.set_text(text)

    def update_waveform(self, samples: list):
        """Push new sample data to the waveform view."""
        self.waveform.set_data(samples)
