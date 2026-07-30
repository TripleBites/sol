#include "color_rect.h"

static void color_rect_init(Node *self) {
    node_base_init(self, &color_rect_class);
    ColorRect *cr = (ColorRect*)self;
    /* Initialize Control fields */
    cr->base.anchor       = anchor_full_rect();
    cr->base.mouse_filter = MOUSE_FILTER_IGNORE;
    cr->color             = color_rgba(1, 1, 1, 1);
}

static void color_rect_destroy(Node *self) {
    node_base_destroy(self);
}

static void color_rect_draw(Node *self, DrawList *dl) {
    ColorRect *cr = (ColorRect*)self;
    draw_list_add_rect_filled(dl, cr->base.global_rect, cr->color);
}

const NodeClass color_rect_class = {
    .type_name        = "ColorRect",
    .instance_size    = sizeof(ColorRect),
    .init             = color_rect_init,
    .destroy          = color_rect_destroy,
    .enter_tree       = NULL,
    .exit_tree        = NULL,
    .ready            = NULL,
    .process          = NULL,
    .draw             = color_rect_draw,
    .get_minimum_size = NULL,  /* use Control's default via explicit min_size */
    .handle_input     = NULL,
};

ColorRect *color_rect_new(void) {
    return (ColorRect*)node_new(&color_rect_class);
}

void color_rect_set_color(ColorRect *cr, Color c) {
    cr->color = c;
}
