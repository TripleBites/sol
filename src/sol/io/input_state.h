#ifndef SOL_IO_INPUT_STATE_H
#define SOL_IO_INPUT_STATE_H

#include <stdbool.h>
#include <stdint.h>

/* ------------------------------------------------------------------ */
/* InputState — the single source of truth for current input.          */
/* Refreshed each frame by platform->poll_input().                     */
/*                                                                     */
/* Pattern (Godot-style):                                              */
/*   Platform pumps OS events into InputState. Engine clears "just"    */
/*   flags at start of each frame. Game code queries state directly.   */
/*                                                                     */
/*   if (input_is_key_just_pressed(SOL_KEY_SPACE)) jump();            */
/*   if (input_is_key_pressed(SOL_KEY_W))      move_forward(dt);     */
/* ------------------------------------------------------------------ */

/* --- One-shot event (for MIDI, device hotplug, quit, etc.) --- */
typedef enum {
    SOL_EV_NONE = 0,
    SOL_EV_MIDI_NOTE_ON,
    SOL_EV_MIDI_NOTE_OFF,
    SOL_EV_MIDI_CC,
    SOL_EV_DEVICE_ADDED,
    SOL_EV_DEVICE_REMOVED,
    SOL_EV_WINDOW_RESIZE,
    SOL_EV_QUIT,
    SOL_EV_COUNT
} SolEventType;

typedef struct {
    SolEventType type;
    uint64_t     timestamp_us;

    union {
        struct { int note; float velocity; int channel; } midi_note;
        struct { int controller; int value; int channel; } midi_cc;
        struct { int device_type; int device_index; } device;
        struct { int width, height; } resize;
    };
} SolEvent;

/* --- Keycode constants (match USB HID usage table for familiarity) --- */
#define SOL_KEY_UNKNOWN    0
#define SOL_KEY_A          4
#define SOL_KEY_B          5
#define SOL_KEY_C          6
#define SOL_KEY_D          7
#define SOL_KEY_E          8
#define SOL_KEY_F          9
#define SOL_KEY_G          10
#define SOL_KEY_H          11
#define SOL_KEY_I          12
#define SOL_KEY_J          13
#define SOL_KEY_K          14
#define SOL_KEY_L          15
#define SOL_KEY_M          16
#define SOL_KEY_N          17
#define SOL_KEY_O          18
#define SOL_KEY_P          19
#define SOL_KEY_Q          20
#define SOL_KEY_R          21
#define SOL_KEY_S          22
#define SOL_KEY_T          23
#define SOL_KEY_U          24
#define SOL_KEY_V          25
#define SOL_KEY_W          26
#define SOL_KEY_X          27
#define SOL_KEY_Y          28
#define SOL_KEY_Z          29
#define SOL_KEY_1          30
#define SOL_KEY_2          31
#define SOL_KEY_3          32
#define SOL_KEY_4          33
#define SOL_KEY_5          34
#define SOL_KEY_6          35
#define SOL_KEY_7          36
#define SOL_KEY_8          37
#define SOL_KEY_9          38
#define SOL_KEY_0          39
#define SOL_KEY_RETURN     40
#define SOL_KEY_ESCAPE     41
#define SOL_KEY_BACKSPACE  42
#define SOL_KEY_TAB        43
#define SOL_KEY_SPACE      44
#define SOL_KEY_LEFT       80
#define SOL_KEY_RIGHT      79
#define SOL_KEY_UP         82
#define SOL_KEY_DOWN       81
#define SOL_KEY_LSHIFT     225
#define SOL_KEY_RSHIFT     229
#define SOL_KEY_LCTRL      224
#define SOL_KEY_RCTRL      228
#define SOL_KEY_LALT       226
#define SOL_KEY_RALT       230

#define SOL_MOUSE_LEFT    1
#define SOL_MOUSE_MIDDLE  2
#define SOL_MOUSE_RIGHT   3

/* --- InputState --- */
#define SOL_MAX_EVENTS 32

typedef struct {
    /* Keyboard — 256-key bitmap, no allocation */
    uint8_t keys_pressed[32];        /* currently held */
    uint8_t keys_just_pressed[32];   /* went down this frame */
    uint8_t keys_just_released[32];  /* went up this frame */

    /* Mouse */
    float   mouse_x, mouse_y;        /* current position (pixels) */
    float   mouse_dx, mouse_dy;      /* delta since last frame */
    uint8_t mouse_buttons;           /* bitmask: bit 0=left, 1=middle, 2=right */
    uint8_t mouse_just_pressed;      /* clicked this frame */
    uint8_t mouse_just_released;
    float   scroll_dx, scroll_dy;    /* scroll wheel this frame */

    /* Touch */
    struct {
        bool  active;
        float x, y;
        bool  just_pressed;
        bool  just_released;
    } touches[10];

    /* One-shot events (rare — MIDI notes, device hotplug, quit) */
    SolEvent events[SOL_MAX_EVENTS];
    int      event_count;

    /* Window */
    int     window_width, window_height;
    bool    should_quit;
} InputState;

/* ------------------------------------------------------------------ */
/* Frame lifecycle — called by engine main loop                        */
/* ------------------------------------------------------------------ */

/* Clear "just" flags and one-shot events. Call at START of frame. */
void input_state_begin_frame(InputState* s);

/* Called by platform's poll_input to mark a key going down/up */
void input_state_key_down(InputState* s, int keycode);
void input_state_key_up(InputState* s, int keycode);

/* Called by platform for mouse */
void input_state_mouse_move(InputState* s, float x, float y);
void input_state_mouse_button(InputState* s, int button, bool pressed);
void input_state_mouse_scroll(InputState* s, float dx, float dy);

/* Called by platform for touch */
void input_state_touch(InputState* s, int finger, bool active, float x, float y);

/* Push a one-shot event (MIDI, device, quit) */
bool input_state_push_event(InputState* s, const SolEvent* ev);

/* ------------------------------------------------------------------ */
/* Query API — what game/UI code calls                                 */
/* ------------------------------------------------------------------ */
bool  input_is_key_pressed(int keycode);
bool  input_is_key_just_pressed(int keycode);
bool  input_is_key_just_released(int keycode);

float input_mouse_x(void);
float input_mouse_y(void);
float input_mouse_dx(void);
float input_mouse_dy(void);
bool  input_is_mouse_pressed(int button);        /* 1=left, 2=middle, 3=right */
bool  input_is_mouse_just_pressed(int button);
float input_scroll_dx(void);
float input_scroll_dy(void);

bool  input_is_touch_active(int finger);
float input_touch_x(int finger);
float input_touch_y(int finger);
bool  input_touch_just_pressed(int finger);

int   input_window_width(void);
int   input_window_height(void);
bool  input_should_quit(void);

/* Drain one-shot events */
bool  input_poll_event(SolEvent* out);

/* --- Set the active InputState (called by core.c each frame) --- */
void  input_state_set_active(InputState* s);

#endif /* SOL_IO_INPUT_STATE_H */
