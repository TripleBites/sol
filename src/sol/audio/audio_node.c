#include "audio_node.h"
#include <stdlib.h>
#include <string.h>

/* ------------------------------------------------------------------ */
/* Allocation / lifecycle                                              */
/* ------------------------------------------------------------------ */
AudioNode* audio_node_alloc(size_t instance_size) {
    AudioNode* an = calloc(1, instance_size);
    if (!an) return NULL;

    /* Base Node init */
    an->base.refcount = 1;
    an->base.flags    = NODE_FLAG_VISIBLE | NODE_FLAG_PROCESS;

    return an;
}

void audio_node_init(AudioNode* an, AudioProcessFunc proc) {
    if (!an) return;
    an->process_audio = proc;
    an->node_id       = 0;
}

/* ------------------------------------------------------------------ */
/* Tree management — AudioNode-specific, no Node vtable dependency    */
/* ------------------------------------------------------------------ */
void audio_node_add_child(AudioNode* parent, AudioNode* child) {
    if (!parent || !child) return;

    /* Grow children array if needed */
    if (parent->base.child_count >= parent->base.child_capacity) {
        size_t new_cap = parent->base.child_capacity
                         ? parent->base.child_capacity * 2 : 4;
        Node** tmp = realloc(parent->base.children, sizeof(Node*) * new_cap);
        if (!tmp) return;
        parent->base.children = tmp;
        parent->base.child_capacity = new_cap;
    }

    parent->base.children[parent->base.child_count++] = &child->base;
    child->base.parent = &parent->base;
    /* No refcount — AudioNode uses direct ownership, not refcounting */
}

void audio_node_remove_child(AudioNode* parent, AudioNode* child) {
    if (!parent || !child) return;

    for (size_t i = 0; i < parent->base.child_count; i++) {
        if (parent->base.children[i] == &child->base) {
            size_t remaining = parent->base.child_count - i - 1;
            if (remaining > 0) {
                memmove(&parent->base.children[i],
                        &parent->base.children[i + 1],
                        sizeof(Node*) * remaining);
            }
            parent->base.child_count--;
            child->base.parent = NULL;
            return;
        }
    }
}

void audio_node_free(AudioNode* an) {
    if (!an) return;

    /* Free children directly — don't go through Node vtable
       since AudioNode doesn't use the Node vtable pattern.
       Null children pointers after freeing to prevent double-free. */
    for (size_t i = 0; i < an->base.child_count; i++) {
        AudioNode* child = (AudioNode*)an->base.children[i];
        if (child) {
            an->base.children[i] = NULL;
            child->base.parent = NULL;
            audio_node_free(child);
        }
    }
    an->base.child_count = 0;

    free(an->probe_buffer);
    an->probe_buffer = NULL;
    free(an->base.name);
    an->base.name = NULL;
    free(an->base.children);
    an->base.children = NULL;
    free(an);
}

/* ------------------------------------------------------------------ */
/* Tree processing (post-order: children first, then self)             */
/* ------------------------------------------------------------------ */
void audio_node_process(AudioNode* root, float* buffer, int n_frames) {
    if (!root) return;

    /* Process children first */
    for (size_t i = 0; i < root->base.child_count; i++) {
        AudioNode* child = (AudioNode*)root->base.children[i];
        audio_node_process(child, buffer, n_frames);
    }

    /* Then this node */
    if (root->process_audio) {
        root->process_audio(root, buffer, n_frames);
    }

    /* Update probe if active */
    if (root->probe_buffer && root->probe_size > 0) {
        int copy = n_frames;
        int space = root->probe_size - root->probe_write;
        if (copy > space) copy = space;
        if (copy > 0) {
            memcpy(root->probe_buffer + root->probe_write,
                   buffer, (size_t)copy * sizeof(float));
            root->probe_write += copy;
            if (root->probe_write >= root->probe_size) {
                root->probe_write = 0;  /* wrap */
            }
        }
    }
}

/* ------------------------------------------------------------------ */
/* Node ID                                                             */
/* ------------------------------------------------------------------ */
void audio_node_set_id(AudioNode* an, uint32_t id) {
    if (an) an->node_id = id;
}

uint32_t audio_node_get_id(AudioNode* an) {
    return an ? an->node_id : 0;
}
