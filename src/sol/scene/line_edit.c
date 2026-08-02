#define _POSIX_C_SOURCE 200809L
#include "line_edit.h"
#include "input_event.h"
#include "signal.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>

/* Cursor blink period in seconds */
#define CURSOR_BLINK_PERIOD 0.53f

/* ------------------------------------------------------------------ */
/* Internal text buffer helpers                                        */
/* ------------------------------------------------------------------ */
static void ensure_capacity(LineEdit *le, size_t needed) {
    if (needed + 1 <= le->text_capacity) return;
    size_t new_cap = le->text_capacity ? le->text_capacity * 2 : 64;
    if (new_cap < needed + 1) new_cap = needed + 1;
    char *tmp = realloc(le->text, new_cap);
    if (!tmp) return;
    le->text = tmp;
    le->text_capacity = new_cap;
}

static void delete_selection(LineEdit *le) {
    if (le->selection_start == 0) return;
    size_t start = le->selection_start - 1;  /* convert to 0-based index */
    size_t end   = le->selection_end - 1;
    if (start > end) { size_t t = start; start = end; end = t; }
    size_t len = end - start;
    if (len > 0) {
        memmove(le->text + start, le->text + end, le->text_len - end + 1);
        le->text_len -= len;
        le->cursor_pos = start + 1;
    }
    le->selection_start = 0;
    le->selection_end = 0;
}

static void insert_at_cursor(LineEdit *le, const char *str, size_t len) {
    if (len == 0) return;
    if (le->selection_start > 0) delete_selection(le);
    if (le->max_length > 0 && le->text_len + len > le->max_length) {
        len = le->max_length - le->text_len;
        if (len == 0) return;
    }
    ensure_capacity(le, le->text_len + len);
    size_t pos = le->cursor_pos - 1;  /* 0-based insert position */
    memmove(le->text + pos + len, le->text + pos, le->text_len - pos + 1);
    memcpy(le->text + pos, str, len);
    le->text_len += len;
    le->cursor_pos += len;
}

/* ------------------------------------------------------------------ */
/* Init / destroy                                                      */
/* ------------------------------------------------------------------ */
static void line_edit_init(Node *self) {
    node_base_init(self, &line_edit_class);
    LineEdit *le = (LineEdit*)self;

    le->base.anchor       = anchor_full_rect();
    le->base.mouse_filter = MOUSE_FILTER_STOP;
    le->base.focus_mode   = FOCUS_ALL;

    le->text            = NULL;
    le->text_len        = 0;
    le->text_capacity   = 0;
    le->cursor_pos      = 1;  /* 1-based, at start */
    le->selection_start = 0;
    le->selection_end   = 0;
    le->placeholder     = NULL;
    le->max_length      = 0;
    le->secret          = false;
    le->editable        = true;

    le->bg_color        = color_rgba(0.10f, 0.10f, 0.14f, 1.0f);
    le->cursor_color    = color_rgba(0.9f, 0.9f, 0.9f, 0.8f);
    le->selection_color = color_rgba(0.30f, 0.50f, 0.80f, 0.4f);
    le->font_color      = color_rgba(0.9f, 0.9f, 0.9f, 1.0f);
    le->font_size       = 16.0f;
    le->has_focus       = false;
    le->cursor_blink_timer = 0;

    /* Ensure we have an empty string */
    ensure_capacity(le, 0);
    le->text[0] = '\0';

    node_add_signal(self, "text_changed");
    node_add_signal(self, "text_submitted");
}

static void line_edit_destroy(Node *self) {
    LineEdit *le = (LineEdit*)self;
    free(le->text);
    free(le->placeholder);
    node_base_destroy(self);
}

/* ------------------------------------------------------------------ */
/* Layout                                                              */
/* ------------------------------------------------------------------ */
static void line_edit_get_minimum_size(Node *self, Vec2 *out) {
    LineEdit *le = (LineEdit*)self;
    Control *c = (Control*)self;

    /* Single line height = font_size + some padding */
    float h = le->font_size + 8.0f;
    float w = 100.0f;  /* reasonable default width */

    if (c->explicit_min_size.x > w) w = c->explicit_min_size.x;
    if (c->explicit_min_size.y > h) h = c->explicit_min_size.y;

    c->min_size.x = w;
    c->min_size.y = h;
    out->x = w;
    out->y = h;
}

/* ------------------------------------------------------------------ */
/* Process (cursor blink)                                              */
/* ------------------------------------------------------------------ */
static void line_edit_process(Node *self, float delta) {
    LineEdit *le = (LineEdit*)self;
    if (le->has_focus) {
        le->cursor_blink_timer += delta;
    }
}

