#include "vbox_container.h"
#include <stdlib.h>
#include <math.h>

/* ------------------------------------------------------------------ */
/* Init / destroy                                                      */
/* ------------------------------------------------------------------ */
static void vbox_init(Node *self) {
    node_base_init(self, &vbox_container_class);
    VBoxContainer *vb = (VBoxContainer*)self;
    vb->base.anchor       = anchor_full_rect();
    vb->base.mouse_filter = MOUSE_FILTER_PASS;
    vb->separation        = 4.0f;
    /* Mark as container */
    vb->base.base.flags |= CONTROL_FLAG_CONTAINER;
}

static void vbox_destroy(Node *self) {
    node_base_destroy(self);
}

static void vbox_arrange_children(Node *node);

static void vbox_get_minimum_size(Node *node, Vec2 *out) {
    VBoxContainer *vb = (VBoxContainer*)node;
    Control *c = (Control*)node;
    float total_h = 0;
    float max_w = 0;
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
        total_h += ms.y;
        if (ms.x > max_w) max_w = ms.x;
        visible++;
    }

    if (visible > 0) {
        total_h += vb->separation * (visible - 1);
    }
    total_h += c->offset.top + c->offset.bottom;
    max_w   += c->offset.left + c->offset.right;

    if (c->explicit_min_size.x > max_w) max_w = c->explicit_min_size.x;
    if (c->explicit_min_size.y > total_h) total_h = c->explicit_min_size.y;

    c->min_size.x = max_w;
    c->min_size.y = total_h;
    out->x = max_w;
    out->y = total_h;
}

static void vbox_arrange_children(Node *node) {
    if (node->klass != &vbox_container_class) return;

    VBoxContainer *vb = (VBoxContainer*)node;
    Control *self = (Control*)node;

    /* Count visible children and compute expand/fill ratios */
    size_t visible = 0;
    float total_min_h = 0;
    float max_w = 0;
    int expand_count = 0;
    int fill_count = 0;

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
        total_min_h += ms.y;
        if (ms.x > max_w) max_w = ms.x;

        if (cc->size_flags_v & SIZE_EXPAND) expand_count++;
        if (cc->size_flags_v & SIZE_FILL)   fill_count++;
    }

    /* Count visible children again for the actual count */
    visible = 0;
    for (size_t i = 0; i < node->child_count; i++) {
        if (node->children[i]->flags & NODE_FLAG_VISIBLE) visible++;
    }
    if (visible == 0) return;

    float separation_total = vb->separation * (visible - 1);
    float available_h = self->rect.h - separation_total;
    float extra_h = available_h - total_min_h;
    if (extra_h < 0) extra_h = 0;

    float expand_share = (expand_count > 0) ? extra_h / expand_count : 0;

    float y = 0;  /* children positions relative to container origin */
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

        float child_h = ms.y;
        if (cc->size_flags_v & SIZE_EXPAND) {
            child_h += expand_share;
        }
        if (cc->size_flags_v & SIZE_FILL && !(cc->size_flags_v & SIZE_EXPAND)) {
            /* FILL without EXPAND: take remaining space (only last child should use this) */
            child_h = self->rect.h - y;
        }

        float child_w = ms.x;
        if (cc->size_flags_h & SIZE_FILL) {
            child_w = self->rect.w;
        }

        /* Align horizontally within the container */
        float x = 0;
        if (cc->size_flags_h & SIZE_SHRINK_CENTER) {
            x = (self->rect.w - child_w) * 0.5f;
        } else if (cc->size_flags_h & SIZE_SHRINK_END) {
            x = self->rect.w - child_w;
        }

        cc->rect.x = x;
        cc->rect.y = y;
        cc->rect.w = child_w;
        cc->rect.h = child_h;

        y += child_h + vb->separation;
    }
}

/* --- Draw (container draws nothing, but draws children via tree traversal) --- */
static void vbox_draw(Node *self, DrawList *dl) {
    VBoxContainer *vb = (VBoxContainer*)self;
    /* Container itself is invisible; children are drawn by scene tree traversal.
       Just push/pop clip for any container-level styling. */
    draw_list_push_clip(dl, vb->base.global_rect);
    /* If we had a StyleBox background, we'd draw it here */
    draw_list_pop_clip(dl);
}

const NodeClass vbox_container_class = {
    .type_name        = "VBoxContainer",
    .instance_size    = sizeof(VBoxContainer),
    .init             = vbox_init,
    .destroy          = vbox_destroy,
    .enter_tree       = NULL,
    .exit_tree        = NULL,
    .ready            = NULL,
    .process          = NULL,
    .draw             = vbox_draw,
    .get_minimum_size = vbox_get_minimum_size,
    .arrange_children = vbox_arrange_children,
    .handle_input     = NULL,
};

VBoxContainer *vbox_container_new(void) {
    return (VBoxContainer*)node_new(&vbox_container_class);
}

void vbox_container_set_separation(VBoxContainer *vb, float sep) {
    vb->separation = sep;
}
