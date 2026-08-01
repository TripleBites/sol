#define _POSIX_C_SOURCE 200809L
#include "node.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <assert.h>

/* ------------------------------------------------------------------ */
/* Internal helpers                                                    */
/* ------------------------------------------------------------------ */
static void enter_tree_recursive(Node *node) {
    if (node->klass->enter_tree) node->klass->enter_tree(node);
    node->flags |= NODE_FLAG_IN_TREE;
    for (size_t i = 0; i < node->child_count; i++) {
        enter_tree_recursive(node->children[i]);
    }
}

static void ready_recursive(Node *node) {
    for (size_t i = 0; i < node->child_count; i++) {
        ready_recursive(node->children[i]);
    }
    if (!(node->flags & NODE_FLAG_READY)) {
        if (node->klass->ready) node->klass->ready(node);
        node->flags |= NODE_FLAG_READY;
    }
}

static void exit_tree_recursive(Node *node) {
    for (size_t i = 0; i < node->child_count; i++) {
        exit_tree_recursive(node->children[i]);
    }
    if (node->klass->exit_tree) node->klass->exit_tree(node);
    node->flags &= ~(NODE_FLAG_IN_TREE | NODE_FLAG_READY);
}

/* ------------------------------------------------------------------ */
/* Allocation / refcounting                                            */
/* ------------------------------------------------------------------ */
Node *node_new(const NodeClass *klass) {
    Node *node = calloc(1, klass->instance_size);
    if (!node) return NULL;
    node->klass = klass;
    node->refcount = 1;
    if (klass->init) klass->init(node);
    return node;
}

Node *node_ref(Node *node) {
    if (node) node->refcount++;
    return node;
}

void node_unref(Node *node) {
    if (!node) return;
    node->refcount--;
    if (node->refcount <= 0) {
        if (node->klass->destroy) node->klass->destroy(node);
        free(node->name);
        free(node->children);
        free(node);
    }
}

/* ------------------------------------------------------------------ */
/* Base init / destroy (called by subclasses)                          */
/* ------------------------------------------------------------------ */
void node_base_init(Node *node, const NodeClass *klass) {
    node->klass     = klass;
    node->flags     = NODE_FLAG_VISIBLE | NODE_FLAG_PROCESS;
    node->refcount  = 1;
}

void node_base_destroy(Node *node) {
    while (node->child_count > 0) {
        Node *child = node->children[node->child_count - 1];
        node_remove_child(node, child);
    }
}

/* ------------------------------------------------------------------ */
/* Tree operations                                                     */
/* ------------------------------------------------------------------ */
void node_add_child(Node *parent, Node *child) {
    if (!parent || !child) return;
    assert(child->parent == NULL && "child already has a parent");

    if (parent->child_count >= parent->child_capacity) {
        size_t new_cap = parent->child_capacity ? parent->child_capacity * 2 : 4;
        Node **tmp = realloc(parent->children, sizeof(Node*) * new_cap);
        if (!tmp) return;
        parent->children = tmp;
        parent->child_capacity = new_cap;
    }

    parent->children[parent->child_count++] = child;
    child->parent = parent;
    node_ref(child);

    /* If parent is in the tree, trigger enter_tree + ready on child subtree */
    if (parent->flags & NODE_FLAG_IN_TREE) {
        enter_tree_recursive(child);
        ready_recursive(child);
        /* Note: we enter_tree the child itself AND all its descendants,
           then ready all of them post-order. This matches the spec:
           enter_tree pre-order, ready post-order. */
    }
}

void node_remove_child(Node *parent, Node *child) {
    if (!parent || !child) return;

    for (size_t i = 0; i < parent->child_count; i++) {
        if (parent->children[i] == child) {
            exit_tree_recursive(child);

            size_t remaining = parent->child_count - i - 1;
            if (remaining > 0) {
                memmove(&parent->children[i], &parent->children[i + 1],
                        sizeof(Node*) * remaining);
            }
            parent->child_count--;
            child->parent = NULL;
            node_unref(child);
            return;
        }
    }
}

void node_free(Node *node) {
    if (!node) return;
    if (node->parent) {
        node_remove_child(node->parent, node);
    }
    node_unref(node);
}

/* ------------------------------------------------------------------ */
/* Name                                                                */
/* ------------------------------------------------------------------ */
void node_set_name(Node *node, const char *name) {
    if (!node) return;
    free(node->name);
    node->name = name ? strdup(name) : NULL;
}

/* ------------------------------------------------------------------ */
/* Type check                                                          */
/* ------------------------------------------------------------------ */
bool node_is_type(const Node *node, const char *type_name) {
    if (!node || !type_name) return false;
    return strcmp(node->klass->type_name, type_name) == 0;
}

/* ------------------------------------------------------------------ */
/* Traversal                                                           */
/* ------------------------------------------------------------------ */
void node_traverse_preorder(Node *node, NodeVisitor visit, void *userdata) {
    if (!node) return;
    if (visit) visit(node, userdata);
    for (size_t i = 0; i < node->child_count; i++) {
        node_traverse_preorder(node->children[i], visit, userdata);
    }
}

void node_traverse_postorder(Node *node, NodeVisitor visit, void *userdata) {
    if (!node) return;
    for (size_t i = 0; i < node->child_count; i++) {
        node_traverse_postorder(node->children[i], visit, userdata);
    }
    if (visit) visit(node, userdata);
}
