#ifndef SOL_UI_COLOR_RECT_H
#define SOL_UI_COLOR_RECT_H

#include "control.h"

/* --- ColorRect (extends Control) --- */
typedef struct {
    Control base;
    Color   color;
} ColorRect;

/* --- API --- */
ColorRect *color_rect_new(void);
void       color_rect_set_color(ColorRect *cr, Color c);

/* Expose vtable for subclassing */
extern const NodeClass color_rect_class;

#endif /* SOL_UI_COLOR_RECT_H */
