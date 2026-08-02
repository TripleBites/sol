#include "mixer.h"
#include <stdlib.h>
#include <string.h>

/* Mixer is a pass-through — children add to the buffer directly,
   and the post-order traversal already processed them before us. */

static void mixer_process(AudioNode* self, float* buffer, int n_frames) {
    (void)self;
    (void)buffer;
    (void)n_frames;
    /* Nothing to do — children already added their output to the buffer */
}

static void mixer_set_param(AudioNode* self, const char* name, float value) {
    (void)self; (void)name; (void)value;
}

static float mixer_get_param(AudioNode* self, const char* name) {
    (void)self; (void)name;
    return 0.0f;
}

MixerNode* mixer_node_new(void) {
    MixerNode* m = (MixerNode*)audio_node_alloc(sizeof(MixerNode));
    if (!m) return NULL;

    m->base.process_audio = mixer_process;
    m->base.set_param     = mixer_set_param;
    m->base.get_param     = mixer_get_param;

    return m;
}
