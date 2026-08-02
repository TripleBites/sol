#ifndef SOL_UI_INPUT_EVENT_H
#define SOL_UI_INPUT_EVENT_H

#include "types.h"
#include <stdbool.h>

/* --- Event types for the UI system --- */
typedef enum {
    UI_EV_MOUSE_MOTION,
    UI_EV_MOUSE_BUTTON,
    UI_EV_MOUSE_SCROLL,
    UI_EV_KEY,
    UI_EV_TEXT,
    UI_EV_FOCUS_ENTER,
    UI_EV_FOCUS_EXIT,
} UIEventType;

typedef struct {
    UIEventType type;

    /* Mouse / touch */
    Vec2  pos;            /* screen-space coordinates */
    Vec2  delta;
    int   button;         /* 1=left, 2=middle, 3=right */
    bool  pressed;        /* true=pressed, false=released */

    /* Keyboard */
    int   keycode;
    int   unicode;        /* for UI_EV_TEXT */
    bool  alt, shift, ctrl, meta;
} UIInputEvent;

#endif /* SOL_UI_INPUT_EVENT_H */
