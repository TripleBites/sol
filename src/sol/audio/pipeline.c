#include "pipeline.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>

/* ------------------------------------------------------------------ */
/* Create / destroy                                                    */
/* ------------------------------------------------------------------ */
AudioPipeline* audio_pipeline_new(int sample_rate, int buffer_size) {
    AudioPipeline* ap = calloc(1, sizeof(AudioPipeline));
    if (!ap) return NULL;

    ap->sample_rate    = sample_rate;
    ap->buffer_size    = buffer_size;
    ap->next_node_id   = 1;   /* 0 = invalid/NULL */

    /* Mix buffer — generous fixed size for any reasonable callback */
    ap->mix_buffer = calloc(4096, sizeof(float));
    if (!ap->mix_buffer) {
        free(ap);
        return NULL;
    }

    /* Control queue — 256 entries, lock-free SPSC */
    ap->cq_capacity = 256;
    ap->cq = calloc((size_t)ap->cq_capacity, sizeof(ControlMsg));
    if (!ap->cq) {
        free(ap->mix_buffer);
        free(ap);
        return NULL;
    }
    ap->cq_read  = 0;
    ap->cq_write = 0;

    /* Node lookup table */
    ap->node_table_size = 64;
    ap->node_table = calloc((size_t)ap->node_table_size, sizeof(AudioNode*));
    if (!ap->node_table) {
        free(ap->cq);
        free(ap->mix_buffer);
        free(ap);
        return NULL;
    }

    return ap;
}

void audio_pipeline_free(AudioPipeline* ap) {
    if (!ap) return;
    audio_pipeline_stop(ap);

    /* Free probe buffers and clear node pointers to prevent double-free */
    for (int i = 0; i < ap->probe_count; i++) {
        /* Clear the node's pointer to this buffer */
        uint32_t nid = ap->probes[i].node_id;
        if (nid > 0 && nid < (uint32_t)ap->node_table_size) {
            AudioNode* node = ap->node_table[nid];
            if (node && node->probe_buffer == ap->probes[i].buffer) {
                node->probe_buffer = NULL;
                node->probe_size = 0;
            }
        }
        free(ap->probes[i].buffer);
        ap->probes[i].buffer = NULL;
    }

    /* Free the audio node tree */
    if (ap->root) {
        audio_node_free(ap->root);
        ap->root = NULL;
    }

    free(ap->node_table);
    free(ap->cq);
    free(ap->mix_buffer);
    free(ap);
}

/* ------------------------------------------------------------------ */
/* Root                                                                */
/* ------------------------------------------------------------------ */
/* Recursively register a node and all its children */
static void register_tree(AudioPipeline* ap, AudioNode* node) {
    if (!node) return;
    audio_pipeline_register_node(ap, node);
    for (size_t i = 0; i < node->base.child_count; i++) {
        register_tree(ap, (AudioNode*)node->base.children[i]);
    }
}

void audio_pipeline_set_root(AudioPipeline* ap, AudioNode* root) {
    if (ap->root) {
        audio_node_free(ap->root);
    }
    ap->root = root;

    /* Register entire tree for control-queue addressing */
    register_tree(ap, root);
}

/* ------------------------------------------------------------------ */
/* Start / stop                                                        */
/* ------------------------------------------------------------------ */
bool audio_pipeline_start(AudioPipeline* ap, const SolIO* platform) {
    if (!ap || !platform) return false;
    if (!platform->audio_init) {
        fprintf(stderr, "[pipeline] platform has no audio support\n");
        return false;
    }

    ap->platform = platform;
    ap->running  = true;

    /* mono for v1 */
    if (!platform->audio_init(ap->sample_rate, 1,
                               audio_pipeline_callback, ap)) {
        fprintf(stderr, "[pipeline] audio_init failed\n");
        ap->running = false;
        return false;
    }

    printf("[pipeline] Started: %dHz, %d-sample buffer\n",
           ap->sample_rate, ap->buffer_size);
    return true;
}

void audio_pipeline_stop(AudioPipeline* ap) {
    if (!ap || !ap->running) return;
    ap->running = false;

    if (ap->platform && ap->platform->audio_shutdown) {
        ap->platform->audio_shutdown();
    }
    ap->platform = NULL;

    printf("[pipeline] Stopped\n");
}

/* ------------------------------------------------------------------ */
/* Control queue (SPSC — main thread writes, audio thread reads)       */
/* ------------------------------------------------------------------ */

/* Called from main thread */
static bool cq_push(AudioPipeline* ap, ControlMsg msg) {
    int next = (ap->cq_write + 1) % ap->cq_capacity;
    if (next == ap->cq_read) return false;  /* full */
    ap->cq[ap->cq_write] = msg;
    ap->cq_write = next;
    return true;
}

/* Called from audio thread — drains all pending messages */
static void cq_drain(AudioPipeline* ap) {
    while (ap->cq_read != ap->cq_write) {
        ControlMsg* msg = &ap->cq[ap->cq_read];
        ap->cq_read = (ap->cq_read + 1) % ap->cq_capacity;

        /* Lookup target node */
        AudioNode* target = NULL;
        if (msg->node_id > 0 && msg->node_id < (uint32_t)ap->node_table_size) {
            target = ap->node_table[msg->node_id];
        }

        if (!target) continue;

        switch (msg->type) {
        case MSG_SET_PARAM:
            if (target->set_param) {
                target->set_param(target, msg->param, msg->value);
            }
            break;
        case MSG_NOTE_ON:
        case MSG_NOTE_OFF:
            /* Forwarded to VoiceNode via set_param convention */
            if (target->set_param) {
                target->set_param(target,
                    msg->type == MSG_NOTE_ON ? "note_on" : "note_off",
                    msg->type == MSG_NOTE_ON ? msg->velocity : 0.0f);
            }
            break;
        default:
            break;
        }
    }
}

