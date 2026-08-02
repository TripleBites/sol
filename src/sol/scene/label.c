#define _POSIX_C_SOURCE 200809L
#include "label.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>

/* Approximate: average character width is ~0.55 × font_size */
#define GLYPH_WIDTH_RATIO  0.55f

/* ------------------------------------------------------------------ */
/* Init / destroy                                                      */
/* ------------------------------------------------------------------ */
static void label_init(Node *self) {
    node_base_init(self, &label_class);
    Label *lbl = (Label*)self;

    lbl->base.anchor       = anchor_full_rect();
    lbl->base.mouse_filter = MOUSE_FILTER_IGNORE;
    lbl->base.focus_mode   = FOCUS_NONE;

    lbl->text       = NULL;
    lbl->font_size  = 16.0f;
    lbl->align      = LABEL_ALIGN_LEFT | LABEL_ALIGN_TOP;
    lbl->autowrap   = false;
    lbl->clip_text  = true;
    lbl->font_color = color_rgba(1, 1, 1, 1);
}

static void label_destroy(Node *self) {
    Label *lbl = (Label*)self;
    free(lbl->text);
    lbl->text = NULL;
    node_base_destroy(self);
}

/* ------------------------------------------------------------------ */
/* Layout                                                              */
/* ------------------------------------------------------------------ */
static void label_get_minimum_size(Node *self, Vec2 *out) {
    Label *lbl = (Label*)self;
    Control *c = (Control*)self;

    /* Approximate text dimensions.
       Full font metrics will come with stb_truetype integration. */
    float text_w = 0, text_h = lbl->font_size;

    if (lbl->text && lbl->text[0]) {
        size_t len = strlen(lbl->text);
        text_w = (float)len * lbl->font_size * GLYPH_WIDTH_RATIO;
    }

    float w = text_w + c->offset.left + c->offset.right;
    float h = text_h + c->offset.top + c->offset.bottom;

    if (c->explicit_min_size.x > w) w = c->explicit_min_size.x;
    if (c->explicit_min_size.y > h) h = c->explicit_min_size.y;

    c->min_size.x = w;
    c->min_size.y = h;
    out->x = w;
    out->y = h;
}

/* ------------------------------------------------------------------ */
/* Drawing                                                             */
/* ------------------------------------------------------------------ */
static void label_draw(Node *self, DrawList *dl) {
    Label *lbl = (Label*)self;

    if (!lbl->text || !lbl->text[0]) return;

    /* Compute the aligned text rect within the label's bounds */
    Rect text_rect = lbl->base.global_rect;

    /* Vertical alignment */
    float text_h = lbl->font_size;
    if (lbl->align & LABEL_ALIGN_MIDDLE) {
        text_rect.y += (text_rect.h - text_h) * 0.5f;
    } else if (lbl->align & LABEL_ALIGN_BOTTOM) {
        text_rect.y += text_rect.h - text_h;
    }

    /* Create a text draw command. The renderer backend handles
       actual glyph rasterization via the font subsystem. */
    draw_list_add_text(dl, text_rect, lbl->text, lbl->font_color, lbl->align);
}

/* ------------------------------------------------------------------ */
/* VTable                                                              */
/* ------------------------------------------------------------------ */
const NodeClass label_class = {
    .type_name        = "Label",
    .instance_size    = sizeof(Label),
    .init             = label_init,
    .destroy          = label_destroy,
    .enter_tree       = NULL,
    .exit_tree        = NULL,
    .ready            = NULL,
    .process          = NULL,
    .draw             = label_draw,
    .get_minimum_size = label_get_minimum_size,
    .arrange_children = NULL,
    .handle_input     = NULL,
};

/* ------------------------------------------------------------------ */
/* Public API                                                          */
/* ------------------------------------------------------------------ */
Label *label_new(void) {
    return (Label*)node_new(&label_class);
}

void label_set_text(Label *lbl, const char *text) {
    if (!lbl) return;
    free(lbl->text);
    lbl->text = text ? strdup(text) : NULL;
}

void label_set_font_size(Label *lbl, float size) {
    if (!lbl) return;
    lbl->font_size = size;
}

void label_set_align(Label *lbl, uint32_t align) {
    if (!lbl) return;
    lbl->align = align;
}

void label_set_font_color(Label *lbl, Color c) {
    if (!lbl) return;
    lbl->font_color = c;
}

void label_set_autowrap(Label *lbl, bool wrap) {
    if (!lbl) return;
    lbl->autowrap = wrap;
}
