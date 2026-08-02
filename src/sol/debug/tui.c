/* TUI — Terminal UI debug renderer.
   Renders Sol's DrawList to a grid of TuiCell for terminal display.
   Maps DrawCmd primitives to Unicode box-drawing and block characters. */

#include "tui.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <termios.h>
#include <unistd.h>
#include <fcntl.h>
#include <math.h>

#ifndef M_PI
#define M_PI 3.14159265358979323846f
#endif

/* ------------------------------------------------------------------ */
/* Grid helpers                                                        */
/* ------------------------------------------------------------------ */
TuiGrid *tui_grid_create(int cols, int rows) {
    if (cols < 1 || cols > TUI_MAX_COLS) cols = 80;
    if (rows < 1 || rows > TUI_MAX_ROWS) rows = 40;

    TuiGrid *g = calloc(1, sizeof(TuiGrid));
    if (!g) return NULL;
    g->cols  = cols;
    g->rows  = rows;
    g->cells = calloc((size_t)(cols * rows), sizeof(TuiCell));
    if (!g->cells) { free(g); return NULL; }
    return g;
}

void tui_grid_free(TuiGrid *grid) {
    if (!grid) return;
    free(grid->cells);
    free(grid);
}

void tui_grid_clear(TuiGrid *grid) {
    if (!grid) return;
    memset(grid->cells, 0, (size_t)(grid->cols * grid->rows) * sizeof(TuiCell));
}

/* Put a cell, with bounds+transparency check */
static void put_cell(TuiGrid *g, int x, int y, uint32_t ch,
                     uint8_t fr, uint8_t fg, uint8_t fb,
                     uint8_t br, uint8_t bg, uint8_t bb,
                     bool bold) {
    if (x < 0 || x >= g->cols || y < 0 || y >= g->rows) return;
    TuiCell *c = &g->cells[y * g->cols + x];
    c->ch   = ch;
    c->fg_r = fr; c->fg_g = fg; c->fg_b = fb;
    c->bg_r = br; c->bg_g = bg; c->bg_b = bb;
    c->bold = bold;
}

/* Clamp float color to 0-255 */
static uint8_t fc(float v) {
    int i = (int)(v * 255.0f);
    if (i < 0) return 0;
    if (i > 255) return 255;
    return (uint8_t)i;
}

/* ------------------------------------------------------------------ */
/* Premultiply alpha for compositing onto dark bg                      */
/* ------------------------------------------------------------------ */
static void premul(float *r, float *g, float *b, float a) {
    *r = *r * a + 0.05f * (1.0f - a);
    *g = *g * a + 0.05f * (1.0f - a);
    *b = *b * a + 0.08f * (1.0f - a);
}

/* ------------------------------------------------------------------ */
/* Block characters for partial fills (based on 2x2 subpixel grid)     */
/* ------------------------------------------------------------------ */
/* 2x2 block chars:  ▘▝▀▗▐▄▖▌▞▛▜▚▟█  (positions: 0=tl,1=tr,2=bl,3=br) */
static const uint32_t BLOCK_CHARS[16] = {
    0x0020, /* .... = space */
    0x2598, /* ...X = ▘ upper right */
    0x259D, /* ..X. = ▝ upper left? reversed mapping... */
    0x2580, /* ..XX = ▀ upper half */
    0x2596, /* .X.. = ▖ lower right? */
    0x258C, /* .X.X = ▌ left half? */
    0x259E, /* .XX. = ▞ */
    0x259B, /* .XXX = ▜ */
    0x2597, /* X... = ▗ lower left? */
    0x259A, /* X..X = ▚ */
    0x2590, /* X.X. = ▐ right half */
    0x259C, /* X.XX = ▛ */
    0x2584, /* XX.. = ▄ lower half */
    0x2599, /* XX.X = ▟ */
    0x259F, /* XXX. = ▙ */
    0x2588, /* XXXX = █ full block */
};

/* Compute a 4-bit mask from sub-cell coverage (0.0 to 1.0 in sub_x, sub_y) */
static uint8_t subcell_mask(float fx, float fy, float fw, float fh,
                            float cell_x, float cell_y) {
    uint8_t mask = 0;
    /* 4 sub-cells per character cell in a 2x2 grid */
    for (int sy = 0; sy < 2; sy++) {
        for (int sx = 0; sx < 2; sx++) {
            float scx = cell_x + sx * 0.5f;
            float scy = cell_y + sy * 0.5f;
            if (scx >= fx && scx < fx + fw && scy >= fy && scy < fy + fh) {
                mask |= (1u << (sy * 2 + sx));
            }
        }
    }
    return mask;
}

