#include "render2d.h"
#include <stdlib.h>
#include <string.h>
#include <math.h>

#ifndef M_PI
#define M_PI 3.14159265358979323846f
#endif

/* ------------------------------------------------------------------ */
/* Init / free                                                         */
/* ------------------------------------------------------------------ */
Render2D* render2d_new(void) {
    Render2D* r = calloc(1, sizeof(Render2D));
    if (r) render2d_init(r);
    return r;
}

void render2d_init(Render2D* r) {
    memset(r, 0, sizeof(*r));

    r->rect_capacity   = 4096;
    r->line_capacity   = 4096;
    r->circle_capacity = 1024;

    r->rects   = malloc(sizeof(Render2D_Rect)   * r->rect_capacity);
    r->lines   = malloc(sizeof(Render2D_Line)   * r->line_capacity);
    r->circles = malloc(sizeof(Render2D_Circle) * r->circle_capacity);
    r->vertices = malloc(RENDER2D_MAX_VERTICES * RENDER2D_VERTEX_SIZE);
}

void render2d_free(Render2D* r) {
    free(r->rects);
    free(r->lines);
    free(r->circles);
    free(r->vertices);
    memset(r, 0, sizeof(*r));
}

/* ------------------------------------------------------------------ */
/* Begin frame                                                         */
/* ------------------------------------------------------------------ */
void render2d_begin(Render2D* r, float screen_w, float screen_h) {
    r->rect_count   = 0;
    r->line_count   = 0;
    r->circle_count = 0;
    r->vertex_count = 0;
    r->screen_w     = screen_w;
    r->screen_h     = screen_h;
    r->current_z    = 0.0f;
}

/* ------------------------------------------------------------------ */
/* Z control                                                           */
/* ------------------------------------------------------------------ */
void render2d_set_z(Render2D* r, float z) {
    r->current_z = z;
}

/* ------------------------------------------------------------------ */
/* Grow arrays                                                         */
/* ------------------------------------------------------------------ */
static bool grow_rects(Render2D* r) {
    uint32_t new_cap = r->rect_capacity * 2;
    Render2D_Rect* p = realloc(r->rects, sizeof(Render2D_Rect) * new_cap);
    if (!p) return false;
    r->rects = p;
    r->rect_capacity = new_cap;
    return true;
}

static bool grow_lines(Render2D* r) {
    uint32_t new_cap = r->line_capacity * 2;
    Render2D_Line* p = realloc(r->lines, sizeof(Render2D_Line) * new_cap);
    if (!p) return false;
    r->lines = p;
    r->line_capacity = new_cap;
    return true;
}

static bool grow_circles(Render2D* r) {
    uint32_t new_cap = r->circle_capacity * 2;
    Render2D_Circle* p = realloc(r->circles, sizeof(Render2D_Circle) * new_cap);
    if (!p) return false;
    r->circles = p;
    r->circle_capacity = new_cap;
    return true;
}

/* ------------------------------------------------------------------ */
/* Primitives                                                          */
/* ------------------------------------------------------------------ */
void render2d_draw_rect(Render2D* r, float x, float y, float w, float h,
                         Color color, float corner_radius) {
    if (r->rect_count >= r->rect_capacity && !grow_rects(r)) return;
    Render2D_Rect* item = &r->rects[r->rect_count++];
    item->x = x; item->y = y; item->w = w; item->h = h;
    item->r = color.r; item->g = color.g; item->b = color.b; item->a = color.a;
    item->z = r->current_z;
    /* corner_radius stored as w component? No, we don't have that field.
       For v1, corner_radius > 0 uses SDF shader path — but our existing
       UI shader doesn't support SDF. Store in unused field. */
    if (corner_radius > 0.0f) {
        /* Pass radius via h sign bit hack — will be used when we have SDF shader */
        item->h = -fabsf(item->h);  /* negative h signals rounded rect */
    }
}

void render2d_draw_rect_simple(Render2D* r, float x, float y,
                                float w, float h, Color color) {
    render2d_draw_rect(r, x, y, w, h, color, 0.0f);
}

void render2d_draw_rect_border(Render2D* r, float x, float y,
                                float w, float h, Color color,
                                float border_width) {
    /* Top edge */
    render2d_draw_rect_simple(r, x, y, w, border_width, color);
    /* Bottom edge */
    render2d_draw_rect_simple(r, x, y + h - border_width, w, border_width, color);
    /* Left edge */
    render2d_draw_rect_simple(r, x, y, border_width, h, color);
    /* Right edge */
    render2d_draw_rect_simple(r, x + w - border_width, y, border_width, h, color);
}