/* ------------------------------------------------------------------ */
/* Drawing                                                             */
/* ------------------------------------------------------------------ */
static void line_edit_draw(Node *self, DrawList *dl) {
    LineEdit *le = (LineEdit*)self;
    Rect r = le->base.global_rect;

    /* Background */
    draw_list_add_rect_filled_rounded(dl, r, le->bg_color, 3.0f);

    /* Text or placeholder */
    const char *display_text = NULL;
    if (le->text_len > 0 && !le->secret) {
        display_text = le->text;
    } else if (le->text_len > 0 && le->secret) {
        /* Show asterisks — handled by renderer for now, pass text through */
        display_text = le->text;
    } else if (le->placeholder) {
        display_text = le->placeholder;
    }

    if (display_text) {
        Color tc = (le->text_len > 0) ? le->font_color
                   : color_rgba(0.5f, 0.5f, 0.5f, 0.7f);
        Rect text_rect = r;
        text_rect.x += 6.0f;  /* padding */
        text_rect.y += (r.h - le->font_size) * 0.5f;
        text_rect.w -= 12.0f;
        draw_list_add_text(dl, text_rect, display_text, tc, 0);
    }

    /* Cursor */
    if (le->has_focus && le->editable) {
        bool blink_on = ((int)(le->cursor_blink_timer / CURSOR_BLINK_PERIOD)) % 2 == 0;
        if (blink_on) {
            float cursor_x = r.x + 6.0f;
            /* Approximate cursor position from text length */
            if (le->text_len > 0) {
                cursor_x += (float)le->cursor_pos * le->font_size * 0.55f;
            }
            float cursor_y = r.y + 4.0f;
            float cursor_h = r.h - 8.0f;
            Rect cr = rect_make(cursor_x, cursor_y, 1.5f, cursor_h);
            draw_list_add_rect_filled(dl, cr, le->cursor_color);
        }
    }
}

/* ------------------------------------------------------------------ */
/* Input handling                                                      */
/* ------------------------------------------------------------------ */
static void move_cursor_left(LineEdit *le, bool extend_selection) {
    if (!extend_selection) {
        le->selection_start = 0;
        le->selection_end = 0;
    }
    if (le->cursor_pos > 1) {
        if (extend_selection) {
            if (le->selection_start == 0) {
                le->selection_start = le->cursor_pos;
            }
        }
        le->cursor_pos--;
        if (extend_selection) {
            le->selection_end = le->cursor_pos;
        }
    }
}

static void move_cursor_right(LineEdit *le, bool extend_selection) {
    if (!extend_selection) {
        le->selection_start = 0;
        le->selection_end = 0;
    }
    if (le->cursor_pos <= le->text_len) {
        if (extend_selection) {
            if (le->selection_start == 0) {
                le->selection_start = le->cursor_pos;
            }
        }
        le->cursor_pos++;
        if (extend_selection) {
            le->selection_end = le->cursor_pos;
        }
    }
}

