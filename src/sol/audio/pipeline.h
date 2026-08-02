#ifndef SOL_AUDIO_PIPELINE_H
#define SOL_AUDIO_PIPELINE_H

#include "audio_node.h"
#include "../io/io.h"

/* --- Control message types --- */
typedef enum {
    MSG_NOTE_ON = 0,
    MSG_NOTE_OFF,
    MSG_SET_PARAM,
    MSG_SWAP_NODE,
} ControlMsgType;

/* --- Control message ---
   Sent from main thread → audio thread via lock-free SPSC ring buffer. */
typedef struct {
    ControlMsgType type;
    uint32_t node_id;         /* target AudioNode */
    int      note;            /* for note_on/off: MIDI note number */
    float    velocity;        /* for note_on: 0.0–1.0 */
    char     param[32];       /* for set_param: parameter name */
    float    value;           /* for set_param: new value */
} ControlMsg;

/* --- Probe slot ---
   One per probed node. Audio thread writes; GUI thread reads.
   Single-writer (audio) / single-reader (GUI) — no lock needed. */
typedef struct {
    uint32_t node_id;
    float*   buffer;          /* ring buffer, allocated once */
    int      size;            /* total capacity in samples */
    int      write_pos;       /* audio thread advances */
} ProbeSlot;

/* --- AudioPipeline --- */
typedef struct {
    AudioNode* root;

    /* Control queue — SPSC ring buffer */
    ControlMsg* cq;
    int         cq_capacity;
    int         cq_read;      /* audio thread */
    int         cq_write;     /* main thread */

    /* Audio state */
    int    sample_rate;
    int    buffer_size;       /* frames per callback */
    float* mix_buffer;        /* working buffer, preallocated */

    /* Node ID → pointer mapping */
    AudioNode** node_table;
    int         node_table_size;
    uint32_t    next_node_id;

    /* Probes */
    ProbeSlot probes[16];
    int       probe_count;

    /* Platform */
    const SolIO* platform;
    bool         running;

    /* Audio stream state for main-thread sync */
    void* stream_state;       /* opaque, platform-specific */
} AudioPipeline;

/* --- API --- */
AudioPipeline* audio_pipeline_new(int sample_rate, int buffer_size);
void           audio_pipeline_free(AudioPipeline* ap);

/* Set the root AudioNode. Pipeline takes ownership (refs it). */
void audio_pipeline_set_root(AudioPipeline* ap, AudioNode* root);

/* Start / stop audio processing */
bool audio_pipeline_start(AudioPipeline* ap, const SolIO* platform);
void audio_pipeline_stop(AudioPipeline* ap);

/* --- Control messages (main thread → audio thread) --- */
void audio_pipeline_send_note_on(AudioPipeline* ap, uint32_t node_id,
                                  int note, float velocity);
void audio_pipeline_send_note_off(AudioPipeline* ap, uint32_t node_id, int note);
void audio_pipeline_send_set_param(AudioPipeline* ap, uint32_t node_id,
                                    const char* param, float value);

/* --- Probe access (main thread / GUI) --- */
int   audio_pipeline_add_probe(AudioPipeline* ap, uint32_t node_id, int buf_size);
int   audio_pipeline_probe_read(AudioPipeline* ap, int probe_index,
                                 float* dst, int max_samples);

/* --- Node ID management --- */
uint32_t audio_pipeline_register_node(AudioPipeline* ap, AudioNode* node);

/* --- The audio callback (passed to SolIO.audio_init) ---
   This is the real-time entry point called from the audio thread. */
void audio_pipeline_callback(float* output, int n_frames, int channels,
                              void* userdata);

#endif /* SOL_AUDIO_PIPELINE_H */
