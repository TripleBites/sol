#include "gain.h"
#include <stdlib.h>
#include <string.h>

static void gain_process(AudioNode* self, float* buffer, int n_frames) {
    GainNode* g = (GainNode*)self;
    float level = g->level;
    for (int i = 0; i < n_frames; i++) {
        buffer[i] *= level;
    }
}

static void gain_set_param(AudioNode* self, const char* name, float value) {
    GainNode* g = (GainNode*)self;
    if (strcmp(name, "level") == 0) {
        g->level = value;
    }
}

static float gain_get_param(AudioNode* self, const char* name) {
    GainNode* g = (GainNode*)self;
    if (strcmp(name, "level") == 0) return g->level;
    return 0.0f;
}

GainNode* gain_node_new(float level) {
    GainNode* g = (GainNode*)audio_node_alloc(sizeof(GainNode));
    if (!g) return NULL;

    g->level = level;

    g->base.process_audio = gain_process;
    g->base.set_param     = gain_set_param;
    g->base.get_param     = gain_get_param;

    return g;
}

void gain_node_set_level(GainNode* g, float level) {
    if (g) g->level = level;
}
