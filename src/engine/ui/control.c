#include "control.h"
#include <stdlib.h>
#include <math.h>

/* ------------------------------------------------------------------ */
/* Default vtable implementations                                     */
/* ------------------------------------------------------------------ */
static void control_init(Node *self) {
    node_base_init(self, &control_class);
    Control *c = (Control*)self;
    c->anchor       = anchor_full_rect();
    c->offset.left  = 0; c->offset.top   = 0;
    c->offset.right = 0; c->offset.bottom = 0;
    c->min_size.x   = 0; c->min_size.y   = 0;
    c->explicit_min_size.x = 0; c->explicit_min_size.y = 0;
    c->mouse_filter = MOUSE_FILTER_PASS;
    c->focus_mode   = FOCUS_NONE;
    c->theme        = NULL;
}

static void control_destroy(Node *self) {
    node_base_destroy(self);
}

static void control_enter_tree(Node *self) {
    (void)self;
}

static void control_exit_tree(Node *self) {
    (void)self;
}

static void control_ready(Node *self) {
    (void)self;
}

static void control_process(Node *self, float delta) {
    (void)self; (void)delta;
}

static void control_get_minimum_size(Node *self, Vec2 *out) {
    Control *c = (Control*)self;
    *out = control_get_min_size(c);
}

Vec2 control_get_min_size(const Control *c) {
    Vec2 out;
    out.x = c->explicit_min_size.x > c->min_size.x ? c->explicit_min_size.x : c->min_size.x;
    out.y = c->explicit_min_size.y > c->min_size.y ? c->explicit_min_size.y : c->min_size.y;
    return out;
}

static int control_handle_input(Node *self, const struct InputEvent *ev) {
    (void)self; (void)ev;
    return 0;  /* unhandled */
}

/* ------------------------------------------------------------------ */
/* Public vtable                                                       */
/* ------------------------------------------------------------------ */
const NodeClass control_class = {
    .type_name        = "Control",
    .instance_size    = sizeof(Control),
    .init             = control_init,
    .destroy          = control_destroy,
    .enter_tree       = control_enter_tree,
    .exit_tree        = control_exit_tree,
    .ready            = control_ready,
    .process          = control_process,
    .draw             = NULL,  /* draw is per-widget */
    .get_minimum_size = control_get_minimum_size,
    .arrange_children = NULL,
    .handle_input     = control_handle_input,
};

/* ------------------------------------------------------------------ */
/* Public API                                                          */
/* ------------------------------------------------------------------ */
Control *control_new(const NodeClass *klass) {
    return (Control*)node_new(klass ? klass : &control_class);
}

void control_set_anchor(Control *c, float left, float top, float right, float bottom) {
    c->anchor.left   = left;
    c->anchor.top    = top;
    c->anchor.right  = right;
    c->anchor.bottom = bottom;
}

void control_set_offset(Control *c, float left, float top, float right, float bottom) {
    c->offset.left   = left;
    c->offset.top    = top;
    c->offset.right  = right;
    c->offset.bottom = bottom;
}

void control_set_min_size(Control *c, float w, float h) {
    c->explicit_min_size.x = w;
    c->explicit_min_size.y = h;
}

void control_set_size_flags(Control *c, uint32_t h, uint32_t v) {
    c->size_flags_h = h;
    c->size_flags_v = v;
}

/* --- Anchor-based layout --- */
void control_compute_rect_from_anchors(Control *c, Rect parent_rect) {
    Anchor *a = &c->anchor;
    Rect *r = &c->rect;

    r->x = parent_rect.x + parent_rect.w * a->left  + c->offset.left;
    r->y = parent_rect.y + parent_rect.h * a->top   + c->offset.top;
    r->w = parent_rect.w * (a->right - a->left)  + c->offset.right  - c->offset.left;
    r->h = parent_rect.h * (a->bottom - a->top) + c->offset.bottom - c->offset.top;

    /* Clamp to min_size */
    Vec2 ms;
    control_get_minimum_size(&c->base, &ms);
    if (r->w < ms.x) r->w = ms.x;
    if (r->h < ms.y) r->h = ms.y;
}

void control_compute_global_rect(Control *c, const Rect *parent_global) {
    if (parent_global) {
        c->global_rect.x = parent_global->x + c->rect.x;
        c->global_rect.y = parent_global->y + c->rect.y;
        c->global_rect.w = c->rect.w;
        c->global_rect.h = c->rect.h;
    } else {
        c->global_rect = c->rect;
    }
}

/* Default draw - draws nothing (ColorRect overrides this) */
void control_draw(Control *c, DrawList *dl) {
    /* Push clip to the control's global rect */
    draw_list_push_clip(dl, c->global_rect);

    /* Subclasses add their own drawing here via the klass->draw vtable */
    if (c->base.klass->draw) {
        c->base.klass->draw(&c->base, dl);
    }

    draw_list_pop_clip(dl);
}
