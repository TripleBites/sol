#include "voice.h"
#include <stdlib.h>
#include <string.h>
#include <math.h>

/* ------------------------------------------------------------------ */
/* MIDI → frequency                                                     */
/* ------------------------------------------------------------------ */
float midi_to_freq(int midi_note) {
    /* A4 = MIDI 69 = 440.0 Hz */
    return 440.0f * powf(2.0f, (midi_note - 69) / 12.0f);
}

/* ------------------------------------------------------------------ */
/* Process (hot path — pass-through, children do the work)              */
/* ------------------------------------------------------------------ */
static void voice_process(AudioNode* self, float* buffer, int n_frames) {
    VoiceNode* v = (VoiceNode*)self;
    (void)buffer;
    (void)n_frames;

    /* Lazy child discovery: cache env and osc pointers on first call.
       Convention: child[0] = EnvelopeNode, child[1] = OscillatorNode */
    if (!v->env_child && self->base.child_count >= 1) {
        v->env_child = (AudioNode*)self->base.children[0];
    }
    if (!v->osc_child && self->base.child_count >= 2) {
        v->osc_child = (AudioNode*)self->base.children[1];
    }

    /* Children (osc → env) already processed by post-order traversal. */
}

/* ------------------------------------------------------------------ */
/* Param control                                                       */
/* ------------------------------------------------------------------ */
static void voice_set_param(AudioNode* self, const char* name, float value) {
    VoiceNode* v = (VoiceNode*)self;

    if (strcmp(name, "note_on") == 0) {
        /* value = MIDI note number packed as float */
        int note = (int)value;
        float freq = midi_to_freq(note);
        voice_node_note_on(v, note, v->velocity, 44100); /* sample_rate from context? */
        (void)freq;
    } else if (strcmp(name, "note_off") == 0) {
        voice_node_note_off(v);
    } else if (strcmp(name, "velocity") == 0) {
        v->velocity = value;
    }
}

static float voice_get_param(AudioNode* self, const char* name) {
    VoiceNode* v = (VoiceNode*)self;
    if (strcmp(name, "midi_note") == 0) return (float)v->midi_note;
    if (strcmp(name, "velocity") == 0)  return v->velocity;
    if (strcmp(name, "active") == 0)    return v->active ? 1.0f : 0.0f;
    return 0.0f;
}

/* ------------------------------------------------------------------ */
/* Note on / off                                                       */
/* ------------------------------------------------------------------ */
void voice_node_note_on(VoiceNode* v, int note, float velocity, int sample_rate) {
    if (!v) return;
    v->midi_note = note;
    v->velocity  = velocity;
    v->active    = true;

    /* Set oscillator frequency */
    if (v->osc_child && v->osc_child->set_param) {
        float freq = midi_to_freq(note);
        v->osc_child->set_param(v->osc_child, "freq", freq);
    }

    /* Trigger envelope */
    if (v->env_child && v->env_child->set_param) {
        v->env_child->set_param(v->env_child, "note_on", velocity);
    }
}

void voice_node_note_off(VoiceNode* v) {
    if (!v) return;
    v->active = false;

    /* Release envelope */
    if (v->env_child && v->env_child->set_param) {
        v->env_child->set_param(v->env_child, "note_off", 0.0f);
    }
}

bool voice_node_is_active(VoiceNode* v) {
    return v ? v->active : false;
}

bool voice_node_is_releasing(VoiceNode* v) {
    /* A voice is releasing if note-off has been sent but the envelope
       might still be sounding. We check the envelope's level. */
    if (!v || v->active) return false;
    if (v->env_child && v->env_child->get_param) {
        return v->env_child->get_param(v->env_child, "level") > 0.001f;
    }
    return false;
}

/* ------------------------------------------------------------------ */
/* Construction                                                        */
/* ------------------------------------------------------------------ */
VoiceNode* voice_node_new(int midi_note, float velocity, int sample_rate) {
    VoiceNode* v = (VoiceNode*)audio_node_alloc(sizeof(VoiceNode));
    if (!v) return NULL;

    v->midi_note = midi_note;
    v->velocity  = velocity;
    v->active    = false;
    v->env_child = NULL;
    v->osc_child = NULL;

    v->base.process_audio = voice_process;
    v->base.set_param     = voice_set_param;
    v->base.get_param     = voice_get_param;

    (void)sample_rate;
    return v;
}
