#ifndef SOL_UI_THEME_H
#define SOL_UI_THEME_H

#include "types.h"
#include "style_box.h"
#include <stddef.h>
#include <stdbool.h>

/* --- Theme — reference-counted key-value store for UI styling --- */

typedef struct Theme Theme;

/* Lifecycle */
Theme *theme_new(void);
Theme *theme_ref(Theme *t);
void   theme_unref(Theme *t);

/* Color storage */
void  theme_set_color(Theme *t, const char *name, const char *type_class, Color c);
Color theme_get_color(const Theme *t, const char *name, const char *type_class,
                      Color fallback);
bool  theme_has_color(const Theme *t, const char *name, const char *type_class);

/* StyleBox storage */
void     theme_set_style_box(Theme *t, const char *name, const char *type_class,
                             StyleBox sb);
StyleBox theme_get_style_box(const Theme *t, const char *name, const char *type_class,
                             StyleBox fallback);
bool     theme_has_style_box(const Theme *t, const char *name, const char *type_class);

/* Lookup with type-class fallback: tries "name/type_class" then "name/"" (all types) */
Color    theme_get_color_typed(const Theme *t, const char *name,
                               const char *type_class, Color fallback);
StyleBox theme_get_style_box_typed(const Theme *t, const char *name,
                                   const char *type_class, StyleBox fallback);

/* Default theme — creates a reasonable dark theme */
Theme *theme_create_default(void);

#endif /* SOL_UI_THEME_H */
