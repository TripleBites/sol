#ifndef SOL_UI_CONTROL_H
#define SOL_UI_CONTROL_H

#include "node.h"
#include "draw_list.h"
#include "theme.h"

/* --- Size flags (bitmask) --- */
#define SIZE_FILL           (1u << 0)
#define SIZE_EXPAND         (1u << 1)
#define SIZE_SHRINK_BEGIN   (1u << 2)
#define SIZE_SHRINK_CENTER  (1u << 3)
#define SIZE_SHRINK_END     (1u << 4)

/* --- Mouse filter --- */
#define MOUSE_FILTER_STOP   0
#define MOUSE_FILTER_PASS   1
#define MOUSE_FILTER_IGNORE 2

/* --- Focus mode --- */
#define FOCUS_NONE  0
#define FOCUS_CLICK 1
#define FOCUS_ALL   2

/* --- Anchor preset constants --- */
#define ANCHOR_LEFT   0.0f
#define ANCHOR_TOP    0.0f
#define ANCHOR_RIGHT  1.0f
#define ANCHOR_BOTTOM 1.0f
#define ANCHOR_CENTER 0.5f

/* --- Anchor struct --- */
typedef struct {
    float left, top, right, bottom;
} Anchor;

/* --- Control (extends Node) --- */
typedef struct {
    Node   base;

    /* Layout */
    Rect   rect;         /* local, relative to parent */
    Rect   global_rect;  /* screen-space, computed during arrange */
    Anchor anchor;
    struct {
        float left, top, right, bottom;
    } offset;
    Vec2   min_size;
    Vec2   explicit_min_size;
    uint32_t size_flags_h;
    uint32_t size_flags_v;

    /* Theming */
    Theme *theme;        /* Strong ref to custom theme; NULL = inherit */

    /* Input */
    uint8_t mouse_filter;
    uint8_t focus_mode;
} Control;

/* --- Anchor presets (convenience) --- */
#define anchor_full_rect()  ((Anchor){0, 0, 1, 1})
#define anchor_top_left()   ((Anchor){0, 0, 0, 0})
#define anchor_center()     ((Anchor){0.5f, 0.5f, 0.5f, 0.5f})

/* --- Control API --- */
Control *control_new(const NodeClass *klass);

/* Anchor / offset helpers */
void control_set_anchor(Control *c, float left, float top, float right, float bottom);
void control_set_offset(Control *c, float left, float top, float right, float bottom);
void control_set_min_size(Control *c, float w, float h);
void control_set_size_flags(Control *c, uint32_t h, uint32_t v);

/* Layout computation - exposed for container use */
Vec2 control_get_min_size(const Control *c);
void control_compute_rect_from_anchors(Control *c, Rect parent_rect);
void control_compute_global_rect(Control *c, const Rect *parent_global);

/* Direct rect setter — for programmatic positioning (TUI root setup) */
void control_set_rect(Control *c, float x, float y, float w, float h);

/* Get the Control's NodeClass (for subclass vtables) */
extern const NodeClass control_class;

/* Container flag - set on the Control's base.flags */
#define CONTROL_FLAG_CONTAINER (1u << 8)

#endif /* SOL_UI_CONTROL_H */
