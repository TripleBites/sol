#ifndef SOL_DEBUG_TUI_H
#define SOL_DEBUG_TUI_H

#include "../scene/draw_list.h"
#include <stdint.h>
#include <stdbool.h>

/* ------------------------------------------------------------------ */
/* TUI — Terminal UI debug renderer.                                   */
/*                                                                     */
/* Renders a DrawList to a grid of TuiCell, suitable for terminal      */
/* output via ANSI escape codes. Designed for LLM/agent-driven         */
/* development: the rendered grid can be read programmatically         */
/* to verify UI layout without a GPU.                                  */
/*                                                                     */
/* Usage:                                                               */
/*   TuiGrid *grid = tui_render_to_grid(draw_list, cols, rows);        */
/*   // Read grid->cells[y * cols + x] for programmatic inspection     */
/*   char *ansi = tui_render_ansi(draw_list, cols, rows);              */
/*   // Print to terminal for human viewing                            */
/*   tui_grid_free(grid);                                              */
/* ------------------------------------------------------------------ */

/* Maximum terminal dimensions supported */
#define TUI_MAX_COLS 240
#define TUI_MAX_ROWS 120

/* One terminal cell */
typedef struct {
    uint32_t ch;         /* Unicode codepoint (0 = empty/transparent) */
    uint8_t  fg_r, fg_g, fg_b;  /* Foreground color (0-255) */
    uint8_t  bg_r, bg_g, bg_b;  /* Background color (0-255) */
    bool     bold;
    bool     dim;
} TuiCell;

/* 2D grid of cells */
typedef struct {
    TuiCell *cells;
    int      cols;
    int      rows;
} TuiGrid;

/* --- Lifecycle --- */
TuiGrid *tui_grid_create(int cols, int rows);
void     tui_grid_free(TuiGrid *grid);
void     tui_grid_clear(TuiGrid *grid);

/* --- Render DrawList to grid ---
   source_w/source_h: pixel dimensions of the source DrawList (e.g., window size).
   Commands are scaled from source space to grid space. */
void     tui_render_to_grid_scaled(const DrawList *dl, TuiGrid *grid,
                                    float source_w, float source_h);

/* Render without scaling (1:1 pixel-to-cell mapping) */
void     tui_render_to_grid(const DrawList *dl, TuiGrid *grid);

/* --- Render DrawList to ANSI-escaped string ---
   Returns a malloc'd string the caller must free.
   The string contains ANSI escape codes and can be printed directly. */
char    *tui_render_ansi(const DrawList *dl, int cols, int rows);

/* --- Single cell → ANSI utilities (for Python binding) --- */
void     tui_cell_to_ansi(const TuiCell *cell, char *buf, size_t buf_size);

/* --- Keyboard input (non-blocking, raw mode) --- */
typedef struct {
    int  keycode;          /* Unicode codepoint or special key code */
    bool is_special;       /* true for arrows, function keys, etc. */
    bool pressed;          /* true = key down, false = key up */
} TuiKeyEvent;

/* Special key codes (outside Unicode range) */
#define TUI_KEY_UP       0x110000
#define TUI_KEY_DOWN     0x110001
#define TUI_KEY_LEFT     0x110002
#define TUI_KEY_RIGHT    0x110003
#define TUI_KEY_ESCAPE   0x110004
#define TUI_KEY_ENTER    0x110005
#define TUI_KEY_TAB      0x110006
#define TUI_KEY_BACKSPACE 0x110007
#define TUI_KEY_SPACE    0x110008
#define TUI_KEY_F1       0x110100
#define TUI_KEY_F2       0x110101
#define TUI_KEY_F3       0x110102
#define TUI_KEY_F4       0x110103
#define TUI_KEY_F5       0x110104

/* --- TUI input (raw terminal mode) --- */
bool        tui_input_init(void);
void        tui_input_shutdown(void);
TuiKeyEvent tui_input_poll(void);    /* returns keycode=0 if nothing pending */

/* --- Mouse input (SGR extended mode, optional) --- */
typedef struct {
    int  x, y;       /* cell coordinates (0,0 = top-left) */
    int  button;     /* 0=none, 1=left, 2=middle, 3=right, 4=scroll-up, 5=scroll-down */
    bool pressed;
    bool moved;
} TuiMouseEvent;

bool          tui_mouse_init(void);
void          tui_mouse_shutdown(void);
TuiMouseEvent tui_mouse_poll(void);

#endif /* SOL_DEBUG_TUI_H */
