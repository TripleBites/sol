#include "center_container.h"
#include <stdlib.h>
#include <math.h>

/* ------------------------------------------------------------------ */
/* Init / destroy                                                      */
/* ------------------------------------------------------------------ */
static void center_init(Node *self) {
    node_base_init(self, &center_container_class);
    CenterContainer *cc = (CenterContainer*)self;
    cc->base.anchor       = anchor_full_rect();
    cc->base.mouse_filter = MOUSE_FILTER_PASS;
    cc->base.base.flags  |= CONTROL_FLAG_CONTAINER;
}

static void center_destroy(Node *self) {
    node_base_destroy(self);
}

/* ------------------------------------------------------------------ */
/* Layout                                                              */
/* ------------------------------------------------------------------ */
static void center_get_minimum_size(Node *node, Vec2 *out) {
    Control *c = (Control*)node;

    /* Find the first visible child and use its min size */
    for (size_t i = 0; i < node->child_count; i++) {
        Node *child = node->children[i];
        if (!(child->flags & NODE_FLAG_VISIBLE)) continue;
        Vec2 ms = {0, 0};
        if (child->klass->get_minimum_size) {
            child->klass->get_minimum_size(child, &ms);
        } else {
            ms = control_get_min_size((Control*)child);
        }
        if (c->explicit_min_size.x > ms.x) ms.x = c->explicit_min_size.x;
        if (c->explicit_min_size.y > ms.y) ms.y = c->explicit_min_size.y;
        c->min_size.x = ms.x;
        c->min_size.y = ms.y;
        out->x = ms.x;
        out->y = ms.y;
        return;
    }

    /* No visible child */
    out->x = c->explicit_min_size.x;
    out->y = c->explicit_min_size.y;
    c->min_size.x = out->x;
    c->min_size.y = out->y;
}

static void center_arrange_children(Node *node) {
    Control *self = (Control*)node;

    /* Center each visible child within the container */
    for (size_t i = 0; i < node->child_count; i++) {
        Node *child = node->children[i];
        if (!(child->flags & NODE_FLAG_VISIBLE)) continue;
        Control *cc = (Control*)child;

        Vec2 ms = {0, 0};
        if (child->klass->get_minimum_size) {
            child->klass->get_minimum_size(child, &ms);
        } else {
            ms = control_get_min_size(cc);
        }

        /* Center the child at its minimum size */
        float child_w = ms.x;
        float child_h = ms.y;

        cc->rect.x = (self->rect.w - child_w) * 0.5f;
        cc->rect.y = (self->rect.h - child_h) * 0.5f;
        cc->rect.w = child_w;
        cc->rect.h = child_h;
    }
}

/* --- Draw --- */
static void center_draw(Node *self, DrawList *dl) {
    (void)self;
    (void)dl;
}

/* ------------------------------------------------------------------ */
/* VTable                                                              */
/* ------------------------------------------------------------------ */
const NodeClass center_container_class = {
    .type_name        = "CenterContainer",
    .instance_size    = sizeof(CenterContainer),
    .init             = center_init,
    .destroy          = center_destroy,
    .enter_tree       = NULL,
    .exit_tree        = NULL,
    .ready            = NULL,
    .process          = NULL,
    .draw             = center_draw,
    .get_minimum_size = center_get_minimum_size,
    .arrange_children = center_arrange_children,
    .handle_input     = NULL,
};

/* ------------------------------------------------------------------ */
/* Public API                                                          */
/* ------------------------------------------------------------------ */
CenterContainer *center_container_new(void) {
    return (CenterContainer*)node_new(&center_container_class);
}
