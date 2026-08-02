"""Patch save/load — JSON serialization of AudioNode trees.

Format: .solpatch JSON files containing the full AudioPipeline state.

Example:
{
  "sample_rate": 44100,
  "root": {
    "type": "MixerNode",
    "name": "main",
    "params": {},
    "children": [...]
  }
}
"""
import ctypes
import json
from sol.audio_bindings import (
    AudioPipeline, AudioNode,
    OscillatorNode, MixerNode, GainNode,
    EnvelopeNode, VoiceNode,
    OSC_SINE, OSC_SQUARE, OSC_SAW, OSC_TRIANGLE, OSC_NOISE,
    WAVEFORM_NAMES,
)

# Registry maps type name strings to constructor functions
_NODE_REGISTRY = {}


def register_node_type(name: str, constructor):
    """Register an AudioNode subclass for deserialization."""
    _NODE_REGISTRY[name] = constructor


def _build_default_registry():
    register_node_type("MixerNode", MixerNode)
    register_node_type("GainNode", GainNode)
    register_node_type("OscillatorNode", OscillatorNode)
    register_node_type("EnvelopeNode", EnvelopeNode)
    register_node_type("VoiceNode", VoiceNode)


_build_default_registry()


# ---------------------------------------------------------------------------
# Serialization helpers
# ---------------------------------------------------------------------------

def _serialize_node(node: AudioNode) -> dict:
    """Serialize one AudioNode and its children recursively."""
    node_type = type(node).__name__
    result = {
        "type": node_type,
        "name": getattr(node, "_name", ""),
        "params": _get_node_params(node),
    }

    # Collect children (AudioNode doesn't have a public children list yet,
    # but VoiceNode children are Envelope + Oscillator)
    children = []
    if hasattr(node, "_children"):
        for child in node._children:
            children.append(_serialize_node(child))

    if children:
        result["children"] = children

    return result


def _get_node_params(node: AudioNode) -> dict:
    """Extract serializable parameters from a node using C get_param."""
    params = {}
    t = type(node).__name__

    from sol.audio_bindings import _audio_node_get_param_float

    def gp(name):
        """Get param from C, return float."""
        try:
            return _audio_node_get_param_float(node._ptr, name.encode("utf-8"))
        except Exception:
            return 0.0

    if t == "OscillatorNode":
        params["waveform"] = int(gp("waveform"))
        params["freq"] = round(gp("freq"), 2)
        params["amp"] = round(gp("amp"), 3)
    elif t == "GainNode":
        params["level"] = round(gp("level"), 3)
    elif t == "EnvelopeNode":
        params["attack"] = round(gp("attack"), 3)
        params["decay"] = round(gp("decay"), 3)
        params["sustain"] = round(gp("sustain"), 3)
        params["release"] = round(gp("release"), 3)
    elif t == "VoiceNode":
        params["note"] = int(gp("midi_note"))
        params["velocity"] = round(gp("velocity"), 3)
    elif t == "MixerNode":
        pass  # Mixer has no params

    return params


def save_patch(pipeline: AudioPipeline, path: str) -> None:
    """Serialize the AudioPipeline's root AudioNode tree to a .solpatch file."""
    if not hasattr(pipeline, "_root"):
        raise ValueError("Pipeline has no root node")

    root = pipeline._root
    data = {
        "sample_rate": pipeline.sample_rate,
        "root": _serialize_node(root),
    }

    with open(path, "w") as f:
        json.dump(data, f, indent=2)

    print(f"[patch] Saved to {path}")


def load_patch(pipeline: AudioPipeline, path: str) -> dict:
    """Load a .solpatch file and reconstruct the AudioNode tree.

    Replaces the pipeline's root with the deserialized tree and
    re-registers all nodes so parameter control works immediately.

    Returns the parsed dict for inspection."""
    with open(path, "r") as f:
        data = json.load(f)

    sample_rate = data.get("sample_rate", pipeline.sample_rate)
    root_data = data.get("root", {})

    print(f"[patch] Loaded from {path}")
    print(f"  Sample rate: {sample_rate}")
    print(f"  Root type: {root_data.get('type', '?')}")

    # Reconstruct the tree
    try:
        new_root = _deserialize_node(root_data, sample_rate)
        pipeline.set_root(new_root)

        # Re-register all nodes with the pipeline
        from sol.audio_bindings import _audio_pipeline_register_node

        def register_tree(node):
            if node is not None:
                _audio_pipeline_register_node(pipeline._ptr, node.ptr)
                node._owned = True
                if hasattr(node, '_children'):
                    for child in node._children:
                        register_tree(child)

        register_tree(new_root)
        print(f"[patch] Tree reconstructed and registered")
    except Exception as e:
        print(f"[patch] Warning: could not reconstruct tree: {e}")
        print(f"[patch] Returning raw data only")

    return data


def _deserialize_node(node_data: dict, sample_rate: int = 44100) -> AudioNode:
    """Reconstruct an AudioNode from serialized data."""
    node_type = node_data["type"]
    params = node_data.get("params", {})

    constructor = _NODE_REGISTRY.get(node_type)
    if not constructor:
        raise ValueError(f"Unknown node type: {node_type}")

    if node_type == "OscillatorNode":
        waveform = params.get("waveform", OSC_SINE)
        freq = params.get("freq", 440.0)
        amp = params.get("amp", 1.0)
        node = constructor(waveform, freq, amp, sample_rate)
    elif node_type == "EnvelopeNode":
        attack = params.get("attack", 0.01)
        decay = params.get("decay", 0.15)
        sustain = params.get("sustain", 0.7)
        release = params.get("release", 0.3)
        node = constructor(attack, decay, sustain, release, sample_rate)
    elif node_type == "GainNode":
        level = params.get("level", 1.0)
        node = constructor(level)
    elif node_type == "VoiceNode":
        note = params.get("note", 60)
        velocity = params.get("velocity", 0.8)
        node = constructor(note, velocity, sample_rate)
    else:
        node = constructor()

    node._name = node_data.get("name", "")

    # Recurse into children
    for child_data in node_data.get("children", []):
        child = _deserialize_node(child_data, sample_rate)
        node.add_child(child)

    return node
