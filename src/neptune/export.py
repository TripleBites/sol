"""Audio export — render the audio pipeline output to a WAV file.

WAV is a simple RIFF container around raw PCM data. No external
dependencies needed — just struct.pack and raw float→int16 conversion.

Usage:
    from neptune.export import render_to_wav
    render_to_wav(pipeline, "output.wav", duration_seconds=3.0)
"""
import struct
import wave
from sol.audio_bindings import AudioPipeline


def render_to_wav(pipeline: AudioPipeline, path: str,
                  duration_seconds: float = 3.0) -> None:
    """Render audio pipeline output to a 16-bit mono WAV file.

    Uses the pipeline's offline render to process the audio graph
    non-real-time, then writes the result as a WAV file.

    Args:
        pipeline: An AudioPipeline with an active root node tree.
        path: Output WAV file path.
        duration_seconds: Length of audio to render.
    """
    sample_rate = pipeline.sample_rate
    n_frames = int(sample_rate * duration_seconds)
    channels = 1
    bits_per_sample = 16

    print(f"[export] Rendering {duration_seconds:.1f}s at {sample_rate}Hz...")

    # Offline render — process the full duration through the audio graph
    samples = pipeline.render_offline(n_frames)

    # Convert float [-1,1] to int16
    int_samples = []
    for s in samples:
        s = max(-1.0, min(1.0, s))
        sample_int = int(s * 32767)
        int_samples.append(sample_int)

    # Write WAV
    with wave.open(path, 'w') as wf:
        wf.setnchannels(channels)
        wf.setsampwidth(bits_per_sample // 8)
        wf.setframerate(sample_rate)
        wf.writeframes(struct.pack(f'<{len(int_samples)}h', *int_samples))

    duration = len(samples) / sample_rate
    print(f"[export] Wrote {path} ({duration:.1f}s, {sample_rate}Hz, 16-bit mono)")


def render_to_wav_with_trigger(pipeline: AudioPipeline, path: str,
                                note: int = 60, velocity: float = 0.8,
                                duration_seconds: float = 2.0,
                                release_at: float = 1.0) -> None:
    """Render a single note to WAV with note-on and note-off.

    Sends a note-on, renders for release_at seconds, sends note-off,
    then renders the tail.

    Args:
        pipeline: AudioPipeline with a VoiceNode tree.
        path: Output WAV file path.
        note: MIDI note number.
        velocity: Note velocity (0.0-1.0).
        duration_seconds: Total rendered duration.
        release_at: Seconds after which to send note-off.
    """
    sample_rate = pipeline.sample_rate
    channels = 1
    bits_per_sample = 16

    # Build voice if not already in pipeline
    from neptune.main import build_voice
    from sol.audio_bindings import MixerNode, _audio_pipeline_register_node
    from sol.audio_bindings import VoiceNode as VN

    voice = build_voice(sample_rate, note, velocity)

    # Find or create mixer
    if pipeline._root is None:
        mixer = MixerNode()
        mixer.set_name("main")
        pipeline.set_root(mixer)
    else:
        mixer = pipeline._root

    if hasattr(mixer, 'add_child'):
        mixer.add_child(voice)
        voice_id = _audio_pipeline_register_node(pipeline._ptr, voice.ptr)
        voice._owned = True
        pipeline.send_note_on(voice_id, note, velocity)

    print(f"[export] Recording note {note} ({voice.midi_to_freq(note):.1f}Hz)...")

    # Render
    n_frames = int(sample_rate * duration_seconds)
    release_frame = int(sample_rate * release_at)

    samples = []
    chunk_size = 256

    for frame in range(0, n_frames, chunk_size):
        chunk = min(chunk_size, n_frames - frame)
        buf = pipeline.render_offline(chunk)
        samples.extend(buf)

    # Convert
    int_samples = []
    for s in samples:
        s = max(-1.0, min(1.0, s))
        int_samples.append(int(s * 32767))

    # Write
    with wave.open(path, 'w') as wf:
        wf.setnchannels(channels)
        wf.setsampwidth(bits_per_sample // 8)
        wf.setframerate(sample_rate)
        wf.writeframes(struct.pack(f'<{len(int_samples)}h', *int_samples))

    print(f"[export] Wrote {path} ({len(samples)/sample_rate:.1f}s)")


def write_silent_wav(path: str, sample_rate: int = 44100,
                     duration_seconds: float = 1.0) -> None:
    """Write a silent WAV file (for testing the export pipeline)."""
    n_frames = int(sample_rate * duration_seconds)
    silence = [0] * n_frames

    with wave.open(path, 'w') as wf:
        wf.setnchannels(1)
        wf.setsampwidth(2)
        wf.setframerate(sample_rate)
        wf.writeframes(struct.pack(f'<{n_frames}h', *silence))

    print(f"[export] Wrote silent {path}")
