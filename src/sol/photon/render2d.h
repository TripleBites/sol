#ifndef SOL_PHOTON_RENDER2D_H
#define SOL_PHOTON_RENDER2D_H

#include "../scene/types.h"
#include <stdint.h>
#include <stdbool.h>

/* ------------------------------------------------------------------ */
/* Render2D — immediate-mode 2D batch renderer.                       */
/*                                                                     */
/* Usage each frame:                                                   */
/*   render2d_begin(&r, screen_w, screen_h);                          */
/*   render2d_draw_rect(&r, 10, 10, 100, 100, RED);                  */
/*   render2d_draw_line(&r, 0, 0, 800, 600, WHITE, 2.0);            */
/*   render2d_flush(&r, cmd);   ← records vkCmdDraw into cmd buffer  */
/*                                                                     */
/* Items are sorted by (z_index) then drawn. Same z = submission       */
/* order. All primitives share a single vertex format:                 */
/*   float pos[2], float color[4]  (24 bytes)                         */
/*                                                                     */
/* Maximum per frame: 65536 vertices (~10k rects, or 100k lines)      */
/* ------------------------------------------------------------------ */

#define RENDER2D_MAX_VERTICES 65536
#define RENDER2D_VERTEX_SIZE  24   /* pos(8) + color(16) */

typedef struct {
    float x, y, w, h;
    float r, g, b, a;
    float z;            /* sort key: lower = drawn first */
} Render2D_Rect;

typedef struct {
    float x1, y1, x2, y2;
    float r, g, b, a;
    float width;
    float z;
} Render2D_Line;

typedef struct {
    float cx, cy, radius;
    float r, g, b, a;
    bool  filled;
    float z;
} Render2D_Circle;

/* --- The batch renderer --- */
typedef struct {
    /* Accumulated items */
    Render2D_Rect*   rects;
    uint32_t         rect_count;
    uint32_t         rect_capacity;

    Render2D_Line*   lines;
    uint32_t         line_count;
    uint32_t         line_capacity;

    Render2D_Circle* circles;
    uint32_t         circle_count;
    uint32_t         circle_capacity;

    /* Final sorted vertex data (24 bytes per vertex, interleaved) */
    uint8_t*  vertices;        /* preallocated: MAX_VERTICES * 24 */
    uint32_t  vertex_count;    /* total vertices written this frame */

    /* Screen dimensions for NDC transform */
    float screen_w, screen_h;

    /* Current z-sort key for submission order */
    float current_z;
} Render2D;

/* --- API --- */
Render2D* render2d_new(void);
void render2d_free(Render2D* r);
void render2d_init(Render2D* r);

/* Begin a new frame. Clears accumulated data. */
void render2d_begin(Render2D* r, float screen_w, float screen_h);

/* -- Primitives -- */

/* Solid filled rectangle with optional rounded corners.
   corner_radius = 0 for sharp corners, >0 for rounded. */
void render2d_draw_rect(Render2D* r,
                         float x, float y, float w, float h,
                         Color color, float corner_radius);

/* Solid filled rectangle without rounded corners (convenience). */
void render2d_draw_rect_simple(Render2D* r, float x, float y,
                                float w, float h, Color color);

/* Rectangle border (outline). width is border thickness in pixels. */
void render2d_draw_rect_border(Render2D* r,
                                float x, float y, float w, float h,
                                Color color, float border_width);

/* Line from (x1,y1) to (x2,y2) with given thickness. */
void render2d_draw_line(Render2D* r,
                         float x1, float y1, float x2, float y2,
                         Color color, float thickness);

/* Filled or outlined circle. */
void render2d_draw_circle(Render2D* r,
                           float cx, float cy, float radius,
                           Color color, bool filled);

/* -- Z-index control -- */

/* Set z for subsequent draw calls. Higher z = drawn later (on top).
   Default z is 0. Same z = submission order. */
void render2d_set_z(Render2D* r, float z);

/* -- Flush -- */

/* Sort all accumulated items by z-index, generate vertex data,
   and make it available for the Vulkan backend to upload.
   Returns pointer to packed vertex data and count. */
void render2d_flush(Render2D* r, const uint8_t** out_vertices,
                     uint32_t* out_vertex_count);

#endif /* SOL_PHOTON_RENDER2D_H */
