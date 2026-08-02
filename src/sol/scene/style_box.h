#ifndef SOL_UI_STYLE_BOX_H
#define SOL_UI_STYLE_BOX_H

#include "types.h"
#include "draw_list.h"
#include <stdbool.h>

/* --- StyleBox — defines how to draw a rectangular background/border --- */
typedef struct {
    Color  bg_color;
    Color  border_color;
    float  border_width;
    float  corner_radius;
    float  margin_left;
    float  margin_top;
    float  margin_right;
    float  margin_bottom;
} StyleBox;

/* --- Constructors --- */
StyleBox style_box_flat(Color bg);
StyleBox style_box_rounded(Color bg, float radius);
StyleBox style_box_bordered(Color bg, Color border, float width);

/* --- Convenience presets --- */
#define style_box_none()  style_box_flat(color_rgba(0,0,0,0))

/* --- Draw into a DrawList --- */
void style_box_draw(const StyleBox *sb, DrawList *dl, Rect rect);

/* --- Get the content area (rect inset by margins) --- */
Rect style_box_get_inner_rect(const StyleBox *sb, Rect outer);

#endif /* SOL_UI_STYLE_BOX_H */