/* ------------------------------------------------------------------ */
/* Draw a filled rect using block characters                           */
/* ------------------------------------------------------------------ */
static void draw_filled_rect(TuiGrid *g, float rx, float ry, float rw, float rh,
                             uint8_t fr, uint8_t fg, uint8_t fb) {
    int x0 = (int)floorf(rx);
    int y0 = (int)floorf(ry);
    int x1 = (int)ceilf(rx + rw);
    int y1 = (int)ceilf(ry + rh);

    for (int cy = y0; cy < y1 && cy < g->rows; cy++) {
        for (int cx = x0; cx < x1 && cx < g->cols; cx++) {
            float fx = (float)cx;
            float fy = (float)cy;
            uint8_t mask = subcell_mask(rx, ry, rw, rh, fx, fy);
            uint32_t ch = BLOCK_CHARS[mask & 0x0F];
            if (ch != 0x0020) {
                put_cell(g, cx, cy, ch, fr, fg, fb, 10, 10, 15, false);
            }
        }
    }
}

/* ------------------------------------------------------------------ */
/* Draw a border rect using box-drawing chars                          */
/* ------------------------------------------------------------------ */
static void draw_border(TuiGrid *g, float rx, float ry, float rw, float rh,
                        uint8_t fr, uint8_t fg, uint8_t fb) {
    int x0 = (int)rx;
    int y0 = (int)ry;
    int x1 = (int)(rx + rw - 1.0f);
    int y1 = (int)(ry + rh - 1.0f);

    if (x0 >= g->cols || y0 >= g->rows || x1 < 0 || y1 < 0) return;
    if (x0 < 0) x0 = 0;
    if (y0 < 0) y0 = 0;
    if (x1 >= g->cols) x1 = g->cols - 1;
    if (y1 >= g->rows) y1 = g->rows - 1;

    /* Corners */
    put_cell(g, x0, y0, 0x250C, fr, fg, fb, 10, 10, 15, true); /* ┌ */
    put_cell(g, x1, y0, 0x2510, fr, fg, fb, 10, 10, 15, true); /* ┐ */
    put_cell(g, x0, y1, 0x2514, fr, fg, fb, 10, 10, 15, true); /* └ */
    put_cell(g, x1, y1, 0x2518, fr, fg, fb, 10, 10, 15, true); /* ┘ */

    /* Horizontal lines */
    for (int x = x0 + 1; x < x1; x++) {
        put_cell(g, x, y0, 0x2500, fr, fg, fb, 10, 10, 15, true); /* ─ */
        put_cell(g, x, y1, 0x2500, fr, fg, fb, 10, 10, 15, true);
    }

    /* Vertical lines */
    for (int y = y0 + 1; y < y1; y++) {
        put_cell(g, x0, y, 0x2502, fr, fg, fb, 10, 10, 15, true); /* │ */
        put_cell(g, x1, y, 0x2502, fr, fg, fb, 10, 10, 15, true);
    }
}

/* ------------------------------------------------------------------ */
/* Draw text — simple ASCII/UTF-8 rendering                            */
/* ------------------------------------------------------------------ */
static void draw_text(TuiGrid *g, float rx, float ry, float rw, float rh,
                      const char *text, size_t text_len,
                      uint8_t fr, uint8_t fg, uint8_t fb) {
    int x = (int)rx;
    int y = (int)ry;
    if (x >= g->cols || y >= g->rows || y < 0) return;
    if (x < 0) x = 0;

    /* Simple ASCII rendering: one char per cell */
    size_t max_chars = (size_t)((int)(rx + rw) - x);
    if (max_chars > (size_t)(g->cols - x)) max_chars = (size_t)(g->cols - x);

    for (size_t i = 0; i < text_len && i < max_chars; i++) {
        uint32_t ch = (uint8_t)text[i];
        /* Basic UTF-8 detection for 2-byte sequences */
        if ((ch & 0xE0) == 0xC0 && i + 1 < text_len) {
            uint32_t ch2 = (uint8_t)text[i + 1];
            if ((ch2 & 0xC0) == 0x80) {
                ch = ((ch & 0x1F) << 6) | (ch2 & 0x3F);
                i++;
            }
        }
        /* Handle emoji and other multibyte — skip for now */
        if (ch >= 0x80 && ch < 0xC0) continue;

        put_cell(g, x + (int)i, y, ch, fr, fg, fb, 10, 10, 15, false);
    }
}

