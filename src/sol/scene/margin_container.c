#include "margin_container.h"
#include <stdlib.h>
#include <math.h>

/* ------------------------------------------------------------------ */
/* Init / destroy                                                      */
/* ------------------------------------------------------------------ */
static void margin_init(Node *self) {
    node_base_init(self, &margin_container_class);
    MarginContainer *mc = (MarginContainer*)self;
    mc->base.anchor       = anchor_full_rect();
    mc->base.mouse_filter = MOUSE_FILTER_PASS;
    mc->margin_left       = 0;
    mc->margin_top        = 0;
    mc->margin_right      = 0;
    mc->margin_bottom     = 0;
    mc->base.base.flags  |= CONTROL_FLAG_CONTAINER;
}

static void margin_destroy(Node *self) {
    node_base_destroy(self);
}

/* ------------------------------------------------------------------ */
/* Layout                                                              */
/* ------------------------------------------------------------------ */
static void margin_get_minimum_size(Node *node, Vec2 *out) {
    MarginContainer *mc = (MarginContainer*)node;
    Control *c = (Control*)node;
    float child_w = 0, child_h = 0;

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
        child_w = ms.x;
        child_h = ms.y;
        break;  /* only first visible child matters */
    }

    float total_w = child_w + mc->margin_left + mc->margin_right;
    float total_h = child_h + mc->margin_top  + mc->margin_bottom;

    if (c->explicit_min_size.x > total_w) total_w = c->explicit_min_size.x;
    if (c->explicit_min_size.y > total_h) total_h = c->explicit_min_size.y;

    c->min_size.x = total_w;
    c->min_size.y = total_h;
    out->x = total_w;
    out->y = total_h;
}

static void margin_arrange_children(Node *node) {
    MarginContainer *mc = (MarginContainer*)node;
    Control *self = (Control*)node;

    /* Find the first visible child and position it inside the margins */
    for (size_t i = 0; i < node->child_count; i++) {
        Node *child = node->children[i];
        if (!(child->flags & NODE_FLAG_VISIBLE)) continue;
        Control *cc = (Control*)child;

        float inner_w = self->rect.w - mc->margin_left - mc->margin_right;
        float inner_h = self->rect.h - mc->margin_top  - mc->margin_bottom;
        if (inner_w < 0) inner_w = 0;
        if (inner_h < 0) inner_h = 0;

        cc->rect.x = mc->margin_left;
        cc->rect.y = mc->margin_top;
        cc->rect.w = inner_w;
        cc->rect.h = inner_h;
        break;  /* only first visible child is arranged */
    }
}

/* --- Draw --- */
static void margin_draw(Node *self, DrawList *dl) {
    (void)self;
    (void)dl;
}

/* ------------------------------------------------------------------ */
/* VTable                                                              */
/* ------------------------------------------------------------------ */
const NodeClass margin_container_class = {
    .type_name        = "MarginContainer",
    .instance_size    = sizeof(MarginContainer),
    .init             = margin_init,
    .destroy          = margin_destroy,
    .enter_tree       = NULL,
    .exit_tree        = NULL,
    .ready            = NULL,
    .process          = NULL,
    .draw             = margin_draw,
    .get_minimum_size = margin_get_minimum_size,
    .arrange_children = margin_arrange_children,
    .handle_input     = NULL,
};

/* ------------------------------------------------------------------ */
/* Public API                                                          */
/* ------------------------------------------------------------------ */
MarginContainer *margin_container_new(void) {
    return (MarginContainer*)node_new(&margin_container_class);
}

void margin_container_set_margin(MarginContainer *mc,
                                  float left, float top,
                                  float right, float bottom) {
    mc->margin_left   = left;
    mc->margin_top    = top;
    mc->margin_right  = right;
    mc->margin_bottom = bottom;
}
