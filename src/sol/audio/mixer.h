#ifndef SOL_AUDIO_MIXER_H
#define SOL_AUDIO_MIXER_H

#include "audio_node.h"

/* --- MixerNode — sums all children into the output buffer. ---
   Children are processed in order; each child ADDS to the buffer.
   The mixer itself does no further processing — the accumulation
   happens naturally through post-order traversal.                */

typedef struct {
    AudioNode base;
    /* No additional state needed for v1 — children sum automatically */
} MixerNode;

MixerNode* mixer_node_new(void);

#endif /* SOL_AUDIO_MIXER_H */
