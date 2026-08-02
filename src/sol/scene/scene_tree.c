#include "scene_tree.h"
#include "control.h"
#include "signal.h"
#include <stdlib.h>
#include <stdio.h>

/* ------------------------------------------------------------------ */
/* Create / destroy                                                    */
/* ------------------------------------------------------------------ */
SceneTree *scene_tree_create(void) {
    SceneTree *tree = calloc(1, sizeof(SceneTree));
    tree->draw_list = draw_list_create();
    tree->deferred_queue = deferred_queue_create();
    return tree;
}

void scene_tree_destroy(SceneTree *tree) {
    if (!tree) return;
    if (tree->root) node_unref(tree->root);
    draw_list_destroy(tree->draw_list);
    deferred_queue_destroy(tree->deferred_queue);
    free(tree);
}

/* Helper: enter_tree recursively (pre-order) */
static void set_root_enter(Node *n) {
    if (n->klass->enter_tree) n->klass->enter_tree(n);
    n->flags |= NODE_FLAG_IN_TREE;
    for (size_t i = 0; i < n->child_count; i++) {
        set_root_enter(n->children[i]);
    }
}

/* Helper: ready recursively (post-order) */
static void set_root_ready(Node *n) {
    for (size_t i = 0; i < n->child_count; i++) {
        set_root_ready(n->children[i]);
    }
    if (!(n->flags & NODE_FLAG_READY)) {
        if (n->klass->ready) n->klass->ready(n);
        n->flags |= NODE_FLAG_READY;
    }
}

void scene_tree_set_root(SceneTree *tree, Node *root) {
    if (tree->root) {
        node_unref(tree->root);
    }
    tree->root = root;
    if (root) {
        node_ref(root);
        set_root_enter(root);
        set_root_ready(root);
    }
    tree->layout_dirty = true;
}

/* ------------------------------------------------------------------ */
/* Layout (two-pass: measure → arrange)                                */
/* ------------------------------------------------------------------ */
static void measure_pass(Node *node);
static void arrange_pass(Node *node, Rect parent_rect);