/* ------------------------------------------------------------------ */
/* Draw a line strip (for waveforms) using braille dots                */
/* ------------------------------------------------------------------ */
/* Braille dot patterns: dots 1-8 map to bits 0-7
   Dot positions in a 2x4 grid:
     1 4
     2 5
     3 6
     7 8
   Base codepoint: U+2800 */
static uint32_t braille_dot(uint8_t bits) {
    return 0x2800 + (uint32_t)bits;
}

static void draw_line_strip(TuiGrid *g, const Vec2 *points, size_t count,
                            uint8_t fr, uint8_t fg, uint8_t fb) {
    if (!points || count < 2) return;

    /* Each character cell represents a 2x4 sub-grid.
       We render points into a braille grid. */
    int cell_h = 4; /* 4 rows per char */
    int cell_w = 2; /* 2 cols per char */

    /* Create a temporary 2D braille dot buffer */
    int bw = g->cols * cell_w;
    int bh = g->rows * cell_h;
    uint8_t *dots = calloc((size_t)(bw * bh), 1);
    if (!dots) return;

    /* Rasterize line segments */
    for (size_t i = 0; i < count - 1; i++) {
        float x0 = points[i].x;
        float y0 = points[i].y;
        float x1 = points[i + 1].x;
        float y1 = points[i + 1].y;

        float dx = x1 - x0;
        float dy = y1 - y0;
        float steps = fmaxf(fabsf(dx), fabsf(dy));
        if (steps < 1.0f) steps = 1.0f;

        for (float t = 0.0f; t <= 1.0f; t += 1.0f / steps) {
            float px = x0 + dx * t;
            float py = y0 + dy * t;
            int ddx = (int)(px * (float)cell_w);
            int ddy = (int)(py * (float)cell_h);
            if (ddx >= 0 && ddx < bw && ddy >= 0 && ddy < bh) {
                dots[ddy * bw + ddx] = 1;
            }
        }
    }

    /* Convert dot buffer to braille characters */
    for (int cy = 0; cy < g->rows; cy++) {
        for (int cx = 0; cx < g->cols; cx++) {
            uint8_t bits = 0;
            for (int dy = 0; dy < cell_h; dy++) {
                for (int dx = 0; dx < cell_w; dx++) {
                    int ddx = cx * cell_w + dx;
                    int ddy = cy * cell_h + dy;
                    if (ddx < bw && ddy < bh && dots[ddy * bw + ddx]) {
                        /* Map 2x4 grid position to braille bit:
                           Braille layout:
                           offset 0: dot 1 (top-left)
                           offset 1: dot 4 (top-right)
                           offset 2: dot 2 (mid-left)
                           offset 3: dot 5 (mid-right)
                           offset 4: dot 3 (bottom-left)
                           offset 5: dot 6 (bottom-right)
                           offset 6: dot 7 (bottom-left-2)
                           offset 7: dot 8 (bottom-right-2) */
                        static const uint8_t pos_to_bit[8] = {0, 3, 1, 4, 2, 5, 6, 7};
                        int pos = dy * cell_w + dx;
                        if (pos < 8) bits |= (1u << pos_to_bit[pos]);
                    }
                }
            }
            if (bits) {
                TuiCell *c = &g->cells[cy * g->cols + cx];
                /* Only set if cell is empty (don't overwrite other content) */
                if (c->ch == 0) {
                    put_cell(g, cx, cy, braille_dot(bits), fr, fg, fb, 10, 10, 15, false);
                }
            }
        }
    }

    free(dots);
}