void render2d_draw_line(Render2D* r, float x1, float y1, float x2, float y2,
                         Color color, float thickness) {
    if (r->line_count >= r->line_capacity && !grow_lines(r)) return;
    Render2D_Line* item = &r->lines[r->line_count++];
    item->x1 = x1; item->y1 = y1; item->x2 = x2; item->y2 = y2;
    item->r = color.r; item->g = color.g; item->b = color.b; item->a = color.a;
    item->width = thickness;
    item->z = r->current_z;
}

void render2d_draw_circle(Render2D* r, float cx, float cy, float radius,
                           Color color, bool filled) {
    if (r->circle_count >= r->circle_capacity && !grow_circles(r)) return;
    Render2D_Circle* item = &r->circles[r->circle_count++];
    item->cx = cx; item->cy = cy; item->radius = radius;
    item->r = color.r; item->g = color.g; item->b = color.b; item->a = color.a;
    item->filled = filled;
    item->z = r->current_z;
}

/* ------------------------------------------------------------------ */
/* Vertex generation helpers                                           */
/* ------------------------------------------------------------------ */

/* Write one vertex (pos + color, 24 bytes) into the buffer */
static void write_vertex(uint8_t* buf, uint32_t* offset,
                          float x, float y,
                          float r, float g, float b, float a) {
    float* v = (float*)(buf + *offset);
    v[0] = x;  v[1] = y;        /* position */
    v[2] = r;  v[3] = g;         /* color */
    v[4] = b;  v[5] = a;
    *offset += 24;
}

/* Write a filled rect: 2 triangles = 6 vertices */
static void emit_rect(uint8_t* buf, uint32_t* offset,
                       float x, float y, float w, float h,
                       float r, float g, float b, float a) {
    float x2 = x + w, y2 = y + h;

    /* Triangle 1: top-left, top-right, bottom-right */
    write_vertex(buf, offset, x,  y,  r, g, b, a);
    write_vertex(buf, offset, x2, y,  r, g, b, a);
    write_vertex(buf, offset, x2, y2, r, g, b, a);
    /* Triangle 2: top-left, bottom-right, bottom-left */
    write_vertex(buf, offset, x,  y,  r, g, b, a);
    write_vertex(buf, offset, x2, y2, r, g, b, a);
    write_vertex(buf, offset, x,  y2, r, g, b, a);
}

/* Write a line quad: 2 triangles = 6 vertices */
static void emit_line(uint8_t* buf, uint32_t* offset,
                       float x1, float y1, float x2, float y2,
                       float thickness,
                       float r, float g, float b, float a) {
    float dx = x2 - x1, dy = y2 - y1;
    float len = sqrtf(dx * dx + dy * dy);
    if (len < 0.001f) return;

    /* Normal direction (perpendicular, normalized) × half thickness */
    float nx = -dy / len * thickness * 0.5f;
    float ny =  dx / len * thickness * 0.5f;

    float p0x = x1 + nx, p0y = y1 + ny;
    float p1x = x1 - nx, p1y = y1 - ny;
    float p2x = x2 - nx, p2y = y2 - ny;
    float p3x = x2 + nx, p3y = y2 + ny;

    /* Triangle 1 */
    write_vertex(buf, offset, p0x, p0y, r, g, b, a);
    write_vertex(buf, offset, p1x, p1y, r, g, b, a);
    write_vertex(buf, offset, p2x, p2y, r, g, b, a);
    /* Triangle 2 */
    write_vertex(buf, offset, p0x, p0y, r, g, b, a);
    write_vertex(buf, offset, p2x, p2y, r, g, b, a);
    write_vertex(buf, offset, p3x, p3y, r, g, b, a);
}

/* Write a filled circle: triangle fan from center */
static void emit_circle_filled(uint8_t* buf, uint32_t* offset,
                                float cx, float cy, float radius,
                                float r, float g, float b, float a) {
    int segments = (int)(radius * 0.8f);  /* more segments for bigger circles */
    if (segments < 8)  segments = 8;
    if (segments > 64) segments = 64;

    float prev_x = cx + radius, prev_y = cy;
    for (int i = 1; i <= segments; i++) {
        float angle = (float)i / (float)segments * 2.0f * M_PI;
        float cur_x = cx + cosf(angle) * radius;
        float cur_y = cy + sinf(angle) * radius;

        write_vertex(buf, offset, cx,   cy,   r, g, b, a);
        write_vertex(buf, offset, prev_x, prev_y, r, g, b, a);
        write_vertex(buf, offset, cur_x, cur_y, r, g, b, a);

        prev_x = cur_x;
        prev_y = cur_y;
    }
}

