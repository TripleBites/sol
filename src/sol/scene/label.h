#ifndef SOL_UI_LABEL_H
#define SOL_UI_LABEL_H

#include "control.h"
#include <stdbool.h>

/* --- Text alignment --- */
#define LABEL_ALIGN_LEFT    0
#define LABEL_ALIGN_CENTER  1
#define LABEL_ALIGN_RIGHT   2
#define LABEL_ALIGN_TOP     0
#define LABEL_ALIGN_MIDDLE  4
#define LABEL_ALIGN_BOTTOM  8

/* --- Label (extends Control) --- */
typedef struct {
    Control base;

    char    *text;
    float    font_size;
    uint32_t align;        /* OR of horizontal + vertical flags */
    bool     autowrap;
    bool     clip_text;
    Color    font_color;
} Label;

Label *label_new(void);

/* Properties */
void label_set_text(Label *lbl, const char *text);
void label_set_font_size(Label *lbl, float size);
void label_set_align(Label *lbl, uint32_t align);
void label_set_font_color(Label *lbl, Color c);
void label_set_autowrap(Label *lbl, bool wrap);

extern const NodeClass label_class;

#endif /* SOL_UI_LABEL_H */