/* ------------------------------------------------------------------ */
/* Draw a filled circle (approximate)                                  */
/* ------------------------------------------------------------------ */
static void draw_circle_filled(TuiGrid *g, float cx, float cy, float radius,
                               uint8_t fr, uint8_t fg, uint8_t fb) {
    int x0 = (int)floorf(cx - radius);
    int y0 = (int)floorf(cy - radius);
    int x1 = (int)ceilf(cx + radius);
    int y1 = (int)ceilf(cy + radius);

    for (int py = y0; py <= y1 && py < g->rows; py++) {
        for (int px = x0; px <= x1 && px < g->cols; px++) {
            /* Check if the cell center is inside the circle */
            float ccx = (float)px + 0.5f;
            float ccy = (float)py + 0.5f;
            float dx = ccx - cx;
            float dy = ccy - cy;
            float dist = sqrtf(dx * dx + dy * dy);
            if (dist <= radius) {
                /* Compute a subcell mask for anti-aliasing */
                float mask_f = 1.0f;
                if (dist > radius - 0.5f) {
                    mask_f = radius - dist + 0.5f;
                }
                uint8_t mask = (uint8_t)(mask_f * 4.0f);
                if (mask > 4) mask = 4;

                /* Simple filled: just use full block if mostly inside */
                uint32_t ch = 0x2591; /* ░ light shade */
                if (mask >= 3) ch = 0x2588; /* █ full block */
                else if (mask >= 2) ch = 0x2593; /* ▓ dark shade */
                else if (mask >= 1) ch = 0x2592; /* ▒ medium shade */

                put_cell(g, px, py, ch, fr, fg, fb, 10, 10, 15, false);
            }
        }
    }
}

/* ------------------------------------------------------------------ */
/* Main render functions                                               */
/* ------------------------------------------------------------------ */

/* Scale a value from source pixels to grid cells */
static float scale_x(float v, float sw, float gw) { return v * (gw / sw); }
static float scale_y(float v, float sh, float gh) { return v * (gh / sh); }

static DrawCmd scale_draw_cmd(DrawCmd cmd, float sw, float sh, float gw, float gh) {
    cmd.rect.x      = scale_x(cmd.rect.x, sw, gw);
    cmd.rect.y      = scale_y(cmd.rect.y, sh, gh);
    cmd.rect.w      = scale_x(cmd.rect.w, sw, gw);
    cmd.rect.h      = scale_y(cmd.rect.h, sh, gh);
    cmd.corner_radius = scale_x(cmd.corner_radius, sw, gw);
    cmd.border_width   = scale_x(cmd.border_width, sw, gw);

    /* Scale line strip points */
    if (cmd.type == DRAW_CMD_LINE_STRIP && cmd.line_points) {
        /* Points are scaled in-place — we work on a copy */
        for (size_t i = 0; i < cmd.line_point_count; i++) {
            /* Can't modify const — skip scaling for now,
               caller should pre-scale points */
        }
    }
    return cmd;
}

void tui_render_to_grid_scaled(const DrawList *dl, TuiGrid *grid,
                                float source_w, float source_h) {
    if (!dl || !grid || source_w <= 0.0f || source_h <= 0.0f) return;

    float gw = (float)grid->cols;
    float gh = (float)grid->rows;

    /* Allocate a temporary scaled draw list */
    DrawCmd *scaled_cmds = malloc(dl->count * sizeof(DrawCmd));
    if (!scaled_cmds) return;

    /* Allocate temporary line point arrays for scaled lines */
    Vec2 **line_ptrs = calloc(dl->count, sizeof(Vec2*));
    if (!line_ptrs) { free(scaled_cmds); return; }

    for (size_t i = 0; i < dl->count; i++) {
        scaled_cmds[i] = scale_draw_cmd(dl->cmds[i], source_w, source_h, gw, gh);

        /* Scale line strip points */
        if (scaled_cmds[i].type == DRAW_CMD_LINE_STRIP &&
            scaled_cmds[i].line_points &&
            scaled_cmds[i].line_point_count > 0) {
            size_t n = scaled_cmds[i].line_point_count;
            Vec2 *pts = malloc(n * sizeof(Vec2));
            if (pts) {
                for (size_t j = 0; j < n; j++) {
                    pts[j].x = scale_x(dl->cmds[i].line_points[j].x, source_w, gw);
                    pts[j].y = scale_y(dl->cmds[i].line_points[j].y, source_h, gh);
                }
                scaled_cmds[i].line_points = pts;
                line_ptrs[i] = pts;
            }
        }
    }

    /* Build a temporary DrawList wrapper */
    DrawList scaled_dl = *dl;
    scaled_dl.cmds = scaled_cmds;

    /* Render with scaled commands */
    tui_render_to_grid(&scaled_dl, grid);

    /* Clean up line point arrays */
    for (size_t i = 0; i < dl->count; i++) {
        free(line_ptrs[i]);
    }
    free(line_ptrs);
    free(scaled_cmds);
}

