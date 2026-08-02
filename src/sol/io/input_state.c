#include "input_state.h"
#include <string.h>

/* Thread-local pointer to current frame's state (avoids passing it everywhere) */
static InputState* g_active_state = NULL;

void input_state_set_active(InputState* s) {
    g_active_state = s;
}

/* ------------------------------------------------------------------ */
/* Frame lifecycle                                                     */
/* ------------------------------------------------------------------ */
void input_state_begin_frame(InputState* s) {
    /* Clear "just" flags from previous frame */
    memset(s->keys_just_pressed,  0, sizeof(s->keys_just_pressed));
    memset(s->keys_just_released, 0, sizeof(s->keys_just_released));
    s->mouse_just_pressed  = 0;
    s->mouse_just_released = 0;
    s->mouse_dx = 0.0f;
    s->mouse_dy = 0.0f;
    s->scroll_dx = 0.0f;
    s->scroll_dy = 0.0f;
    s->event_count = 0;
    s->should_quit = false;

    for (int i = 0; i < 10; i++) {
        s->touches[i].just_pressed  = false;
        s->touches[i].just_released = false;
    }
}

/* ------------------------------------------------------------------ */
/* Platform update helpers                                             */
/* ------------------------------------------------------------------ */
void input_state_key_down(InputState* s, int keycode) {
    if (keycode < 0 || keycode > 255) return;
    int byte = keycode / 8;
    int bit  = keycode % 8;
    if (!(s->keys_pressed[byte] & (1u << bit))) {
        s->keys_just_pressed[byte] |= (1u << bit);
    }
    s->keys_pressed[byte] |= (1u << bit);
}

void input_state_key_up(InputState* s, int keycode) {
    if (keycode < 0 || keycode > 255) return;
    int byte = keycode / 8;
    int bit  = keycode % 8;
    s->keys_just_released[byte] |= (1u << bit);
    s->keys_pressed[byte] &= ~(1u << bit);
}

void input_state_mouse_move(InputState* s, float x, float y) {
    s->mouse_dx += x - s->mouse_x;
    s->mouse_dy += y - s->mouse_y;
    s->mouse_x = x;
    s->mouse_y = y;
}

void input_state_mouse_button(InputState* s, int button, bool pressed) {
    if (button < 1 || button > 3) return;
    uint8_t mask = (uint8_t)(1u << (button - 1));
    if (pressed) {
        if (!(s->mouse_buttons & mask)) {
            s->mouse_just_pressed |= mask;
        }
        s->mouse_buttons |= mask;
    } else {
        s->mouse_just_released |= mask;
        s->mouse_buttons &= ~mask;
    }
}

void input_state_mouse_scroll(InputState* s, float dx, float dy) {
    s->scroll_dx += dx;
    s->scroll_dy += dy;
}

void input_state_touch(InputState* s, int finger, bool active, float x, float y) {
    if (finger < 0 || finger >= 10) return;
    if (active) {
        if (!s->touches[finger].active) {
            s->touches[finger].just_pressed = true;
        }
        s->touches[finger].active = true;
        s->touches[finger].x = x;
        s->touches[finger].y = y;
    } else {
        if (s->touches[finger].active) {
            s->touches[finger].just_released = true;
        }
        s->touches[finger].active = false;
    }
}

bool input_state_push_event(InputState* s, const SolEvent* ev) {
    if (s->event_count >= SOL_MAX_EVENTS) return false;
    s->events[s->event_count++] = *ev;
    return true;
}

/* ------------------------------------------------------------------ */
/* Query API                                                           */
/* ------------------------------------------------------------------ */
static inline bool key_bit(const uint8_t* map, int kc) {
    if (kc < 0 || kc > 255) return false;
    return (map[kc / 8] & (1u << (kc % 8))) != 0;
}

bool input_is_key_pressed(int keycode) {
    return g_active_state ? key_bit(g_active_state->keys_pressed, keycode) : false;
}
bool input_is_key_just_pressed(int keycode) {
    return g_active_state ? key_bit(g_active_state->keys_just_pressed, keycode) : false;
}
bool input_is_key_just_released(int keycode) {
    return g_active_state ? key_bit(g_active_state->keys_just_released, keycode) : false;
}

float input_mouse_x(void) { return g_active_state ? g_active_state->mouse_x : 0.0f; }
float input_mouse_y(void) { return g_active_state ? g_active_state->mouse_y : 0.0f; }
float input_mouse_dx(void) { return g_active_state ? g_active_state->mouse_dx : 0.0f; }
float input_mouse_dy(void) { return g_active_state ? g_active_state->mouse_dy : 0.0f; }

bool input_is_mouse_pressed(int button) {
    if (!g_active_state || button < 1 || button > 3) return false;
    return (g_active_state->mouse_buttons & (1u << (button - 1))) != 0;
}
bool input_is_mouse_just_pressed(int button) {
    if (!g_active_state || button < 1 || button > 3) return false;
    return (g_active_state->mouse_just_pressed & (1u << (button - 1))) != 0;
}
float input_scroll_dx(void) { return g_active_state ? g_active_state->scroll_dx : 0.0f; }
float input_scroll_dy(void) { return g_active_state ? g_active_state->scroll_dy : 0.0f; }

bool input_is_touch_active(int finger) {
    if (!g_active_state || finger < 0 || finger >= 10) return false;
    return g_active_state->touches[finger].active;
}
float input_touch_x(int finger) {
    if (!g_active_state || finger < 0 || finger >= 10) return false;
    return g_active_state->touches[finger].x;
}
float input_touch_y(int finger) {
    if (!g_active_state || finger < 0 || finger >= 10) return false;
    return g_active_state->touches[finger].y;
}
bool input_touch_just_pressed(int finger) {
    if (!g_active_state || finger < 0 || finger >= 10) return false;
    return g_active_state->touches[finger].just_pressed;
}

int input_window_width(void)  { return g_active_state ? g_active_state->window_width : 0; }
int input_window_height(void) { return g_active_state ? g_active_state->window_height : 0; }
bool input_should_quit(void)  { return g_active_state ? g_active_state->should_quit : false; }

bool input_poll_event(SolEvent* out) {
    if (!g_active_state || g_active_state->event_count == 0) return false;
    *out = g_active_state->events[0];
    g_active_state->event_count--;
    if (g_active_state->event_count > 0) {
        memmove(g_active_state->events,
                g_active_state->events + 1,
                (size_t)g_active_state->event_count * sizeof(SolEvent));
    }
    return true;
}
