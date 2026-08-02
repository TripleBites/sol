#ifndef SOL_UI_LINE_EDIT_H
#define SOL_UI_LINE_EDIT_H

#include "control.h"
#include <stdbool.h>
#include <stddef.h>

/* --- LineEdit (extends Control) ---
   Single-line text input field with cursor, selection, and signals. */
typedef struct {
    Control base;

    /* Text buffer */
    char   *text;
    size_t  text_len;
    size_t  text_capacity;

    /* Cursor and selection */
    size_t  cursor_pos;
    size_t  selection_start;  /* 0 = no selection; otherwise start of range */
    size_t  selection_end;

    /* Settings */
    char   *placeholder;
    size_t  max_length;
    bool    secret;           /* show * instead of characters */
    bool    editable;

    /* Appearance */
    Color   bg_color;
    Color   cursor_color;
    Color   selection_color;
    Color   font_color;
    float   font_size;

    /* State */
    bool    has_focus;
    float   cursor_blink_timer;
} LineEdit;

LineEdit *line_edit_new(void);

/* Properties */
void line_edit_set_text(LineEdit *le, const char *text);
const char *line_edit_get_text(const LineEdit *le);
void line_edit_set_placeholder(LineEdit *le, const char *placeholder);
void line_edit_set_max_length(LineEdit *le, size_t max_len);
void line_edit_set_secret(LineEdit *le, bool secret);
void line_edit_set_editable(LineEdit *le, bool editable);

/* Selection */
void line_edit_select_all(LineEdit *le);
void line_edit_clear_selection(LineEdit *le);

extern const NodeClass line_edit_class;

#endif /* SOL_UI_LINE_EDIT_H */
