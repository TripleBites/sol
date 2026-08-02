#ifndef SOL_AUDIO_VOICE_H
#define SOL_AUDIO_VOICE_H

#include "audio_node.h"

/* --- VoiceNode — one active MIDI note. ---
   A VoiceNode expects two children:
     child[0] = EnvelopeNode  (ADSR amplitude envelope)
     child[1] = OscillatorNode (waveform generator)

   Post-order tree traversal handles the audio:
     Oscillator adds samples → Envelope multiplies → VoiceNode passes through.

   VoiceNode's job is purely control: it receives note_on/note_off
   via the control queue and forwards frequency changes and envelope
   triggers to its children.                                        */

typedef struct {
    AudioNode base;

    int   midi_note;    /* MIDI note number (0–127) */
    float velocity;     /* 0.0–1.0 */
    bool  active;       /* true while note is held */

    /* Cached child pointers for fast access */
    AudioNode* env_child;
    AudioNode* osc_child;
} VoiceNode;

/* --- API --- */
VoiceNode* voice_node_new(int midi_note, float velocity, int sample_rate);
void voice_node_note_on(VoiceNode* v, int note, float velocity, int sample_rate);
void voice_node_note_off(VoiceNode* v);
bool voice_node_is_active(VoiceNode* v);
bool voice_node_is_releasing(VoiceNode* v);  /* note-off but still sounding */

/* MIDI note → frequency (A4 = 440Hz = MIDI 69) */
float midi_to_freq(int midi_note);

#endif /* SOL_AUDIO_VOICE_H */
