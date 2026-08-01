#ifndef SOL_UI_VBOX_CONTAINER_H
#define SOL_UI_VBOX_CONTAINER_H

#include "control.h"

/* --- VBoxContainer (extends Control) --- */
typedef struct {
    Control base;
    float   separation;
} VBoxContainer;

VBoxContainer *vbox_container_new(void);
void           vbox_container_set_separation(VBoxContainer *vb, float sep);

extern const NodeClass vbox_container_class;

#endif /* SOL_UI_VBOX_CONTAINER_H */
