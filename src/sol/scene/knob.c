#include "knob.h"
#include "input_event.h"
#include "signal.h"
#include <stdlib.h>
#include <math.h>
#include <string.h>

#ifndef M_PI
#define M_PI 3.14159265358979323846f
#endif

#define KNOB_ANGLE_MIN  (-140.0f * (M_PI / 180.0f))
#define KNOB_ANGLE_MAX  ( 140.0f * (M_PI / 180.0f))
#define KNOB_DRAG_SCALE  0.005f

/* ------------------------------------------------------------------ */
/* Init / destroy                                                      */
/* ------------------------------------------------------------------ */
static void knob_init(Node *self) {
    node_base_init(self, &knob_class);
    Knob *k = (Knob*)self;

    k->base.anchor       = anchor_full_rect();
    k->base.mouse_filter = MOUSE_FILTER_STOP;
    k->base.focus_mode   = FOCUS_NONE;

    k->value       = 0.5f;
    k->min_value   = 0.0f;
    k->max_value   = 1.0f;
    k->step        = 0.0f;   /* 0 = continuous */

    k->knob_color      = color_rgba(0.22f, 0.22f, 0.28f, 1.0f);
    k->indicator_color = color_rgba(0.9f, 0.5f, 0.2f, 1.0f);
    k->track_color     = color_rgba(0.12f, 0.12f, 0.15f, 1.0f);
    k->radius          = 24.0f;

    k->is_dragging      = false;
    k->drag_start_value = 0;
    k->drag_start_y     = 0;

    node_add_signal(self, "value_changed");
}

static void knob_destroy(Node *self) {
    node_base_destroy(self);
}

/* ------------------------------------------------------------------ */
/* Layout                                                              */
/* ------------------------------------------------------------------ */
static void knob_get_minimum_size(Node *self, Vec2 *out) {
    Knob *k = (Knob*)self;
    Control *c = (Control*)self;
    float size = k->radius * 2.0f + 8.0f;

    if (c->explicit_min_size.x > size) size = c->explicit_min_size.x;
    if (c->explicit_min_size.y > size) size = c->explicit_min_size.y;

    c->min_size.x = size;
    c->min_size.y = size;
    out->x = size;
    out->y = size;
}

/* ------------------------------------------------------------------ */
/* Set value with clamping + signal                                    */
/* ------------------------------------------------------------------ */
static void knob_set_value_internal(Knob *k, float val) {
    /* Clamp */
    if (val < k->min_value) val = k->min_value;
    if (val > k->max_value) val = k->max_value;

    /* Snap to step */
    if (k->step > 0.0f) {
        val = roundf((val - k->min_value) / k->step) * k->step + k->min_value;
    }

    if (fabsf(val - k->value) > 0.0001f) {
        k->value = val;
        Variant args[1];
        args[0] = var_float((double)val);
        node_emit_signal(&k->base.base, "value_changed", args, 1);
    }
}

/* ------------------------------------------------------------------ */
/* Drawing                                                             */
/* ------------------------------------------------------------------ */
static void knob_draw(Node *self, DrawList *dl) {
    Knob *k = (Knob*)self;
    Rect r = k->base.global_rect;
    float cx = r.x + r.w * 0.5f;
    float cy = r.y + r.h * 0.5f;
    float radius = k->radius;

    /* Track circle (background) */
    if (k->track_color.a > 0.0f) {
        draw_list_add_circle_filled(dl, cx, cy, radius + 2.0f, k->track_color);
    }

    /* Knob body */
    draw_list_add_circle_filled(dl, cx, cy, radius, k->knob_color);

    /* Indicator line */
    float t = (k->value - k->min_value) / (k->max_value - k->min_value);
    float angle = KNOB_ANGLE_MIN + t * (KNOB_ANGLE_MAX - KNOB_ANGLE_MIN);

    float inner_r = radius * 0.3f;
    float outer_r = radius * 0.85f;

    Vec2 pts[2];
    pts[0].x = cx + cosf(angle) * inner_r;
    pts[0].y = cy + sinf(angle) * inner_r;
    pts[1].x = cx + cosf(angle) * outer_r;
    pts[1].y = cy + sinf(angle) * outer_r;

    draw_list_add_line_strip(dl, pts, 2, k->indicator_color);

    /* Center dot */
    draw_list_add_circle_filled(dl, cx, cy, radius * 0.15f, k->indicator_color);
}

/* ------------------------------------------------------------------ */
/* Input handling                                                      */
/* ------------------------------------------------------------------ */
static int knob_handle_input(Node *self, const UIInputEvent *ev) {
    Knob *k = (Knob*)self;

    switch (ev->type) {
    case UI_EV_MOUSE_BUTTON:
        if (ev->button == 1) {
            if (ev->pressed) {
                k->is_dragging = true;
                k->drag_start_value = k->value;
                k->drag_start_y = ev->pos.y;
                return 1;
            } else {
                k->is_dragging = false;
                return 1;
            }
        }
        break;

    case UI_EV_MOUSE_MOTION:
        if (k->is_dragging) {
            /* Vertical drag: up = increase, down = decrease */
            float dy = k->drag_start_y - ev->pos.y;
            float range = k->max_value - k->min_value;
            float new_val = k->drag_start_value + dy * KNOB_DRAG_SCALE * range;
            knob_set_value_internal(k, new_val);
            return 1;
        }
        break;

    default:
        break;
    }

    return 0;
}

/* ------------------------------------------------------------------ */
/* VTable                                                              */
/* ------------------------------------------------------------------ */
const NodeClass knob_class = {
    .type_name        = "Knob",
    .instance_size    = sizeof(Knob),
    .init             = knob_init,
    .destroy          = knob_destroy,
    .enter_tree       = NULL,
    .exit_tree        = NULL,
    .ready            = NULL,
    .process          = NULL,
    .draw             = knob_draw,
    .get_minimum_size = knob_get_minimum_size,
    .arrange_children = NULL,
    .handle_input     = knob_handle_input,
};

/* ------------------------------------------------------------------ */
/* Public API                                                          */
/* ------------------------------------------------------------------ */
Knob *knob_new(void) {
    return (Knob*)node_new(&knob_class);
}

void knob_set_range(Knob *k, float min_val, float max_val, float step) {
    if (!k) return;
    k->min_value = min_val;
    k->max_value = max_val;
    k->step = step;
    if (k->value < min_val) k->value = min_val;
    if (k->value > max_val) k->value = max_val;
}

void knob_set_value(Knob *k, float value) {
    if (!k) return;
    knob_set_value_internal(k, value);
}

float knob_get_value(const Knob *k) {
    return k ? k->value : 0.0f;
}

void knob_set_colors(Knob *k, Color knob_c, Color indicator, Color track) {
    if (!k) return;
    k->knob_color      = knob_c;
    k->indicator_color = indicator;
    k->track_color     = track;
}
