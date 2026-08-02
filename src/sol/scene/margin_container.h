#ifndef SOL_UI_MARGIN_CONTAINER_H
#define SOL_UI_MARGIN_CONTAINER_H

#include "control.h"

/* --- MarginContainer (extends Control) --- */
typedef struct {
    Control base;
    float   margin_left;
    float   margin_top;
    float   margin_right;
    float   margin_bottom;
} MarginContainer;

MarginContainer *margin_container_new(void);
void             margin_container_set_margin(MarginContainer *mc,
                                              float left, float top,
                                              float right, float bottom);

extern const NodeClass margin_container_class;

#endif /* SOL_UI_MARGIN_CONTAINER_H */
