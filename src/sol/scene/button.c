#include "button.h"
#include "input_event.h"
#include "signal.h"
#include <stdlib.h>
#include <string.h>

/* ------------------------------------------------------------------ */
/* Init / destroy                                                      */
/* ------------------------------------------------------------------ */
static void button_init(Node *self) {
    node_base_init(self, &button_class);
    Button *btn = (Button*)self;

    btn->base.anchor       = anchor_full_rect();
    btn->base.mouse_filter = MOUSE_FILTER_STOP;  /* Button consumes clicks */
    btn->base.focus_mode   = FOCUS_ALL;

    btn->color_normal  = color_rgba(0.25f, 0.25f, 0.30f, 1.0f);
    btn->color_hover   = color_rgba(0.30f, 0.30f, 0.35f, 1.0f);
    btn->color_pressed = color_rgba(0.15f, 0.15f, 0.20f, 1.0f);
    btn->corner_radius = 4.0f;

    btn->is_hovered  = false;
    btn->is_pressed  = false;
    btn->toggle_mode = false;
    btn->toggled     = false;

    /* Register signals */
    node_add_signal(self, "pressed");
    node_add_signal(self, "toggled");
    node_add_signal(self, "button_down");
    node_add_signal(self, "button_up");
}

static void button_destroy(Node *self) {
    node_base_destroy(self);
}

/* ------------------------------------------------------------------ */
/* Drawing                                                             */
/* ------------------------------------------------------------------ */
static void button_draw(Node *self, DrawList *dl) {
    Button *btn = (Button*)self;
    Color c;

    if (btn->is_pressed || btn->toggled) {
        c = btn->color_pressed;
    } else if (btn->is_hovered) {
        c = btn->color_hover;
    } else {
        c = btn->color_normal;
    }

    if (btn->corner_radius > 0) {
        draw_list_add_rect_filled_rounded(dl, btn->base.global_rect, c, btn->corner_radius);
    } else {
        draw_list_add_rect_filled(dl, btn->base.global_rect, c);
    }
}

/* ------------------------------------------------------------------ */
/* Input handling                                                      */
/* ------------------------------------------------------------------ */
static int button_handle_input(Node *self, const UIInputEvent *ev) {
    Button *btn = (Button*)self;
    (void)ev;

    switch (ev->type) {
    case UI_EV_MOUSE_MOTION:
        /* hover state managed by SceneTree via enter/leave.
           Motion here just updates is_hovered based on hit_test. */
        break;

    case UI_EV_FOCUS_ENTER:
        btn->is_hovered = true;
        return 1;

    case UI_EV_FOCUS_EXIT:
        btn->is_hovered = false;
        /* If mouse was pressed on this button and cursor left, cancel */
        if (btn->is_pressed) {
            btn->is_pressed = false;
        }
        return 1;

    case UI_EV_MOUSE_BUTTON:
        if (ev->button == 1) {  /* left mouse button */
            if (ev->pressed) {
                btn->is_pressed = true;
                node_emit_signal(self, "button_down", NULL, 0);
                return 1;  /* consumed */
            } else {
                if (btn->is_pressed) {
                    btn->is_pressed = false;
                    node_emit_signal(self, "button_up", NULL, 0);

                    /* Button click: pressed inside, released inside */
                    if (btn->toggle_mode) {
                        btn->toggled = !btn->toggled;
                        Variant args[1];
                        args[0] = var_bool(btn->toggled);
                        node_emit_signal(self, "toggled", args, 1);
                    }
                    node_emit_signal(self, "pressed", NULL, 0);
                    return 1;
                }
            }
        }
        break;

    default:
        break;
    }

    return 0;  /* not handled */
}

static void button_get_minimum_size(Node *self, Vec2 *out) {
    *out = control_get_min_size((Control*)self);
}

/* ------------------------------------------------------------------ */
/* VTable                                                              */
/* ------------------------------------------------------------------ */
const NodeClass button_class = {
    .type_name        = "Button",
    .instance_size    = sizeof(Button),
    .init             = button_init,
    .destroy          = button_destroy,
    .enter_tree       = NULL,
    .exit_tree        = NULL,
    .ready            = NULL,
    .process          = NULL,
    .draw             = button_draw,
    .get_minimum_size = button_get_minimum_size,
    .arrange_children = NULL,
    .handle_input     = button_handle_input,
};

/* ------------------------------------------------------------------ */
/* Public API                                                          */
/* ------------------------------------------------------------------ */
Button *button_new(void) {
    return (Button*)node_new(&button_class);
}

void button_set_text(Button *btn, const char *text) {
    (void)btn;
    (void)text;
    /* Text rendering requires font/text subsystem (Phase 3).
       For now, buttons are colored rectangles. */
}

void button_set_colors(Button *btn, Color normal, Color hover, Color pressed) {
    btn->color_normal  = normal;
    btn->color_hover   = hover;
    btn->color_pressed = pressed;
}

void button_set_toggle_mode(Button *btn, bool enabled) {
    btn->toggle_mode = enabled;
    if (!enabled) btn->toggled = false;
}

void button_set_toggled(Button *btn, bool toggled) {
    if (btn->toggle_mode) {
        btn->toggled = toggled;
    }
}

bool button_is_toggled(const Button *btn) {
    return btn->toggled;
}
