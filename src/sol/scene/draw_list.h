#ifndef SOL_UI_DRAW_LIST_H
#define SOL_UI_DRAW_LIST_H

#include "types.h"
#include <stddef.h>
#include <stdint.h>

typedef enum {
    DRAW_CMD_RECT_FILLED,
    DRAW_CMD_RECT_BORDER,
    DRAW_CMD_TEXT,
    DRAW_CMD_TEXTURE,
    DRAW_CMD_CLIP_PUSH,
    DRAW_CMD_CLIP_POP,
    DRAW_CMD_LINE_STRIP,     /* polyline: line_points[0..line_point_count-1] */
    DRAW_CMD_CIRCLE_FILLED,  /* filled circle at rect center with radius */
} DrawCmdType;

typedef struct DrawCmd {
    DrawCmdType type;
    Rect        rect;
    Color       color;
    float       corner_radius;
    float       border_width;

    /* For DRAW_CMD_TEXT */
    const char *text;
    size_t      text_len;
    uint32_t    align;

    /* For DRAW_CMD_CLIP_PUSH / POP */
    int         clip_index;

    /* For DRAW_CMD_LINE_STRIP */
    Vec2       *line_points;
    size_t      line_point_count;
} DrawCmd;

typedef struct DrawList {
    DrawCmd *cmds;
    size_t   count;
    size_t   capacity;
    int      clip_stack[16];
    int      clip_depth;
} DrawList;

DrawList *draw_list_create(void);
void      draw_list_clear(DrawList *dl);
void      draw_list_destroy(DrawList *dl);

/* Drawing commands */
void draw_list_add_rect_filled(DrawList *dl, Rect r, Color c);
void draw_list_add_rect_filled_rounded(DrawList *dl, Rect r, Color c, float radius);
void draw_list_add_rect_border(DrawList *dl, Rect r, Color c, float width);
void draw_list_add_text(DrawList *dl, Rect r, const char *text, Color c, uint32_t align);
void draw_list_push_clip(DrawList *dl, Rect r);
void draw_list_pop_clip(DrawList *dl);
void draw_list_add_line_strip(DrawList *dl, const Vec2 *points, size_t count, Color c);
void draw_list_add_circle_filled(DrawList *dl, float cx, float cy, float radius, Color c);

/* Iteration (for the renderer backend) */
size_t    draw_list_cmd_count(const DrawList *dl);
DrawCmd   draw_list_get_cmd(const DrawList *dl, size_t i);

#endif /* SOL_UI_DRAW_LIST_H */
