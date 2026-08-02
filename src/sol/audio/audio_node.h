#ifndef SOL_AUDIO_NODE_H
#define SOL_AUDIO_NODE_H

#include "../scene/node.h"

/* ------------------------------------------------------------------ */
/* AudioNode — extends Node for real-time audio processing.           */
/*                                                                     */
/* process_audio is a direct function pointer (not in the vtable)      */
/* for zero dispatch overhead in the audio hot path. It is called      */
/* from the audio thread only. Children are processed first (post-     */
/* order), so each node adds to or modifies the buffer after its       */
/* children have contributed.                                          */
/*                                                                     */
/* set_param / get_param are called from the main thread (via control  */
/* queue messages), never from the audio thread.                       */
/* ------------------------------------------------------------------ */

typedef struct AudioNode AudioNode;

/* process_audio: adds this node's output INTO buffer (accumulates).
   Called post-order: children have already contributed. */
typedef void (*AudioProcessFunc)(AudioNode* self, float* buffer, int n_frames);

struct AudioNode {
    Node base;                    /* tree, refcount, name, flags */

    /* Audio hot path — direct fn pointer, not in vtable */
    AudioProcessFunc process_audio;

    /* Parameter interface — main thread only */
    void  (*set_param)(AudioNode* self, const char* name, float value);
    float (*get_param)(AudioNode* self, const char* name);

    /* Unique stable ID for control queue addressing */
    uint32_t node_id;

    /* Probe ring buffer — written by audio thread, read by GUI */
    float* probe_buffer;
    int    probe_write;
    int    probe_size;
};

/* --- Lifecycle --- */
AudioNode* audio_node_alloc(size_t instance_size);
void       audio_node_init(AudioNode* an, AudioProcessFunc proc);
void       audio_node_free(AudioNode* an);

/* --- Tree processing (called by AudioPipeline) --- */
void audio_node_process(AudioNode* root, float* buffer, int n_frames);

/* --- Tree management (AudioNode-specific — bypasses Node vtable) --- */
void audio_node_add_child(AudioNode* parent, AudioNode* child);
void audio_node_remove_child(AudioNode* parent, AudioNode* child);

/* --- Node ID (assigned by AudioPipeline on add) --- */
void audio_node_set_id(AudioNode* an, uint32_t id);
uint32_t audio_node_get_id(AudioNode* an);

#endif /* SOL_AUDIO_NODE_H */
