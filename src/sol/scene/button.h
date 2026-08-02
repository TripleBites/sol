#ifndef SOL_UI_BUTTON_H
#define SOL_UI_BUTTON_H

#include "control.h"
#include <stdbool.h>

/* --- Button (extends Control) --- */
typedef struct {
    Control base;

    /* Appearance */
    Color  color_normal;
    Color  color_hover;
    Color  color_pressed;
    float  corner_radius;

    /* State */
    bool   is_hovered;
    bool   is_pressed;      /* mouse button held down inside the button */
    bool   toggle_mode;
    bool   toggled;         /* only meaningful in toggle_mode */
} Button;

Button *button_new(void);

/* Properties */
void button_set_text(Button *btn, const char *text);
void button_set_colors(Button *btn, Color normal, Color hover, Color pressed);
void button_set_toggle_mode(Button *btn, bool enabled);
void button_set_toggled(Button *btn, bool toggled);
bool button_is_toggled(const Button *btn);

extern const NodeClass button_class;

#endif /* SOL_UI_BUTTON_H */
