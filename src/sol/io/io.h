#ifndef SOL_IO_H
#define SOL_IO_H

#include <stdbool.h>
#include "input_state.h"

/* --- Audio callback ---
 * Called from the platform's audio thread. Must be real-time safe:
 * no malloc, no locks, no I/O. Fills `output` with interleaved
 * float samples in [-1.0, 1.0]. `n_frames` is per-channel frame count.
 */
typedef void (*SolAudioCallback)(float* output, int n_frames, int channels, void* userdata);

/* --- Platform vtable --- */
typedef struct SolIO {
    /* Window / frame loop */
    bool (*init)(const char* title, int width, int height);
    void (*shutdown)(void);
    bool (*update)(void);          /* returns false if should close */
    void (*get_size)(int* width, int* height);

    /* Input — called each frame before anything else.
       Fills the shared InputState from OS events. */
    void (*poll_input)(struct SolIO* self);

    /* Shared InputState — set by engine before first frame */
    InputState* input_state;

    /* Render — called by core after SceneTree processes.
       Receives the DrawList and window dimensions. Platform-specific:
       SDL3 → Vulkan draw, TUI → ANSI terminal output, Headless → no-op */
    void (*render)(void* draw_list, int width, int height);

    /* Audio (may be NULL) */
    bool (*audio_init)(int sample_rate, int channels,
                       SolAudioCallback callback, void* userdata);
    void (*audio_shutdown)(void);
    void (*audio_lock)(void);
    void (*audio_unlock)(void);
} SolIO;

/* --- Backend constructors --- */
const SolIO* sol_io_sdl3(void);
const SolIO* sol_io_headless(void);
const SolIO* sol_io_tui(void);

/* --- Hot-swap --- */
void sol_io_set_active(SolIO* new_platform);

/* --- Input queries (global — reads current platform's InputState) --- */
bool  input_is_key_pressed(int keycode);
bool  input_is_key_just_pressed(int keycode);
bool  input_is_key_just_released(int keycode);
float input_get_mouse_x(void);
float input_get_mouse_y(void);
bool  input_is_mouse_pressed(int button);
bool  input_should_quit(void);
int   input_window_width(void);
int   input_window_height(void);
bool  input_poll_event(SolEvent* out);

#endif /* SOL_IO_H */
