#ifndef SOL_UI_HBOX_CONTAINER_H
#define SOL_UI_HBOX_CONTAINER_H

#include "control.h"

/* --- HBoxContainer (extends Control) --- */
typedef struct {
    Control base;
    float   separation;
} HBoxContainer;

HBoxContainer *hbox_container_new(void);
void           hbox_container_set_separation(HBoxContainer *hb, float sep);

extern const NodeClass hbox_container_class;

#endif /* SOL_UI_HBOX_CONTAINER_H */
