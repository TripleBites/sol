#include "hbox_container.h"
#include <stdlib.h>
#include <math.h>

/* ------------------------------------------------------------------ */
/* Init / destroy                                                      */
/* ------------------------------------------------------------------ */
static void hbox_init(Node *self) {
    node_base_init(self, &hbox_container_class);
    HBoxContainer *hb = (HBoxContainer*)self;
    hb->base.anchor       = anchor_full_rect();
    hb->base.mouse_filter = MOUSE_FILTER_PASS;
    hb->separation        = 4.0f;
    hb->base.base.flags  |= CONTROL_FLAG_CONTAINER;
}

static void hbox_destroy(Node *self) {
    node_base_destroy(self);
}

/* ------------------------------------------------------------------ */
/* Layout                                                              */
/* ------------------------------------------------------------------ */
static void hbox_get_minimum_size(Node *node, Vec2 *out) {
    HBoxContainer *hb = (HBoxContainer*)node;
    Control *c = (Control*)node;
    float total_w = 0;
    float max_h = 0;
    size_t visible = 0;

    for (size_t i = 0; i < node->child_count; i++) {
        Node *child = node->children[i];
        if (!(child->flags & NODE_FLAG_VISIBLE)) continue;
        Vec2 ms = {0, 0};
        if (child->klass->get_minimum_size) {
            child->klass->get_minimum_size(child, &ms);
        } else {
            ms = control_get_min_size((Control*)child);
        }
        total_w += ms.x;
        if (ms.y > max_h) max_h = ms.y;
        visible++;
    }

    if (visible > 0) {
        total_w += hb->separation * (visible - 1);
    }
    total_w += c->offset.left + c->offset.right;
    max_h   += c->offset.top  + c->offset.bottom;

    if (c->explicit_min_size.x > total_w) total_w = c->explicit_min_size.x;
    if (c->explicit_min_size.y > max_h)   max_h   = c->explicit_min_size.y;

    c->min_size.x = total_w;
    c->min_size.y = max_h;
    out->x = total_w;
    out->y = max_h;
}

static void hbox_arrange_children(Node *node) {
    HBoxContainer *hb = (HBoxContainer*)node;
    Control *self = (Control*)node;

    /* Count visible children and compute expand/fill ratios */
    size_t visible = 0;
    float total_min_w = 0;
    float max_h = 0;
    int expand_count = 0;

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
        total_min_w += ms.x;
        if (ms.y > max_h) max_h = ms.y;

        if (cc->size_flags_h & SIZE_EXPAND) expand_count++;
        visible++;
    }

    if (visible == 0) return;

    float separation_total = hb->separation * (visible - 1);
    float available_w = self->rect.w - separation_total;
    float extra_w = available_w - total_min_w;
    if (extra_w < 0) extra_w = 0;

    float expand_share = (expand_count > 0) ? extra_w / expand_count : 0;

    /* Find the index of the last visible child */
    size_t last_visible = 0;
    for (size_t i = node->child_count; i > 0; i--) {
        if (node->children[i-1]->flags & NODE_FLAG_VISIBLE) {
            last_visible = i - 1;
            break;
        }
    }

    float x = 0;
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

        float child_w = ms.x;
        if (cc->size_flags_h & SIZE_EXPAND) {
            child_w += expand_share;
        }
        /* SIZE_FILL without EXPAND: only the last visible child gets the
           remaining space; earlier children stay at minimum width. */
        if ((cc->size_flags_h & SIZE_FILL) && !(cc->size_flags_h & SIZE_EXPAND)
            && i == last_visible) {
            child_w = self->rect.w - x;
        }

        float child_h = ms.y;
        if (cc->size_flags_v & SIZE_FILL) {
            child_h = self->rect.h;
        }

        /* Align vertically within the container */
        float y = 0;
        if (cc->size_flags_v & SIZE_SHRINK_CENTER) {
            y = (self->rect.h - child_h) * 0.5f;
        } else if (cc->size_flags_v & SIZE_SHRINK_END) {
            y = self->rect.h - child_h;
        }

        cc->rect.x = x;
        cc->rect.y = y;
        cc->rect.w = child_w;
        cc->rect.h = child_h;

        x += child_w + hb->separation;
    }
}

/* --- Draw (container itself is invisible; clipping handled by draw_pass) --- */
static void hbox_draw(Node *self, DrawList *dl) {
    (void)self;
    (void)dl;
}

/* ------------------------------------------------------------------ */
/* VTable                                                              */
/* ------------------------------------------------------------------ */
const NodeClass hbox_container_class = {
    .type_name        = "HBoxContainer",
    .instance_size    = sizeof(HBoxContainer),
    .init             = hbox_init,
    .destroy          = hbox_destroy,
    .enter_tree       = NULL,
    .exit_tree        = NULL,
    .ready            = NULL,
    .process          = NULL,
    .draw             = hbox_draw,
    .get_minimum_size = hbox_get_minimum_size,
    .arrange_children = hbox_arrange_children,
    .handle_input     = NULL,
};

/* ------------------------------------------------------------------ */
/* Public API                                                          */
/* ------------------------------------------------------------------ */
HBoxContainer *hbox_container_new(void) {
    return (HBoxContainer*)node_new(&hbox_container_class);
}

void hbox_container_set_separation(HBoxContainer *hb, float sep) {
    hb->separation = sep;
}