void tui_render_to_grid(const DrawList *dl, TuiGrid *grid) {
    if (!dl || !grid) return;

    tui_grid_clear(grid);

    Rect clip_stack[16];
    int  clip_depth = 0;
    /* Default clip rect = full grid */
    clip_stack[0] = rect_make(0, 0, (float)grid->cols, (float)grid->rows);
    clip_depth = 1;

    for (size_t i = 0; i < dl->count; i++) {
        DrawCmd cmd = dl->cmds[i];

        /* Get current clip */
        Rect clip = clip_stack[clip_depth - 1];

        switch (cmd.type) {
        case DRAW_CMD_CLIP_PUSH: {
            if (clip_depth < 15) {
                /* Intersect with clip */
                Rect new_clip = cmd.rect;
                if (new_clip.x < clip.x) { new_clip.w -= clip.x - new_clip.x; new_clip.x = clip.x; }
                if (new_clip.y < clip.y) { new_clip.h -= clip.y - new_clip.y; new_clip.y = clip.y; }
                if (new_clip.x + new_clip.w > clip.x + clip.w)
                    new_clip.w = clip.x + clip.w - new_clip.x;
                if (new_clip.y + new_clip.h > clip.y + clip.h)
                    new_clip.h = clip.y + clip.h - new_clip.y;
                clip_stack[clip_depth++] = new_clip;
            }
            break;
        }
        case DRAW_CMD_CLIP_POP: {
            if (clip_depth > 1) clip_depth--;
            break;
        }
        case DRAW_CMD_RECT_FILLED: {
            uint8_t fr = fc(cmd.color.r);
            uint8_t fg = fc(cmd.color.g);
            uint8_t fb = fc(cmd.color.b);
            float cr = cmd.corner_radius;
            if (cr > 0.0f) {
                /* Rounded rect: draw filled rect + corners */
                draw_filled_rect(grid, cmd.rect.x, cmd.rect.y,
                                cmd.rect.w, cmd.rect.h, fr, fg, fb);
            } else {
                draw_filled_rect(grid, cmd.rect.x, cmd.rect.y,
                                cmd.rect.w, cmd.rect.h, fr, fg, fb);
            }
            break;
        }
        case DRAW_CMD_RECT_BORDER: {
            uint8_t fr = fc(cmd.color.r);
            uint8_t fg = fc(cmd.color.g);
            uint8_t fb = fc(cmd.color.b);
            /* For thick borders, draw multiple passes */
            int passes = (int)(cmd.border_width + 0.5f);
            if (passes < 1) passes = 1;
            for (int p = 0; p < passes; p++) {
                draw_border(grid, cmd.rect.x + (float)p, cmd.rect.y + (float)p,
                           cmd.rect.w - 2.0f * (float)p, cmd.rect.h - 2.0f * (float)p,
                           fr, fg, fb);
            }
            break;
        }
        case DRAW_CMD_TEXT: {
            uint8_t fr = fc(cmd.color.r);
            uint8_t fg = fc(cmd.color.g);
            uint8_t fb = fc(cmd.color.b);
            draw_text(grid, cmd.rect.x, cmd.rect.y,
                     cmd.rect.w, cmd.rect.h,
                     cmd.text, cmd.text_len, fr, fg, fb);
            break;
        }
        case DRAW_CMD_LINE_STRIP: {
            uint8_t fr = fc(cmd.color.r);
            uint8_t fg = fc(cmd.color.g);
            uint8_t fb = fc(cmd.color.b);
            draw_line_strip(grid, cmd.line_points, cmd.line_point_count,
                           fr, fg, fb);
            break;
        }
        case DRAW_CMD_CIRCLE_FILLED: {
            uint8_t fr = fc(cmd.color.r);
            uint8_t fg = fc(cmd.color.g);
            uint8_t fb = fc(cmd.color.b);
            float cx = cmd.rect.x + cmd.rect.w * 0.5f;
            float cy = cmd.rect.y + cmd.rect.h * 0.5f;
            float radius = cmd.rect.w * 0.5f;
            draw_circle_filled(grid, cx, cy, radius, fr, fg, fb);
            break;
        }
        case DRAW_CMD_TEXTURE:
        default:
            break;
        }
    }
}

