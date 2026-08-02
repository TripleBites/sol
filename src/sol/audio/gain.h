#ifndef SOL_AUDIO_GAIN_H
#define SOL_AUDIO_GAIN_H

#include "audio_node.h"

/* --- GainNode — multiplies the buffer by a level scalar. ---
   Applied after children (post-order), so it scales the entire
   subtree's accumulated output.                            */

typedef struct {
    AudioNode base;
    float level;  /* 0.0 – 2.0+ (default 1.0 = unity) */
} GainNode;

GainNode* gain_node_new(float level);
void gain_node_set_level(GainNode* g, float level);

#endif /* SOL_AUDIO_GAIN_H */
