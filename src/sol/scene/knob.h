#ifndef SOL_UI_KNOB_H
#define SOL_UI_KNOB_H

#include "control.h"
#include <stdbool.h>

/* --- Knob (extends Control) ---
   A rotatable knob for adjusting continuous parameters.
   Drag vertically or circularly to change the value. */
typedef struct {
    Control base;

    /* Value */
    float  value;
    float  min_value;
    float  max_value;
    float  step;

    /* Appearance */
    Color  knob_color;
    Color  indicator_color;
    Color  track_color;
    float  radius;

    /* Interaction state */
    bool   is_dragging;
    float  drag_start_value;
    float  drag_start_y;
} Knob;

Knob *knob_new(void);

/* Properties */
void  knob_set_range(Knob *k, float min_val, float max_val, float step);
void  knob_set_value(Knob *k, float value);
float knob_get_value(const Knob *k);
void  knob_set_colors(Knob *k, Color knob, Color indicator, Color track);

extern const NodeClass knob_class;

#endif /* SOL_UI_KNOB_H */
