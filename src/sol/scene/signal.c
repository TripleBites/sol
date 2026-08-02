#define _POSIX_C_SOURCE 200809L
#include "signal.h"
#include "node.h"
#include <stdlib.h>
#include <string.h>
#include <assert.h>

/* ------------------------------------------------------------------ */
/* SignalConnection                                                    */
/* ------------------------------------------------------------------ */
typedef struct {
    SignalCallback callback;
    void          *userdata;
    int            flags;
    int            id;
} SignalConnection;

/* ------------------------------------------------------------------ */
/* Signal                                                              */
/* ------------------------------------------------------------------ */
struct Signal {
    char             *name;
    SignalConnection *connections;
    size_t            conn_count;
    size_t            conn_capacity;
    int               next_id;
};

Signal *signal_new(const char *name) {
    Signal *sig = calloc(1, sizeof(Signal));
    if (!sig) return NULL;
    sig->name = name ? strdup(name) : NULL;
    sig->next_id = 1;
    return sig;
}

void signal_free(Signal *sig) {
    if (!sig) return;
    free(sig->name);
    free(sig->connections);
    free(sig);
}

const char *signal_get_name(const Signal *sig) {
    return sig ? sig->name : NULL;
}

/* ------------------------------------------------------------------ */
/* Connection management                                               */
/* ------------------------------------------------------------------ */
int signal_connect(Signal *sig, SignalCallback cb, void *userdata, int flags) {
    if (!sig || !cb) return -1;

    if (sig->conn_count >= sig->conn_capacity) {
        size_t new_cap = sig->conn_capacity ? sig->conn_capacity * 2 : 4;
        SignalConnection *tmp = realloc(sig->connections,
                                        sizeof(SignalConnection) * new_cap);
        if (!tmp) return -1;
        sig->connections = tmp;
        sig->conn_capacity = new_cap;
    }

    int id = sig->next_id++;
    SignalConnection *sc = &sig->connections[sig->conn_count++];
    sc->callback = cb;
    sc->userdata = userdata;
    sc->flags    = flags;
    sc->id       = id;
    return id;
}

void signal_disconnect(Signal *sig, int conn_id) {
    if (!sig) return;
    for (size_t i = 0; i < sig->conn_count; i++) {
        if (sig->connections[i].id == conn_id) {
            size_t remaining = sig->conn_count - i - 1;
            if (remaining > 0) {
                memmove(&sig->connections[i], &sig->connections[i + 1],
                        sizeof(SignalConnection) * remaining);
            }
            sig->conn_count--;
            return;
        }
    }
}

void signal_disconnect_all(Signal *sig) {
    if (!sig) return;
    sig->conn_count = 0;
}

/* ------------------------------------------------------------------ */
/* Emission                                                            */
/* ------------------------------------------------------------------ */
void signal_emit(Signal *sig, Node *emitter, const Variant *args, size_t arg_count) {
    if (!sig) return;

    /* We iterate carefully because callbacks may disconnect themselves */
    size_t i = 0;
    while (i < sig->conn_count) {
        SignalConnection *sc = &sig->connections[i];
        bool one_shot = (sc->flags & CONNECT_ONE_SHOT) != 0;

        if (sc->flags & CONNECT_DEFERRED) {
            /* Deferred: queue for later. The emitter's tree handles flushing. */
            Node *root = emitter;
            while (root->parent) root = root->parent;
            /* The SceneTree stores a deferred queue — we access it through
               a known location. For now, we call immediately and let SceneTree
               integration handle deferring. */
            /* TODO: wire to SceneTree's deferred queue */
            sc->callback(emitter, args, arg_count, sc->userdata);
        } else {
            sc->callback(emitter, args, arg_count, sc->userdata);
        }

        if (one_shot) {
            /* Remove this connection (sc is invalid after this) */
            signal_disconnect(sig, sc->id);
            /* Don't increment i — the next element shifted into this position */
        } else {
            i++;
        }
    }
}

/* ------------------------------------------------------------------ */
/* Deferred Queue                                                      */
/* ------------------------------------------------------------------ */
typedef struct {
    SignalCallback callback;
    Node          *emitter;
    Variant        args[8];     /* small fixed size for UI signals */
    size_t         arg_count;
    void          *userdata;
    int            flags;
    Signal        *signal;
    int            conn_id;
} DeferredCallEntry;

struct DeferredQueue {
    DeferredCallEntry *entries;
    size_t             count;
    size_t             capacity;
};

DeferredQueue *deferred_queue_create(void) {
    return calloc(1, sizeof(DeferredQueue));
}

void deferred_queue_destroy(DeferredQueue *q) {
    if (!q) return;
    free(q->entries);
    free(q);
}

void deferred_queue_push(DeferredQueue *q, SignalCallback cb, Node *emitter,
                         const Variant *args, size_t arg_count, void *userdata,
                         int flags, Signal *sig, int conn_id) {
    if (!q) return;

    if (q->count >= q->capacity) {
        size_t new_cap = q->capacity ? q->capacity * 2 : 8;
        DeferredCallEntry *tmp = realloc(q->entries,
                                          sizeof(DeferredCallEntry) * new_cap);
        if (!tmp) return;
        q->entries    = tmp;
        q->capacity   = new_cap;
    }

    DeferredCallEntry *e = &q->entries[q->count++];
    e->callback  = cb;
    e->emitter   = emitter;
    e->arg_count = arg_count < 8 ? arg_count : 8;
    for (size_t i = 0; i < e->arg_count; i++) e->args[i] = args[i];
    e->userdata  = userdata;
    e->flags     = flags;
    e->signal    = sig;
    e->conn_id   = conn_id;
}

void deferred_queue_flush(DeferredQueue *q) {
    if (!q) return;
    for (size_t i = 0; i < q->count; i++) {
        DeferredCallEntry *e = &q->entries[i];
        e->callback(e->emitter, e->args, e->arg_count, e->userdata);

        /* Handle ONE_SHOT */
        if (e->flags & CONNECT_ONE_SHOT) {
            signal_disconnect(e->signal, e->conn_id);
        }
    }
    q->count = 0;
}
