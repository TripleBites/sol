#ifndef SOL_AUDIO_ENVELOPE_H
#define SOL_AUDIO_ENVELOPE_H

#include "audio_node.h"

/* --- ADSR envelope states --- */
typedef enum {
    ENV_IDLE = 0,     /* silent, waiting for note-on */
    ENV_ATTACK,       /* rising 0→1 */
    ENV_DECAY,        /* falling 1→sustain */
    ENV_SUSTAIN,      /* holding at sustain level */
    ENV_RELEASE,      /* falling sustain→0 after note-off */
} EnvState;

/* --- EnvelopeNode ---
   Applies an ADSR amplitude envelope to the buffer.
   Children (oscillators) add their output first (post-order),
   then the envelope multiplies the accumulated buffer.

   Triggered by set_param("note_on", velocity) and
   set_param("note_off", 0).                                   */

typedef struct {
    AudioNode  base;

    /* ADSR parameters (all in seconds) */
    float attack;       /* time to rise 0→1 */
    float decay;        /* time to fall 1→sustain */
    float sustain;      /* sustain level 0.0–1.0 */
    float release;      /* time to fall sustain→0 */

    /* Per-sample state */
    EnvState state;
    float    level;          /* current envelope level 0.0–1.0 */
    float    sample_rate;
    int      samples_left;   /* frames remaining in current stage */
    float    step;           /* level delta per sample for current stage */
    bool     note_on;        /* gate signal from note_on/note_off */

    /* Precomputed stage lengths in samples */
    int      attack_samples;
    int      decay_samples;
    int      release_samples;
} EnvelopeNode;

/* --- API --- */
EnvelopeNode* envelope_node_new(float attack, float decay,
                                 float sustain, float release,
                                 int sample_rate);
EnvelopeNode* envelope_node_adsr(float a, float d, float s, float r, int sr);
void envelope_node_trigger(EnvelopeNode* env, float velocity);
void envelope_node_release(EnvelopeNode* env);

#endif /* SOL_AUDIO_ENVELOPE_H */
