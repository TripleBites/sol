#include "draw_list.h"
#include <stdlib.h>
#include <string.h>

static void ensure_capacity(DrawList *dl, size_t needed) {
    if (dl->count + needed <= dl->capacity) return;
    size_t new_cap = dl->capacity ? dl->capacity * 2 : 64;
    if (new_cap < dl->count + needed) new_cap = dl->count + needed;
    DrawCmd *tmp = realloc(dl->cmds, sizeof(DrawCmd) * new_cap);
    if (!tmp) return;
    dl->cmds = tmp;
    dl->capacity = new_cap;
}

DrawList *draw_list_create(void) {
    DrawList *dl = calloc(1, sizeof(DrawList));
    return dl;
}

void draw_list_clear(DrawList *dl) {
    if (!dl) return;
    dl->count = 0;
    dl->clip_depth = 0;
}

void draw_list_destroy(DrawList *dl) {
    if (!dl) return;
    free(dl->cmds);
    free(dl);
}

void draw_list_add_rect_filled(DrawList *dl, Rect r, Color c) {
    ensure_capacity(dl, 1);
    DrawCmd *cmd = &dl->cmds[dl->count++];
    memset(cmd, 0, sizeof(*cmd));
    cmd->type = DRAW_CMD_RECT_FILLED;
    cmd->rect = r;
    cmd->color = c;
}

void draw_list_add_rect_filled_rounded(DrawList *dl, Rect r, Color c, float radius) {
    ensure_capacity(dl, 1);
    DrawCmd *cmd = &dl->cmds[dl->count++];
    memset(cmd, 0, sizeof(*cmd));
    cmd->type = DRAW_CMD_RECT_FILLED;
    cmd->rect = r;
    cmd->color = c;
    cmd->corner_radius = radius;
}

void draw_list_add_rect_border(DrawList *dl, Rect r, Color c, float width) {
    ensure_capacity(dl, 1);
    DrawCmd *cmd = &dl->cmds[dl->count++];
    memset(cmd, 0, sizeof(*cmd));
    cmd->type = DRAW_CMD_RECT_BORDER;
    cmd->rect = r;
    cmd->color = c;
    cmd->border_width = width;
}

void draw_list_add_text(DrawList *dl, Rect r, const char *text, Color c, uint32_t align) {
    ensure_capacity(dl, 1);
    DrawCmd *cmd = &dl->cmds[dl->count++];
    memset(cmd, 0, sizeof(*cmd));
    cmd->type = DRAW_CMD_TEXT;
    cmd->rect = r;
    cmd->color = c;
    cmd->text = text;
    cmd->text_len = strlen(text);
    cmd->align = align;
}

void draw_list_push_clip(DrawList *dl, Rect r) {
    ensure_capacity(dl, 1);
    DrawCmd *cmd = &dl->cmds[dl->count++];
    memset(cmd, 0, sizeof(*cmd));
    cmd->type = DRAW_CMD_CLIP_PUSH;
    cmd->rect = r;
    cmd->clip_index = dl->clip_depth;
    if (dl->clip_depth < 16) {
        dl->clip_stack[dl->clip_depth] = dl->clip_depth;
        dl->clip_depth++;
    }
}

void draw_list_pop_clip(DrawList *dl) {
    ensure_capacity(dl, 1);
    DrawCmd *cmd = &dl->cmds[dl->count++];
    memset(cmd, 0, sizeof(*cmd));
    cmd->type = DRAW_CMD_CLIP_POP;
    if (dl->clip_depth > 0) dl->clip_depth--;
}

size_t draw_list_cmd_count(const DrawList *dl) {
    return dl ? dl->count : 0;
}

DrawCmd draw_list_get_cmd(const DrawList *dl, size_t i) {
    DrawCmd empty = {0};
    if (!dl || i >= dl->count) return empty;
    return dl->cmds[i];
}