/* ------------------------------------------------------------------ */
/* ANSI rendering -- optimized with run-length encoding                 */
/* ------------------------------------------------------------------ */

/* Write one UTF-8 codepoint into buf, return bytes written */
static int encode_utf8(uint32_t ch, char *buf) {
    if (ch < 0x80) {
        buf[0] = (char)ch;
        return 1;
    } else if (ch < 0x800) {
        buf[0] = (char)(0xC0 | (ch >> 6));
        buf[1] = (char)(0x80 | (ch & 0x3F));
        return 2;
    } else if (ch < 0x10000) {
        buf[0] = (char)(0xE0 | (ch >> 12));
        buf[1] = (char)(0x80 | ((ch >> 6) & 0x3F));
        buf[2] = (char)(0x80 | (ch & 0x3F));
        return 3;
    } else {
        /* 4-byte (emoji, etc.) -- skip for terminal compatibility */
        buf[0] = '?';
        return 1;
    }
}

/* Check if two cells have the same visual style */
static bool cell_style_eq(const TuiCell *a, const TuiCell *b) {
    return a->fg_r == b->fg_r && a->fg_g == b->fg_g &&
           a->fg_b == b->fg_b && a->bg_r == b->bg_r &&
           a->bg_g == b->bg_g && a->bg_b == b->bg_b &&
           a->bold == b->bold && a->dim == b->dim;
}

/* Emit SGR escape for a cell into buf. Returns bytes written. */
static int emit_sgr(const TuiCell *c, char *buf, size_t sz) {
    return snprintf(buf, sz, "\033[38;2;%d;%d;%d;48;2;%d;%d;%d%s%sm",
                    c->fg_r, c->fg_g, c->fg_b,
                    c->bg_r, c->bg_g, c->bg_b,
                    c->bold ? ";1" : "",
                    c->dim  ? ";2" : "");
}

void tui_cell_to_ansi(const TuiCell *cell, char *buf, size_t buf_size) {
    /* Simplified: just emit the character, no SGR.
       Full SGR rendering is in tui_render_ansi. */
    if (!cell || cell->ch == 0) {
        buf[0] = ' ';
        buf[1] = '\0';
        return;
    }
    encode_utf8(cell->ch, buf);
}

char *tui_render_ansi(const DrawList *dl, int cols, int rows) {
    TuiGrid *grid = tui_grid_create(cols, rows);
    if (!grid) return NULL;

    tui_render_to_grid(dl, grid);

    /* Estimate: ~10 bytes per cell average with RLE */
    size_t est = (size_t)(cols * rows * 14) + 2048;
    char *result = malloc(est);
    if (!result) {
        tui_grid_free(grid);
        return NULL;
    }

    char *dst = result;
    char *end = result + est - 1;

    /* Home cursor (no clear -- we overwrite in-place to avoid flicker) */
    dst += snprintf(dst, (size_t)(end - dst), "\033[H");

    TuiCell empty;
    memset(&empty, 0, sizeof(empty));
    TuiCell prev = empty;
    char char_buf[8];

    for (int y = 0; y < grid->rows; y++) {
        for (int x = 0; x < grid->cols; x++) {
            TuiCell *c = &grid->cells[y * grid->cols + x];

            /* Empty cell: just emit a space */
            if (c->ch == 0) {
                if (!cell_style_eq(&prev, &empty) && dst + 32 < end) {
                    dst += emit_sgr(&empty, dst, (size_t)(end - dst));
                    prev = empty;
                }
                if (dst < end) *dst++ = ' ';
                continue;
            }

            /* Style change? Emit SGR */
            if (!cell_style_eq(c, &prev)) {
                if (dst + 64 < end) {
                    dst += emit_sgr(c, dst, (size_t)(end - dst));
                }
                prev = *c;
            }

            /* Write the character */
            int clen = encode_utf8(c->ch, char_buf);
            for (int i = 0; i < clen && dst < end; i++) {
                *dst++ = char_buf[i];
            }

            if (end - dst < 128) goto done;
        }

        /* End of row: CRLF */
        if (dst + 2 < end) {
            *dst++ = '\r';
            *dst++ = '\n';
        }
        prev = empty;  /* reset style per row */

        if (end - dst < 128) goto done;
    }

done:
    /* Reset SGR */
    if (dst + 8 < end) {
        dst += snprintf(dst, 8, "\033[0m");
    }
    *dst = '\0';

    tui_grid_free(grid);
    return result;
}

