#ifndef SOL_UI_TYPES_H
#define SOL_UI_TYPES_H

/* --- Math types shared across the UI system --- */

typedef struct { float x, y; } Vec2;
typedef struct { float x, y, w, h; } Rect;
typedef struct { float r, g, b, a; } Color;

/* Rect helpers */
#define RECT_LEFT(r)   ((r).x)
#define RECT_RIGHT(r)  ((r).x + (r).w)
#define RECT_TOP(r)    ((r).y)
#define RECT_BOTTOM(r) ((r).y + (r).h)

static inline Rect rect_make(float x, float y, float w, float h) {
    Rect r = { x, y, w, h };
    return r;
}

static inline Vec2 vec2_make(float x, float y) {
    Vec2 v = { x, y };
    return v;
}

static inline Color color_rgba(float r, float g, float b, float a) {
    Color c = { r, g, b, a };
    return c;
}

#define COLOR_RGBA(r,g,b,a) color_rgba((r)/255.0f,(g)/255.0f,(b)/255.0f,(a)/255.0f)

#endif /* SOL_UI_TYPES_H */
