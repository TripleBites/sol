#include "panel_container.h"
#include <stdlib.h>
#include <math.h>

/* ------------------------------------------------------------------ */
/* Init / destroy                                                      */
/* ------------------------------------------------------------------ */
static void panel_init(Node *self) {
    node_base_init(self, &panel_container_class);
    PanelContainer *pc = (PanelContainer*)self;
    pc->base.anchor       = anchor_full_rect();
    pc->base.mouse_filter = MOUSE_FILTER_PASS;
    pc->style_box         = style_box_flat(color_rgba(0.15f, 0.15f, 0.18f, 1.0f));
    pc->style_box.corner_radius = 6.0f;
    pc->base.base.flags  |= CONTROL_FLAG_CONTAINER;
}

static void panel_destroy(Node *self) {
    node_base_destroy(self);
}

/* ------------------------------------------------------------------ */
/* Layout                                                              */
/* ------------------------------------------------------------------ */
static void panel_get_minimum_size(Node *node, Vec2 *out) {
    PanelContainer *pc = (PanelContainer*)node;
    Control *c = (Control*)node;
    float child_w = 0, child_h = 0;

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
        break;
    }

    /* Account for style box margins and border */
    float extra_w = pc->style_box.margin_left + pc->style_box.margin_right
                  + pc->style_box.border_width * 2;
    float extra_h = pc->style_box.margin_top + pc->style_box.margin_bottom
                  + pc->style_box.border_width * 2;

    float total_w = child_w + extra_w;
    float total_h = child_h + extra_h;

    if (c->explicit_min_size.x > total_w) total_w = c->explicit_min_size.x;
    if (c->explicit_min_size.y > total_h) total_h = c->explicit_min_size.y;

    c->min_size.x = total_w;
    c->min_size.y = total_h;
    out->x = total_w;
    out->y = total_h;
}

static void panel_arrange_children(Node *node) {
    PanelContainer *pc = (PanelContainer*)node;
    Control *self = (Control*)node;

    Rect inner = style_box_get_inner_rect(&pc->style_box, self->rect);

    for (size_t i = 0; i < node->child_count; i++) {
        Node *child = node->children[i];
        if (!(child->flags & NODE_FLAG_VISIBLE)) continue;
        Control *cc = (Control*)child;

        cc->rect.x = inner.x;
        cc->rect.y = inner.y;
        cc->rect.w = inner.w;
        cc->rect.h = inner.h;
        break;
    }
}

/* ------------------------------------------------------------------ */
/* Drawing                                                             */
/* ------------------------------------------------------------------ */
static void panel_draw(Node *self, DrawList *dl) {
    PanelContainer *pc = (PanelContainer*)self;
    style_box_draw(&pc->style_box, dl, pc->base.global_rect);
}

/* ------------------------------------------------------------------ */
/* VTable                                                              */
/* ------------------------------------------------------------------ */
const NodeClass panel_container_class = {
    .type_name        = "PanelContainer",
    .instance_size    = sizeof(PanelContainer),
    .init             = panel_init,
    .destroy          = panel_destroy,
    .enter_tree       = NULL,
    .exit_tree        = NULL,
    .ready            = NULL,
    .process          = NULL,
    .draw             = panel_draw,
    .get_minimum_size = panel_get_minimum_size,
    .arrange_children = panel_arrange_children,
    .handle_input     = NULL,
};

/* ------------------------------------------------------------------ */
/* Public API                                                          */
/* ------------------------------------------------------------------ */
PanelContainer *panel_container_new(void) {
    return (PanelContainer*)node_new(&panel_container_class);
}

void panel_container_set_style_box(PanelContainer *pc, StyleBox sb) {
    pc->style_box = sb;
}
