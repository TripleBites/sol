#define _POSIX_C_SOURCE 200809L
#include "theme.h"
#include <stdlib.h>
#include <string.h>

/* ------------------------------------------------------------------ */
/* Entry types                                                         */
/* ------------------------------------------------------------------ */
typedef enum { THEME_ENTRY_COLOR, THEME_ENTRY_STYLE_BOX } ThemeEntryType;

typedef struct {
    ThemeEntryType type;
    char          *name;        /* e.g. "font_color", "bg" */
    char          *type_class;  /* e.g. "Button", "Label", "" for all */
    Color          color;
    StyleBox       style_box;
} ThemeEntry;

/* ------------------------------------------------------------------ */
/* Theme                                                               */
/* ------------------------------------------------------------------ */
struct Theme {
    ThemeEntry *entries;
    size_t      count;
    size_t      capacity;
    int         refcount;
};

Theme *theme_new(void) {
    Theme *t = calloc(1, sizeof(Theme));
    if (t) t->refcount = 1;
    return t;
}

Theme *theme_ref(Theme *t) {
    if (t) t->refcount++;
    return t;
}

void theme_unref(Theme *t) {
    if (!t) return;
    t->refcount--;
    if (t->refcount <= 0) {
        for (size_t i = 0; i < t->count; i++) {
            free(t->entries[i].name);
            free(t->entries[i].type_class);
        }
        free(t->entries);
        free(t);
    }
}

/* ------------------------------------------------------------------ */
/* Internal helpers                                                    */
/* ------------------------------------------------------------------ */
static ThemeEntry *theme_find_entry(const Theme *t, const char *name,
                                     const char *type_class, ThemeEntryType etype) {
    if (!t || !name || !type_class) return NULL;
    for (size_t i = 0; i < t->count; i++) {
        ThemeEntry *e = &t->entries[i];
        if (e->type == etype &&
            strcmp(e->name, name) == 0 &&
            strcmp(e->type_class, type_class) == 0) {
            return e;
        }
    }
    return NULL;
}

static ThemeEntry *theme_ensure_entry(Theme *t, const char *name,
                                       const char *type_class, ThemeEntryType etype) {
    ThemeEntry *e = theme_find_entry(t, name, type_class, etype);
    if (e) return e;

    /* Grow if needed */
    if (t->count >= t->capacity) {
        size_t new_cap = t->capacity ? t->capacity * 2 : 16;
        ThemeEntry *tmp = realloc(t->entries, sizeof(ThemeEntry) * new_cap);
        if (!tmp) return NULL;
        t->entries = tmp;
        t->capacity = new_cap;
    }

    e = &t->entries[t->count++];
    memset(e, 0, sizeof(*e));
    e->type       = etype;
    e->name       = strdup(name);
    e->type_class = strdup(type_class);
    return e;
}

/* ------------------------------------------------------------------ */
/* Color API                                                           */
/* ------------------------------------------------------------------ */
void theme_set_color(Theme *t, const char *name, const char *type_class, Color c) {
    ThemeEntry *e = theme_ensure_entry(t, name, type_class, THEME_ENTRY_COLOR);
    if (e) e->color = c;
}

Color theme_get_color(const Theme *t, const char *name, const char *type_class,
                      Color fallback) {
    ThemeEntry *e = theme_find_entry(t, name, type_class, THEME_ENTRY_COLOR);
    return e ? e->color : fallback;
}

bool theme_has_color(const Theme *t, const char *name, const char *type_class) {
    return theme_find_entry(t, name, type_class, THEME_ENTRY_COLOR) != NULL;
}

/* ------------------------------------------------------------------ */
/* StyleBox API                                                        */
/* ------------------------------------------------------------------ */
void theme_set_style_box(Theme *t, const char *name, const char *type_class,
                          StyleBox sb) {
    ThemeEntry *e = theme_ensure_entry(t, name, type_class, THEME_ENTRY_STYLE_BOX);
    if (e) e->style_box = sb;
}

StyleBox theme_get_style_box(const Theme *t, const char *name, const char *type_class,
                              StyleBox fallback) {
    ThemeEntry *e = theme_find_entry(t, name, type_class, THEME_ENTRY_STYLE_BOX);
    return e ? e->style_box : fallback;
}

bool theme_has_style_box(const Theme *t, const char *name, const char *type_class) {
    return theme_find_entry(t, name, type_class, THEME_ENTRY_STYLE_BOX) != NULL;
}

/* ------------------------------------------------------------------ */
/* Typed lookup (fall back to "" type_class)                           */
/* ------------------------------------------------------------------ */
Color theme_get_color_typed(const Theme *t, const char *name,
                            const char *type_class, Color fallback) {
    /* Try specific type first */
    ThemeEntry *e = theme_find_entry(t, name, type_class, THEME_ENTRY_COLOR);
    if (e) return e->color;
    /* Fall back to untyped */
    e = theme_find_entry(t, name, "", THEME_ENTRY_COLOR);
    return e ? e->color : fallback;
}

StyleBox theme_get_style_box_typed(const Theme *t, const char *name,
                                    const char *type_class, StyleBox fallback) {
    ThemeEntry *e = theme_find_entry(t, name, type_class, THEME_ENTRY_STYLE_BOX);
    if (e) return e->style_box;
    e = theme_find_entry(t, name, "", THEME_ENTRY_STYLE_BOX);
    return e ? e->style_box : fallback;
}

/* ------------------------------------------------------------------ */
/* Default theme                                                       */
/* ------------------------------------------------------------------ */
Theme *theme_create_default(void) {
    Theme *t = theme_new();
    if (!t) return NULL;

    /* Generic defaults */
    theme_set_color(t, "font_color", "",   color_rgba(0.9f, 0.9f, 0.9f, 1.0f));
    theme_set_color(t, "font_color_hover", "", color_rgba(1.0f, 1.0f, 1.0f, 1.0f));
    theme_set_color(t, "bg_color", "",     color_rgba(0.12f, 0.12f, 0.15f, 1.0f));

    /* Button */
    theme_set_color(t, "bg_color", "Button",
                    color_rgba(0.22f, 0.22f, 0.28f, 1.0f));
    theme_set_color(t, "bg_color_hover", "Button",
                    color_rgba(0.28f, 0.28f, 0.35f, 1.0f));
    theme_set_color(t, "bg_color_pressed", "Button",
                    color_rgba(0.15f, 0.15f, 0.20f, 1.0f));
    theme_set_color(t, "font_color", "Button",
                    color_rgba(0.95f, 0.95f, 0.95f, 1.0f));
    theme_set_style_box(t, "normal", "Button",
                        style_box_rounded(
                            color_rgba(0.25f, 0.25f, 0.30f, 1.0f), 4.0f));

    /* PanelContainer */
    theme_set_style_box(t, "panel", "PanelContainer",
                        style_box_rounded(
                            color_rgba(0.15f, 0.15f, 0.18f, 1.0f), 6.0f));

    /* LineEdit */
    theme_set_color(t, "bg_color", "LineEdit",
                    color_rgba(0.10f, 0.10f, 0.14f, 1.0f));
    theme_set_color(t, "cursor_color", "LineEdit",
                    color_rgba(0.9f, 0.9f, 0.9f, 0.8f));
    theme_set_color(t, "selection_color", "LineEdit",
                    color_rgba(0.30f, 0.50f, 0.80f, 0.4f));
    theme_set_color(t, "font_color", "LineEdit",
                    color_rgba(0.9f, 0.9f, 0.9f, 1.0f));

    return t;
}
