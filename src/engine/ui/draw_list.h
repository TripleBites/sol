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

/* Iteration (for the renderer backend) */
size_t    draw_list_cmd_count(const DrawList *dl);
DrawCmd   draw_list_get_cmd(const DrawList *dl, size_t i);

#endif /* SOL_UI_DRAW_LIST_H */
