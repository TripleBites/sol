#ifndef SOL_UI_NODE_H
#define SOL_UI_NODE_H

#include "types.h"
#include "variant.h"
#include "input_event.h"
#include <stddef.h>
#include <stdint.h>
#include <stdbool.h>

/* Forward declarations */
typedef struct Node Node;
typedef struct NodeClass NodeClass;
typedef struct Signal Signal;
struct DrawList;

/* --- Node flags --- */
enum {
    NODE_FLAG_VISIBLE  = 1 << 0,
    NODE_FLAG_PROCESS  = 1 << 1,
    NODE_FLAG_IN_TREE  = 1 << 2,
    NODE_FLAG_READY    = 1 << 3,  /* ready() has been called */
};

/* --- Virtual method table --- */
struct NodeClass {
    const char *type_name;
    size_t       instance_size;

    /* Lifecycle */
    void (*init)(Node *self);
    void (*destroy)(Node *self);
    void (*enter_tree)(Node *self);
    void (*exit_tree)(Node *self);
    void (*ready)(Node *self);

    /* Per-frame */
    void (*process)(Node *self, float delta);
    void (*draw)(Node *self, struct DrawList *dl);

    /* Layout */
    void (*get_minimum_size)(Node *self, Vec2 *out);
    void (*arrange_children)(Node *self);   /* containers only */

    /* Input */
    int (*handle_input)(Node *self, const UIInputEvent *ev);
};

/* --- Base Node --- */
struct Node {
    const NodeClass *klass;
    Node            *parent;         /* weak reference */
    Node           **children;
    size_t           child_count;
    size_t           child_capacity;
    char            *name;
    uint32_t         flags;
    void            *user_data;
    int              refcount;

    /* Signals (dynamic array of Signal*) */
    Signal         **signals;
    size_t           signal_count;
    size_t           signal_capacity;
};

/* --- Allocation --- */
Node *node_new(const NodeClass *klass);
Node *node_ref(Node *node);
void  node_unref(Node *node);

/* --- Tree operations --- */
void node_add_child(Node *parent, Node *child);
void node_remove_child(Node *parent, Node *child);
void node_free(Node *node);

/* --- Name --- */
void node_set_name(Node *node, const char *name);

/* --- Type check --- */
bool node_is_type(const Node *node, const char *type_name);

/* --- Signal management --- */
Signal *node_get_signal(Node *node, const char *name);
Signal *node_add_signal(Node *node, const char *name);
void    node_emit_signal(Node *node, const char *name, const Variant *args, size_t arg_count);

/* --- Internal helpers (called by subclass init/destroy) --- */
void node_base_init(Node *node, const NodeClass *klass);
void node_base_destroy(Node *node);

/* --- Traversal --- */
typedef void (*NodeVisitor)(Node *node, void *userdata);
void node_traverse_preorder(Node *node, NodeVisitor visit, void *userdata);
void node_traverse_postorder(Node *node, NodeVisitor visit, void *userdata);

#endif /* SOL_UI_NODE_H */