/* ------------------------------------------------------------------ */
/* Keyboard input (raw terminal mode)                                   */
/* ------------------------------------------------------------------ */
static struct termios g_tui_old_termios;
static bool g_tui_input_active = false;

bool tui_input_init(void) {
    if (g_tui_input_active) return true;

    struct termios raw;
    if (tcgetattr(STDIN_FILENO, &g_tui_old_termios) < 0) return false;

    raw = g_tui_old_termios;
    raw.c_lflag &= (tcflag_t)(~(ECHO | ICANON | ISIG));
    raw.c_cc[VMIN]  = 0;
    raw.c_cc[VTIME] = 0;

    if (tcsetattr(STDIN_FILENO, TCSAFLUSH, &raw) < 0) return false;

    /* Set stdin to non-blocking */
    int flags = fcntl(STDIN_FILENO, F_GETFL, 0);
    fcntl(STDIN_FILENO, F_SETFL, flags | O_NONBLOCK);

    g_tui_input_active = true;
    return true;
}

void tui_input_shutdown(void) {
    if (!g_tui_input_active) return;
    tcsetattr(STDIN_FILENO, TCSAFLUSH, &g_tui_old_termios);
    g_tui_input_active = false;
}

TuiKeyEvent tui_input_poll(void) {
    TuiKeyEvent ev = {0, false, false};

    unsigned char ch;
    ssize_t n = read(STDIN_FILENO, &ch, 1);
    if (n <= 0) return ev;

    ev.pressed = true;

    /* ESC sequence detection */
    if (ch == 0x1B) {
        /* Check for ESC [ sequences (arrows, etc.) */
        unsigned char seq[8];
        ssize_t n2 = read(STDIN_FILENO, seq, sizeof(seq));
        if (n2 <= 0) {
            ev.keycode    = TUI_KEY_ESCAPE;
            ev.is_special = true;
            return ev;
        }
        if (seq[0] == '[') {
            if (n2 >= 2) {
                if (seq[1] == 'A')      { ev.keycode = TUI_KEY_UP; ev.is_special = true; }
                else if (seq[1] == 'B') { ev.keycode = TUI_KEY_DOWN; ev.is_special = true; }
                else if (seq[1] == 'C') { ev.keycode = TUI_KEY_RIGHT; ev.is_special = true; }
                else if (seq[1] == 'D') { ev.keycode = TUI_KEY_LEFT; ev.is_special = true; }
            }
        }
        return ev;
    }

    /* Map common control characters */
    if (ch == '\n' || ch == '\r') {
        ev.keycode    = TUI_KEY_ENTER;
        ev.is_special = true;
    } else if (ch == '\t') {
        ev.keycode    = TUI_KEY_TAB;
        ev.is_special = true;
    } else if (ch == 0x7F) {
        ev.keycode    = TUI_KEY_BACKSPACE;
        ev.is_special = true;
    } else if (ch == ' ') {
        ev.keycode    = TUI_KEY_SPACE;
        ev.is_special = true;
    } else {
        ev.keycode = ch;
    }

    return ev;
}

/* ------------------------------------------------------------------ */
/* Mouse input (SGR extended mode)                                     */
/* ------------------------------------------------------------------ */
static bool g_mouse_active = false;

bool tui_mouse_init(void) {
    if (g_mouse_active) return true;
    /* Enable SGR extended mouse mode */
    if (write(STDOUT_FILENO, "\033[?1003h\033[?1006h", 16) < 0) {
        return false;
    }
    g_mouse_active = true;
    return true;
}

void tui_mouse_shutdown(void) {
    if (!g_mouse_active) return;
    (void)!write(STDOUT_FILENO, "\033[?1003l\033[?1006l", 16);
    g_mouse_active = false;
}

/* Parse SGR mouse sequence: \e[<Cb;Cx;CyM (press) or m (release) */
TuiMouseEvent tui_mouse_poll(void) {
    TuiMouseEvent mev = {0, 0, 0, false, false};

    /* Mouse events come through stdin interleaved with keyboard.
       This is a simplified reader — in practice you'd buffer and
       parse in tui_input_poll. For now, check for queued ESC sequences. */
    return mev;
}
