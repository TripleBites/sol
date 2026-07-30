#include "scene_tree.h"
#include "control.h"
#include <stdlib.h>
#include <stdio.h>

/* ------------------------------------------------------------------ */
/* Create / destroy                                                    */
/* ------------------------------------------------------------------ */
SceneTree *scene_tree_create(void) {
    SceneTree *tree = calloc(1, sizeof(SceneTree));
    tree->draw_list = draw_list_create();
    return tree;
}

void scene_tree_destroy(SceneTree *tree) {
    if (!tree) return;
    if (tree->root) node_unref(tree->root);
    draw_list_destroy(tree->draw_list);
    free(tree);
}

void scene_tree_set_root(SceneTree *tree, Node *root) {
    if (tree->root) {
        node_unref(tree->root);
    }
    tree->root = root;
    if (root) {
        node_ref(root);
        /* Mark root as in-tree and trigger ready */
        root->flags |= NODE_FLAG_IN_TREE;
        /* enter_tree recursively */
        void enter(Node *n) {
            if (n->klass->enter_tree) n->klass->enter_tree(n);
            n->flags |= NODE_FLAG_IN_TREE;
            for (size_t i = 0; i < n->child_count; i++) enter(n->children[i]);
        }
        enter(root);

        /* ready recursively (post-order) */
        void ready(Node *n) {
            for (size_t i = 0; i < n->child_count; i++) ready(n->children[i]);
            if (!(n->flags & NODE_FLAG_READY)) {
                if (n->klass->ready) n->klass->ready(n);
                n->flags |= NODE_FLAG_READY;
            }
        }
        ready(root);
    }
    tree->layout_dirty = true;
}

/* ------------------------------------------------------------------ */
/* Layout (two-pass: measure → arrange)                                */
/* ------------------------------------------------------------------ */
static void measure_pass(Node *node);
static void arrange_pass(Node *node, Rect parent_rect);

static void measure_pass(Node *node) {
    /* Measure children first (bottom-up) */
    for (size_t i = 0; i < node->child_count; i++) {
        measure_pass(node->children[i]);
    }

    /* Then compute own min_size (containers use children's sizes) */
    if (node->klass->get_minimum_size) {
        Vec2 sz;
        node->klass->get_minimum_size(node, &sz);
    }
}

static void arrange_pass(Node *node, Rect parent_rect) {
    Control *c = (Control*)node;

    /* Is this node a Control? */
    if (node->klass == &control_class ||
        node->klass->type_name != NULL) {  /* All our UI nodes are Controls */
        /* Check if parent is a container (has CONTROL_FLAG_CONTAINER) */
        bool parent_is_container = node->parent &&
            (node->parent->flags & CONTROL_FLAG_CONTAINER);

        if (!parent_is_container) {
            control_compute_rect_from_anchors(c, parent_rect);
        }
        /* If parent IS a container, rect was already set by arrange_children */

        control_compute_global_rect(c, &parent_rect);
    }

    /* Container::arrange_children */
    if (node->klass->arrange_children) {
        node->klass->arrange_children(node);
    }

    /* Recurse into children */
    Rect child_parent = (c && node->klass != NULL) ? c->rect : parent_rect;
    for (size_t i = 0; i < node->child_count; i++) {
        arrange_pass(node->children[i], child_parent);
    }
}

void scene_tree_mark_layout_dirty(SceneTree *tree) {
    tree->layout_dirty = true;
}

/* ------------------------------------------------------------------ */
/* Process                                                             */
/* ------------------------------------------------------------------ */
static void process_pass(Node *node, float delta) {
    if (node->klass->process) {
        node->klass->process(node, delta);
    }
    for (size_t i = 0; i < node->child_count; i++) {
        process_pass(node->children[i], delta);
    }
}

void scene_tree_process(SceneTree *tree, float delta) {
    if (!tree->root) return;

    /* Process */
    process_pass(tree->root, delta);

    /* Layout (if dirty) */
    if (tree->layout_dirty) {
        measure_pass(tree->root);
        arrange_pass(tree->root, ((Control*)tree->root)->rect);
        tree->layout_dirty = false;
    }

    /* Draw */
    scene_tree_draw(tree);
}

/* ------------------------------------------------------------------ */
/* Draw                                                                */
/* ------------------------------------------------------------------ */
static void draw_pass(Node *node, DrawList *dl) {
    if (!(node->flags & NODE_FLAG_VISIBLE)) return;

    /* If this is a Control, handle drawing */
    Control *c = (Control*)node;
    bool is_ctrl = (node->klass == &control_class ||
                    node->klass->type_name != NULL);

    if (is_ctrl) {
        /* Push clip */
        draw_list_push_clip(dl, c->global_rect);

        /* Call the node's draw vtable */
        if (node->klass->draw) {
            node->klass->draw(node, dl);
        }

        /* Draw children inside this clip */
        for (size_t i = 0; i < node->child_count; i++) {
            draw_pass(node->children[i], dl);
        }

        draw_list_pop_clip(dl);
    } else {
        /* Non-control node: just recurse */
        for (size_t i = 0; i < node->child_count; i++) {
            draw_pass(node->children[i], dl);
        }
    }
}

void scene_tree_draw(SceneTree *tree) {
    if (!tree->root) return;
    draw_list_clear(tree->draw_list);
    draw_pass(tree->root, tree->draw_list);
}

DrawList *scene_tree_get_draw_list(SceneTree *tree) {
    return tree->draw_list;
}

/* ------------------------------------------------------------------ */
/* Debug                                                               */
/* ------------------------------------------------------------------ */
static void print_node(Node *node, int depth) {
    for (int i = 0; i < depth; i++) printf("  ");
    Control *c = (Control*)node;
    printf("[%s] \"%s\" rect={%.0f,%.0f,%.0f,%.0f} global={%.0f,%.0f,%.0f,%.0f}\n",
           node->klass->type_name,
           node->name ? node->name : "",
           c->rect.x, c->rect.y, c->rect.w, c->rect.h,
           c->global_rect.x, c->global_rect.y, c->global_rect.w, c->global_rect.h);
    for (size_t i = 0; i < node->child_count; i++) {
        print_node(node->children[i], depth + 1);
    }
}

void scene_tree_print(const SceneTree *tree) {
    if (!tree->root) {
        printf("[SceneTree] (empty)\n");
        return;
    }
    printf("[SceneTree]\n");
    print_node(tree->root, 0);
}
