#ifndef SOL_UI_PANEL_CONTAINER_H
#define SOL_UI_PANEL_CONTAINER_H

#include "control.h"
#include "style_box.h"

/* --- PanelContainer (extends Control) ---
   Draws a StyleBox background, then arranges its single child
   inside the StyleBox's inner content area. */
typedef struct {
    Control  base;
    StyleBox style_box;
} PanelContainer;

PanelContainer *panel_container_new(void);
void            panel_container_set_style_box(PanelContainer *pc, StyleBox sb);

extern const NodeClass panel_container_class;

#endif /* SOL_UI_PANEL_CONTAINER_H */