static void measure_pass(Node *node) {
    /* Measure children first (bottom-up, post-order) */
    for (size_t i = 0; i < node->child_count; i++) {
        measure_pass(node->children[i]);
    }

    /* Compute own min_size. Containers aggregate children; leaves use explicit.
       Store into Control.min_size so arrange_pass can read it. */
    if (node->klass->get_minimum_size) {
        Vec2 sz;
        node->klass->get_minimum_size(node, &sz);
        /* If this is a Control, cache the computed min_size */
        Control *c = (Control*)node;
        c->min_size = sz;
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

    /* Flush deferred signal calls */
    if (tree->deferred_queue) {
        deferred_queue_flush(tree->deferred_queue);
    }

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
/* Heuristic: any node whose vtable provides get_minimum_size, draw, or handle_input
   is a Control-derived type. This safely handles widgets that don't override
   get_minimum_size while excluding non-UI nodes like AudioNode. */
static bool node_is_control_kind(const Node *node) {
    return node->klass->get_minimum_size != NULL
        || node->klass->draw != NULL
        || node->klass->handle_input != NULL;
}

static void print_node(Node *node, int depth) {
    for (int i = 0; i < depth; i++) printf("  ");
    if (node_is_control_kind(node)) {
        Control *c = (Control*)node;
        printf("[%s] \"%s\" rect={%.0f,%.0f,%.0f,%.0f} global={%.0f,%.0f,%.0f,%.0f}\n",
               node->klass->type_name,
               node->name ? node->name : "",
               c->rect.x, c->rect.y, c->rect.w, c->rect.h,
               c->global_rect.x, c->global_rect.y, c->global_rect.w, c->global_rect.h);
    } else {
        printf("[%s] \"%s\"\n",
               node->klass->type_name,
               node->name ? node->name : "");
    }
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

/* ------------------------------------------------------------------ */
/* Hit-test: find the deepest Control under a screen-space point.     */
/* Top-down search, returns the last (deepest) Control that contains   */
/* the point. Visible-only, ignores MOUSE_FILTER_IGNORE.              */
/* ------------------------------------------------------------------ */
static Node *hit_test_recursive(Node *node, Vec2 pos);

Node *scene_tree_hit_test(const SceneTree *tree, Vec2 screen_pos) {
    if (!tree->root) return NULL;
    return hit_test_recursive(tree->root, screen_pos);
}

static Node *hit_test_recursive(Node *node, Vec2 pos) {
    if (!node || !(node->flags & NODE_FLAG_VISIBLE)) return NULL;

    /* Check if this is a Control */
    bool is_ctrl = node_is_control_kind(node);

    if (is_ctrl) {
        Control *c = (Control*)node;

        /* Ignore controls with MOUSE_FILTER_IGNORE */
        if (c->mouse_filter == MOUSE_FILTER_IGNORE) return NULL;

        /* Bounds check */
        if (pos.x < c->global_rect.x ||
            pos.y < c->global_rect.y ||
            pos.x > c->global_rect.x + c->global_rect.w ||
            pos.y > c->global_rect.y + c->global_rect.h) {
            return NULL;
        }

        /* Search children in reverse draw order (later children on top) */
        for (size_t i = node->child_count; i > 0; i--) {
            Node *hit = hit_test_recursive(node->children[i - 1], pos);
            if (hit) return hit;
        }

        /* No child hit — this Control is the target */
        return node;
    } else {
        /* Non-Control node: recurse into children */
        for (size_t i = node->child_count; i > 0; i--) {
            Node *hit = hit_test_recursive(node->children[i - 1], pos);
            if (hit) return hit;
        }
        return NULL;
    }
}

/* ------------------------------------------------------------------ */
/* Input event routing                                                 */
/* ------------------------------------------------------------------ */

/* Check if a Control accepts focus */
static bool control_accepts_focus(const Control *c) {
    return c->focus_mode == FOCUS_CLICK || c->focus_mode == FOCUS_ALL;
}

void scene_tree_input(SceneTree *tree, const UIInputEvent *ev) {
    if (!tree || !tree->root || !ev) return;

    switch (ev->type) {
    case UI_EV_MOUSE_MOTION: {
        /* Find the control under the cursor */
        Node *hit = hit_test_recursive(tree->root, ev->pos);

        /* Handle enter/leave */
        if (hit != tree->hovered_control) {
            /* Leave old */
            if (tree->hovered_control && tree->hovered_control->klass->handle_input) {
                UIInputEvent leave_ev = *ev;
                leave_ev.type = UI_EV_FOCUS_EXIT;
                tree->hovered_control->klass->handle_input(tree->hovered_control, &leave_ev);
            }

            /* Enter new */
            if (hit && hit->klass->handle_input) {
                UIInputEvent enter_ev = *ev;
                enter_ev.type = UI_EV_FOCUS_ENTER;
                hit->klass->handle_input(hit, &enter_ev);
            }

            tree->hovered_control = hit;
        }

        /* Dispatch motion to hit target */
        if (hit && hit->klass->handle_input) {
            hit->klass->handle_input(hit, ev);
        }
        break;
    }

    case UI_EV_MOUSE_BUTTON: {
        Node *hit = hit_test_recursive(tree->root, ev->pos);

        /* Focus on click */
        if (ev->pressed && hit && node_is_control_kind(hit)) {
            Control *c = (Control*)hit;
            if (c->focus_mode == FOCUS_CLICK || c->focus_mode == FOCUS_ALL) {
                /* Blur previous focus */
                if (tree->focused_control && tree->focused_control != hit) {
                    if (tree->focused_control->klass->handle_input) {
                        UIInputEvent focus_ev = *ev;
                        focus_ev.type = UI_EV_FOCUS_EXIT;
                        tree->focused_control->klass->handle_input(tree->focused_control, &focus_ev);
                    }
                }

                /* Focus new */
                if (hit != tree->focused_control && hit->klass->handle_input) {
                    UIInputEvent focus_ev = *ev;
                    focus_ev.type = UI_EV_FOCUS_ENTER;
                    hit->klass->handle_input(hit, &focus_ev);
                }

                tree->focused_control = hit;
            }
        }

        /* Dispatch to target, bubble if unhandled */
        if (hit) {
            Node *target = hit;
            while (target) {
                if (target->klass->handle_input) {
                    int handled = target->klass->handle_input(target, ev);
                    if (handled) break;  /* STOP */
                }

                /* Check mouse filter */
                if (node_is_control_kind(target)) {
                    Control *tc = (Control*)target;
                    if (tc->mouse_filter == MOUSE_FILTER_STOP) break;
                }

                target = target->parent;
            }
        }
        break;
    }

    case UI_EV_KEY:
    case UI_EV_TEXT: {
        /* Route keyboard to focused control, bubble if unhandled */
        if (tree->focused_control) {
            Node *target = tree->focused_control;
            while (target) {
                if (target->klass->handle_input) {
                    int handled = target->klass->handle_input(target, ev);
                    if (handled) break;
                }
                target = target->parent;
            }
        }
        break;
    }

    default:
        break;
    }
}