static int line_edit_handle_input(Node *self, const UIInputEvent *ev) {
    LineEdit *le = (LineEdit*)self;

    switch (ev->type) {
    case UI_EV_FOCUS_ENTER:
        le->has_focus = true;
        le->cursor_blink_timer = 0;
        return 1;

    case UI_EV_FOCUS_EXIT:
        le->has_focus = false;
        le->selection_start = 0;
        le->selection_end = 0;
        return 1;

    case UI_EV_KEY: {
        if (!le->editable) return 0;

        bool ctrl  = ev->ctrl;
        bool shift = ev->shift;

        switch (ev->keycode) {
        case 42: /* BACKSPACE */
            if (le->selection_start > 0) {
                delete_selection(le);
                node_emit_signal(self, "text_changed", NULL, 0);
            } else if (le->cursor_pos > 1) {
                size_t pos = le->cursor_pos - 1;
                memmove(le->text + pos - 1, le->text + pos,
                        le->text_len - pos + 1);
                le->text_len--;
                le->cursor_pos--;
                node_emit_signal(self, "text_changed", NULL, 0);
            }
            return 1;

        case 76: /* DELETE */
            if (le->selection_start > 0) {
                delete_selection(le);
                node_emit_signal(self, "text_changed", NULL, 0);
            } else if (le->cursor_pos <= le->text_len) {
                size_t pos = le->cursor_pos - 1;
                memmove(le->text + pos, le->text + pos + 1,
                        le->text_len - pos);
                le->text_len--;
                node_emit_signal(self, "text_changed", NULL, 0);
            }
            return 1;

        case 80: /* LEFT */
            move_cursor_left(le, shift);
            return 1;

        case 79: /* RIGHT */
            move_cursor_right(le, shift);
            return 1;

        case 74: /* HOME */
            if (!shift) {
                le->selection_start = 0;
                le->selection_end = 0;
            }
            if (shift && le->selection_start == 0) {
                le->selection_start = le->cursor_pos;
            }
            le->cursor_pos = 1;
            if (shift) le->selection_end = le->cursor_pos;
            return 1;

        case 77: /* END */
            if (!shift) {
                le->selection_start = 0;
                le->selection_end = 0;
            }
            if (shift && le->selection_start == 0) {
                le->selection_start = le->cursor_pos;
            }
            le->cursor_pos = le->text_len + 1;
            if (shift) le->selection_end = le->cursor_pos;
            return 1;

        case 40: /* RETURN / ENTER */
            node_emit_signal(self, "text_submitted", NULL, 0);
            return 1;

        case 44: /* SPACE */
            insert_at_cursor(le, " ", 1);
            node_emit_signal(self, "text_changed", NULL, 0);
            return 1;

        default:
            break;
        }
        break;
    }

    case UI_EV_TEXT:
        if (le->editable && ev->unicode >= 32) {
            char utf8[4];
            int len = 0;
            uint32_t cp = (uint32_t)ev->unicode;

            if (cp < 0x80) {
                utf8[0] = (char)cp; len = 1;
            } else if (cp < 0x800) {
                utf8[0] = (char)(0xC0 | (cp >> 6));
                utf8[1] = (char)(0x80 | (cp & 0x3F));
                len = 2;
            } else if (cp < 0x10000) {
                utf8[0] = (char)(0xE0 | (cp >> 12));
                utf8[1] = (char)(0x80 | ((cp >> 6) & 0x3F));
                utf8[2] = (char)(0x80 | (cp & 0x3F));
                len = 3;
            } else {
                utf8[0] = (char)(0xF0 | (cp >> 18));
                utf8[1] = (char)(0x80 | ((cp >> 12) & 0x3F));
                utf8[2] = (char)(0x80 | ((cp >> 6) & 0x3F));
                utf8[3] = (char)(0x80 | (cp & 0x3F));
                len = 4;
            }

            insert_at_cursor(le, utf8, len);
            node_emit_signal(self, "text_changed", NULL, 0);
            return 1;
        }
        break;

    default:
        break;
    }

    return 0;
}

/* ------------------------------------------------------------------ */
/* VTable                                                              */
/* ------------------------------------------------------------------ */
const NodeClass line_edit_class = {
    .type_name        = "LineEdit",
    .instance_size    = sizeof(LineEdit),
    .init             = line_edit_init,
    .destroy          = line_edit_destroy,
    .enter_tree       = NULL,
    .exit_tree        = NULL,
    .ready            = NULL,
    .process          = line_edit_process,
    .draw             = line_edit_draw,
    .get_minimum_size = line_edit_get_minimum_size,
    .arrange_children = NULL,
    .handle_input     = line_edit_handle_input,
};

/* ------------------------------------------------------------------ */
/* Public API                                                          */
/* ------------------------------------------------------------------ */
LineEdit *line_edit_new(void) {
    return (LineEdit*)node_new(&line_edit_class);
}

void line_edit_set_text(LineEdit *le, const char *text) {
    if (!le || !text) return;
    size_t len = strlen(text);
    ensure_capacity(le, len);
    memcpy(le->text, text, len + 1);
    le->text_len = len;
    le->cursor_pos = len + 1;
    le->selection_start = 0;
    le->selection_end = 0;
}

const char *line_edit_get_text(const LineEdit *le) {
    return le ? le->text : NULL;
}

void line_edit_set_placeholder(LineEdit *le, const char *placeholder) {
    if (!le) return;
    free(le->placeholder);
    le->placeholder = placeholder ? strdup(placeholder) : NULL;
}

void line_edit_set_max_length(LineEdit *le, size_t max_len) {
    if (le) le->max_length = max_len;
}

void line_edit_set_secret(LineEdit *le, bool secret) {
    if (le) le->secret = secret;
}

void line_edit_set_editable(LineEdit *le, bool editable) {
    if (le) le->editable = editable;
}

void line_edit_select_all(LineEdit *le) {
    if (!le || le->text_len == 0) return;
    le->selection_start = 1;
    le->selection_end = le->text_len + 1;
    le->cursor_pos = le->text_len + 1;
}

void line_edit_clear_selection(LineEdit *le) {
    if (!le) return;
    le->selection_start = 0;
    le->selection_end = 0;
}