/* Write an outlined circle: thin quad strip along the circumference */
static void emit_circle_outline(uint8_t* buf, uint32_t* offset,
                                 float cx, float cy, float radius,
                                 float r, float g, float b, float a) {
    int segments = (int)(radius * 0.8f);
    if (segments < 8)  segments = 8;
    if (segments > 64) segments = 64;

    float thickness = 1.5f;
    float inner = radius - thickness * 0.5f;
    float outer = radius + thickness * 0.5f;

    for (int i = 0; i < segments; i++) {
        float a1 = (float)i / (float)segments * 2.0f * M_PI;
        float a2 = (float)(i + 1) / (float)segments * 2.0f * M_PI;

        float i1x = cx + cosf(a1) * inner, i1y = cy + sinf(a1) * inner;
        float o1x = cx + cosf(a1) * outer, o1y = cy + sinf(a1) * outer;
        float i2x = cx + cosf(a2) * inner, i2y = cy + sinf(a2) * inner;
        float o2x = cx + cosf(a2) * outer, o2y = cy + sinf(a2) * outer;

        /* Triangle 1 */
        write_vertex(buf, offset, i1x, i1y, r, g, b, a);
        write_vertex(buf, offset, o1x, o1y, r, g, b, a);
        write_vertex(buf, offset, o2x, o2y, r, g, b, a);
        /* Triangle 2 */
        write_vertex(buf, offset, i1x, i1y, r, g, b, a);
        write_vertex(buf, offset, o2x, o2y, r, g, b, a);
        write_vertex(buf, offset, i2x, i2y, r, g, b, a);
    }
}

/* ------------------------------------------------------------------ */
/* Z-index sorting (stable insertion sort — good for mostly-sorted)    */
/* ------------------------------------------------------------------ */
static int cmp_rect(const void* a, const void* b) {
    float za = ((const Render2D_Rect*)a)->z;
    float zb = ((const Render2D_Rect*)b)->z;
    return (za > zb) ? 1 : (za < zb) ? -1 : 0;
}

static int cmp_line(const void* a, const void* b) {
    float za = ((const Render2D_Line*)a)->z;
    float zb = ((const Render2D_Line*)b)->z;
    return (za > zb) ? 1 : (za < zb) ? -1 : 0;
}

static int cmp_circle(const void* a, const void* b) {
    float za = ((const Render2D_Circle*)a)->z;
    float zb = ((const Render2D_Circle*)b)->z;
    return (za > zb) ? 1 : (za < zb) ? -1 : 0;
}

/* ------------------------------------------------------------------ */
/* Flush — sort by z, generate vertices into interleaved buffer         */
/* ------------------------------------------------------------------ */
void render2d_flush(Render2D* r, const uint8_t** out_vertices,
                     uint32_t* out_vertex_count) {
    /* Sort each item type by z */
    if (r->rect_count > 1) {
        qsort(r->rects, r->rect_count, sizeof(Render2D_Rect), cmp_rect);
    }
    if (r->line_count > 1) {
        qsort(r->lines, r->line_count, sizeof(Render2D_Line), cmp_line);
    }
    if (r->circle_count > 1) {
        qsort(r->circles, r->circle_count, sizeof(Render2D_Circle), cmp_circle);
    }

    /* Emit vertices: rects first (z-sorted), then lines, then circles */
    uint32_t offset = 0;
    uint8_t* buf = r->vertices;

    for (uint32_t i = 0; i < r->rect_count; i++) {
        Render2D_Rect* item = &r->rects[i];
        if (offset + 6 * 24 > RENDER2D_MAX_VERTICES * 24) break;
        emit_rect(buf, &offset,
                  item->x, item->y,
                  item->w > 0 ? item->w : -item->w,
                  item->h > 0 ? item->h : -item->h,
                  item->r, item->g, item->b, item->a);
    }

    for (uint32_t i = 0; i < r->line_count; i++) {
        Render2D_Line* item = &r->lines[i];
        if (offset + 6 * 24 > RENDER2D_MAX_VERTICES * 24) break;
        emit_line(buf, &offset,
                  item->x1, item->y1, item->x2, item->y2,
                  item->width,
                  item->r, item->g, item->b, item->a);
    }

    for (uint32_t i = 0; i < r->circle_count; i++) {
        Render2D_Circle* item = &r->circles[i];
        if (item->filled) {
            emit_circle_filled(buf, &offset,
                               item->cx, item->cy, item->radius,
                               item->r, item->g, item->b, item->a);
        } else {
            emit_circle_outline(buf, &offset,
                                item->cx, item->cy, item->radius,
                                item->r, item->g, item->b, item->a);
        }
    }

    r->vertex_count = offset / 24;
    *out_vertices = buf;
    *out_vertex_count = r->vertex_count;
}