/* Public send functions (main thread) */
void audio_pipeline_send_note_on(AudioPipeline* ap, uint32_t node_id,
                                  int note, float velocity) {
    ControlMsg msg = { .type = MSG_NOTE_ON, .node_id = node_id,
                        .note = note, .velocity = velocity };
    cq_push(ap, msg);
}

void audio_pipeline_send_note_off(AudioPipeline* ap, uint32_t node_id,
                                   int note) {
    (void)note;
    ControlMsg msg = { .type = MSG_NOTE_OFF, .node_id = node_id };
    cq_push(ap, msg);
}

void audio_pipeline_send_set_param(AudioPipeline* ap, uint32_t node_id,
                                    const char* param, float value) {
    ControlMsg msg = { .type = MSG_SET_PARAM, .node_id = node_id, .value = value };
    strncpy(msg.param, param, sizeof(msg.param) - 1);
    msg.param[sizeof(msg.param) - 1] = '\0';
    cq_push(ap, msg);
}

/* ------------------------------------------------------------------ */
/* Node ID management                                                  */
/* ------------------------------------------------------------------ */
uint32_t audio_pipeline_register_node(AudioPipeline* ap, AudioNode* node) {
    if (!ap || !node) return 0;

    /* Grow table if needed */
    if (ap->next_node_id >= (uint32_t)ap->node_table_size) {
        int new_size = ap->node_table_size * 2;
        AudioNode** new_table = realloc(ap->node_table,
                                         (size_t)new_size * sizeof(AudioNode*));
        if (!new_table) return 0;
        memset(new_table + ap->node_table_size, 0,
               (size_t)(new_size - ap->node_table_size) * sizeof(AudioNode*));
        ap->node_table = new_table;
        ap->node_table_size = new_size;
    }

    uint32_t id = ap->next_node_id++;
    ap->node_table[id] = node;
    node->node_id = id;
    return id;
}

/* ------------------------------------------------------------------ */
/* Probes                                                              */
/* ------------------------------------------------------------------ */
int audio_pipeline_add_probe(AudioPipeline* ap, uint32_t node_id,
                               int buf_size) {
    if (!ap || ap->probe_count >= 16) return -1;

    /* Find the node */
    AudioNode* node = NULL;
    if (node_id > 0 && node_id < (uint32_t)ap->node_table_size) {
        node = ap->node_table[node_id];
    }
    if (!node) return -1;

    /* Allocate ring buffer */
    float* buf = calloc((size_t)buf_size, sizeof(float));
    if (!buf) return -1;

    int idx = ap->probe_count++;
    ap->probes[idx].node_id   = node_id;
    ap->probes[idx].buffer    = buf;
    ap->probes[idx].size      = buf_size;
    ap->probes[idx].write_pos = 0;

    /* Wire to the node */
    node->probe_buffer = buf;
    node->probe_write  = 0;
    node->probe_size   = buf_size;

    return idx;
}

int audio_pipeline_probe_read(AudioPipeline* ap, int probe_index,
                                float* dst, int max_samples) {
    if (!ap || probe_index < 0 || probe_index >= ap->probe_count) return 0;

    ProbeSlot* p = &ap->probes[probe_index];
    int write = p->write_pos;

    /* Copy from 0..write in order */
    int available = write;
    if (available > max_samples) available = max_samples;
    if (available > 0) {
        memcpy(dst, p->buffer, (size_t)available * sizeof(float));
    }
    return available;
}

/* ------------------------------------------------------------------ */
/* Audio callback                                                      */
/* ------------------------------------------------------------------ */
void audio_pipeline_callback(float* output, int n_frames, int channels,
                               void* userdata) {
    AudioPipeline* ap = (AudioPipeline*)userdata;
    if (!ap || !ap->running) {
        memset(output, 0, (size_t)n_frames * (size_t)channels * sizeof(float));
        return;
    }

    /* 1. Drain control queue (non-blocking — audio thread only) */
    cq_drain(ap);

    /* 2. Zero the mix buffer (up to 4096 samples max) */
    int safe_frames = n_frames;
    if (safe_frames > 4096) safe_frames = 4096;
    memset(ap->mix_buffer, 0, (size_t)safe_frames * sizeof(float));

    /* 3. Process audio tree (post-order) */
    if (ap->root) {
        audio_node_process(ap->root, ap->mix_buffer, safe_frames);
    }

    /* 4. Update probes */
    for (int i = 0; i < ap->probe_count; i++) {
        uint32_t nid = ap->probes[i].node_id;
        if (nid > 0 && nid < (uint32_t)ap->node_table_size) {
            AudioNode* node = ap->node_table[nid];
            if (node && node->probe_buffer == ap->probes[i].buffer) {
                ap->probes[i].write_pos = node->probe_write;
            }
        }
    }

    /* 5. Copy to output (mono → interleaved if multi-channel) */
    for (int i = 0; i < safe_frames; i++) {
        float s = ap->mix_buffer[i];
        for (int ch = 0; ch < channels; ch++) {
            output[i * channels + ch] = s;
        }
    }
}
