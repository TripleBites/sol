#ifndef SOL_UI_VARIANT_H
#define SOL_UI_VARIANT_H

#include "types.h"
#include <stdint.h>
#include <stdbool.h>

/* Lightweight discriminated union for signal arguments.
   Signals carry a small number of Variant arguments.
   For NODE variants, refcount is NOT incremented (caller must ensure lifetime). */

typedef enum {
    VAR_NIL,
    VAR_BOOL,
    VAR_INT,
    VAR_FLOAT,
    VAR_STRING,    /* borrowed pointer — no copy, no free */
    VAR_VEC2,
    VAR_RECT,
    VAR_COLOR,
} VariantType;

typedef struct {
    VariantType type;
    union {
        bool      b;
        int64_t   i;
        double    f;
        const char *s;
        Vec2      v2;
        Rect      r;
        Color     c;
    } value;
} Variant;

/* --- Constructors --- */
static inline Variant var_nil(void) {
    Variant v = { VAR_NIL, {0} };
    return v;
}

static inline Variant var_bool(bool b) {
    Variant v = { VAR_BOOL, {.b = b} };
    return v;
}

static inline Variant var_int(int64_t i) {
    Variant v = { VAR_INT, {.i = i} };
    return v;
}

static inline Variant var_float(double f) {
    Variant v = { VAR_FLOAT, {.f = f} };
    return v;
}

static inline Variant var_string(const char *s) {
    Variant v = { VAR_STRING, {.s = s} };
    return v;
}

static inline Variant var_vec2(Vec2 v) {
    Variant result = { VAR_VEC2, {.v2 = v} };
    return result;
}

static inline Variant var_rect(Rect r) {
    Variant result = { VAR_RECT, {.r = r} };
    return result;
}

static inline Variant var_color(Color c) {
    Variant result = { VAR_COLOR, {.c = c} };
    return result;
}

#endif /* SOL_UI_VARIANT_H */
