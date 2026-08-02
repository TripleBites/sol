#include <stddef.h>
#include <string.h>
#include "core.h"
#include "io/io.h"

static SolIO*      g_platform = NULL;
static InputState  g_input_state;

/* ------------------------------------------------------------------ */
/* Engine lifecycle                                                    */
/* ------------------------------------------------------------------ */
bool sol_init(const char* title, int width, int height) {
    g_platform = (SolIO*)sol_io_sdl3();
    if (!g_platform) return false;

    /* Wire shared InputState */
    memset(&g_input_state, 0, sizeof(g_input_state));
    g_platform->input_state = &g_input_state;
    input_state_set_active(&g_input_state);

    return g_platform->init(title, width, height);
}

bool sol_update(void) {
    if (!g_platform) return false;

    /* 1. Begin frame: clear "just" flags */
    input_state_begin_frame(&g_input_state);

    /* 2. Poll OS events into InputState */
    if (g_platform->poll_input) {
        g_platform->poll_input(g_platform);
    }

    /* 3. Platform update (rendering, audio, etc.) */
    return g_platform->update();
}

void sol_shutdown(void) {
    if (g_platform) {
        g_platform->shutdown();
        g_platform = NULL;
    }
    input_state_set_active(NULL);
}

void sol_get_size(int* width, int* height) {
    if (g_platform) {
        g_platform->get_size(width, height);
    } else {
        *width = 0;
        *height = 0;
    }
}

void sol_io_set_active(SolIO* new_platform) {
    if (g_platform && g_platform->shutdown) {
        g_platform->shutdown();
    }
    g_platform = new_platform;
    if (g_platform) {
        g_platform->input_state = &g_input_state;
    }
}

bool sol_push_event(const SolEvent* ev) {
    return input_state_push_event(&g_input_state, ev);
}
