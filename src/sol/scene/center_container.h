#ifndef SOL_UI_CENTER_CONTAINER_H
#define SOL_UI_CENTER_CONTAINER_H

#include "control.h"

/* --- CenterContainer (extends Control) --- */
typedef struct {
    Control base;
} CenterContainer;

CenterContainer *center_container_new(void);

extern const NodeClass center_container_class;

#endif /* SOL_UI_CENTER_CONTAINER_H */
