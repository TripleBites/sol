#ifndef SOL_UI_SIGNAL_H
#define SOL_UI_SIGNAL_H

#include "variant.h"
#include <stddef.h>
#include <stdint.h>

/* Forward declarations */
typedef struct Node Node;

/* --- Connection flags --- */
#define CONNECT_NORMAL   0
#define CONNECT_ONE_SHOT 1   /* auto-disconnect after first fire */
#define CONNECT_DEFERRED 2   /* queue for next process frame */

/* --- Callback signature --- */
typedef void (*SignalCallback)(Node *emitter, const Variant *args, size_t arg_count, void *userdata);

/* --- Signal --- */
typedef struct Signal Signal;

Signal *signal_new(const char *name);
void    signal_free(Signal *sig);
const char *signal_get_name(const Signal *sig);

/* Connection management */
int  signal_connect(Signal *sig, SignalCallback cb, void *userdata, int flags);
void signal_disconnect(Signal *sig, int conn_id);
void signal_disconnect_all(Signal *sig);

/* Immediate emission */
void signal_emit(Signal *sig, Node *emitter, const Variant *args, size_t arg_count);

/* --- Deferred call queue (for CONNECT_DEFERRED) --- */
typedef struct DeferredCall DeferredCall;
typedef struct DeferredQueue DeferredQueue;

DeferredQueue *deferred_queue_create(void);
void           deferred_queue_destroy(DeferredQueue *q);

/* Enqueue a deferred call (called by signal_emit when connection is DEFERRED) */
void deferred_queue_push(DeferredQueue *q, SignalCallback cb, Node *emitter,
                         const Variant *args, size_t arg_count, void *userdata,
                         int flags, Signal *sig, int conn_id);

/* Drain all deferred calls. Called by SceneTree between process and layout. */
void deferred_queue_flush(DeferredQueue *q);

#endif /* SOL_UI_SIGNAL_H */
