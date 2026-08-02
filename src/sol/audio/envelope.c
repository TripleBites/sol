#include "envelope.h"
#include <stdlib.h>
#include <string.h>
#include <math.h>

/* ------------------------------------------------------------------ */
/* Process (hot path — called from audio thread post-order)             */
/* ------------------------------------------------------------------ */
static void envelope_process(AudioNode* self, float* buffer, int n_frames) {
    EnvelopeNode* env = (EnvelopeNode*)self;

    for (int i = 0; i < n_frames; i++) {
        /* Advance state machine */
        switch (env->state) {
        case ENV_IDLE:
            env->level = 0.0f;
            break;

        case ENV_ATTACK:
            if (env->samples_left > 0) {
                env->level += env->step;
                env->samples_left--;
            } else {
                /* Enter decay */
                env->state = ENV_DECAY;
                env->samples_left = env->decay_samples;
                if (env->samples_left > 0) {
                    env->step = (env->sustain - 1.0f) / (float)env->samples_left;
                } else {
                    env->step = 0.0f;
                    env->state = ENV_SUSTAIN;
                }
            }
            break;

        case ENV_DECAY:
            if (env->samples_left > 0) {
                env->level += env->step;
                env->samples_left--;
            } else {
                env->level  = env->sustain;
                env->state  = ENV_SUSTAIN;
            }
            /* Check for note-off during attack/decay */
            if (!env->note_on) {
                env->state = ENV_RELEASE;
                env->samples_left = env->release_samples;
                if (env->samples_left > 0) {
                    env->step = -env->level / (float)env->samples_left;
                } else {
                    env->level = 0.0f;
                    env->state = ENV_IDLE;
                }
            }
            break;

        case ENV_SUSTAIN:
            if (!env->note_on) {
                env->state = ENV_RELEASE;
                env->samples_left = env->release_samples;
                if (env->samples_left > 0) {
                    env->step = -env->level / (float)env->samples_left;
                } else {
                    env->level = 0.0f;
                    env->state = ENV_IDLE;
                }
            }
            break;

        case ENV_RELEASE:
            if (env->samples_left > 0) {
                env->level += env->step;
                env->samples_left--;
            } else {
                env->level = 0.0f;
                env->state = ENV_IDLE;
            }
            break;
        }

        /* Clamp level */
        if (env->level < 0.0f) env->level = 0.0f;
        if (env->level > 1.0f) env->level = 1.0f;

        /* Apply envelope to this sample */
        buffer[i] *= env->level;
    }
}

/* ------------------------------------------------------------------ */
/* Trigger / release                                                   */
/* ------------------------------------------------------------------ */
void envelope_node_trigger(EnvelopeNode* env, float velocity) {
    if (!env) return;
    env->note_on  = true;
    env->state    = ENV_ATTACK;
    env->level    = 0.0f;
    env->samples_left = env->attack_samples;
    if (env->samples_left > 0) {
        env->step = 1.0f / (float)env->samples_left;
    } else {
        env->level = velocity;  /* instant attack */
        env->state = ENV_DECAY;
        env->samples_left = env->decay_samples;
        if (env->samples_left > 0) {
            env->step = (env->sustain - 1.0f) / (float)env->samples_left;
        }
    }
    (void)velocity;  /* velocity scaling reserved for future */
}

void envelope_node_release(EnvelopeNode* env) {
    if (!env) return;
    env->note_on = false;
}

/* ------------------------------------------------------------------ */
/* Param control                                                       */
/* ------------------------------------------------------------------ */
static void env_set_param(AudioNode* self, const char* name, float value) {
    EnvelopeNode* env = (EnvelopeNode*)self;
    if (strcmp(name, "attack") == 0) {
        env->attack = value;
        env->attack_samples = (int)(value * env->sample_rate);
    } else if (strcmp(name, "decay") == 0) {
        env->decay = value;
        env->decay_samples = (int)(value * env->sample_rate);
    } else if (strcmp(name, "sustain") == 0) {
        env->sustain = value;
    } else if (strcmp(name, "release") == 0) {
        env->release = value;
        env->release_samples = (int)(value * env->sample_rate);
    } else if (strcmp(name, "note_on") == 0) {
        envelope_node_trigger(env, value);
    } else if (strcmp(name, "note_off") == 0) {
        envelope_node_release(env);
    }
}

static float env_get_param(AudioNode* self, const char* name) {
    EnvelopeNode* env = (EnvelopeNode*)self;
    if (strcmp(name, "attack") == 0)  return env->attack;
    if (strcmp(name, "decay") == 0)   return env->decay;
    if (strcmp(name, "sustain") == 0) return env->sustain;
    if (strcmp(name, "release") == 0) return env->release;
    if (strcmp(name, "level") == 0)   return env->level;
    return 0.0f;
}

/* ------------------------------------------------------------------ */
/* Construction                                                        */
/* ------------------------------------------------------------------ */
EnvelopeNode* envelope_node_new(float attack, float decay,
                                 float sustain, float release,
                                 int sample_rate) {
    EnvelopeNode* env = (EnvelopeNode*)audio_node_alloc(sizeof(EnvelopeNode));
    if (!env) return NULL;

    env->attack         = attack;
    env->decay          = decay;
    env->sustain        = sustain;
    env->release        = release;
    env->sample_rate    = (float)sample_rate;
    env->state          = ENV_IDLE;
    env->level          = 0.0f;
    env->samples_left   = 0;
    env->step           = 0.0f;
    env->note_on        = false;

    env->attack_samples  = (int)(attack  * env->sample_rate);
    env->decay_samples   = (int)(decay   * env->sample_rate);
    env->release_samples = (int)(release * env->sample_rate);

    env->base.process_audio = envelope_process;
    env->base.set_param     = env_set_param;
    env->base.get_param     = env_get_param;

    return env;
}

EnvelopeNode* envelope_node_adsr(float a, float d, float s, float r, int sr) {
    return envelope_node_new(a, d, s, r, sr);
}
