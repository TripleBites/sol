#include <stddef.h>
#include <string.h>
#include <stdlib.h>
#include "core.h"
#include "io/io.h"
#include "scene/scene_tree.h"
#include "scene/control.h"
#include "debug/crash.h"
#include "debug/logger.h"

static SolIO*      g_platform = NULL;
static InputState  g_input_state;
static SceneTree*  g_scene_tree = NULL;
static int         g_window_w = 800;
static int         g_window_h = 600;

/* ------------------------------------------------------------------ */
/* Platform selection                                                  */
/* ------------------------------------------------------------------ */
static SolIO* select_platform(void) {
    const char* backend = getenv("SOL_BACKEND");
    if (backend) {
        if (strcmp(backend, "tui") == 0 || strcmp(backend, "terminal") == 0) {
            sol_info("CORE", "Selected TUI backend");
            return (SolIO*)sol_io_tui();
        }
        if (strcmp(backend, "headless") == 0 || strcmp(backend, "alsa") == 0) {
            sol_info("CORE", "Selected headless backend");
            return (SolIO*)sol_io_headless();
        }
    }
    /* Default: SDL3 + Vulkan */
    sol_info("CORE", "Selected SDL3/Vulkan backend");
    return (SolIO*)sol_io_sdl3();
}

/* ------------------------------------------------------------------ */
/* Engine lifecycle                                                    */
/* ------------------------------------------------------------------ */
bool sol_init(const char* title, int width, int height) {
    crash_handler_install();

    g_window_w = width;
    g_window_h = height;

    /* Pick platform */
    g_platform = select_platform();
    if (!g_platform) return false;

    /* Wire shared InputState */
    memset(&g_input_state, 0, sizeof(g_input_state));
    g_platform->input_state = &g_input_state;
    input_state_set_active(&g_input_state);

    /* Create core-owned SceneTree */
    g_scene_tree = scene_tree_create();
    if (!g_scene_tree) {
        sol_error("CORE", "Failed to create SceneTree");
        return false;
    }

    /* Init platform (creates window, audio device, etc.) */
    if (!g_platform->init(title, width, height)) {
        sol_error("CORE", "Platform init failed");
        scene_tree_destroy(g_scene_tree);
        g_scene_tree = NULL;
        return false;
    }

    sol_info("CORE", "Engine initialized: %s %dx%d", title, width, height);
    return true;
}

bool sol_update(void) {
    if (!g_platform) return false;

    /* 1. Begin frame: clear "just" flags */
    input_state_begin_frame(&g_input_state);

    /* 2. Poll OS events into InputState */
    if (g_platform->poll_input) {
        g_platform->poll_input(g_platform);
    }

    /* 3. Get current display size (may have changed from resize) */
    if (g_platform->get_size) {
        g_platform->get_size(&g_window_w, &g_window_h);
    }

    /* 4. Process SceneTree: process → layout → draw */
    if (g_scene_tree && g_scene_tree->root) {
        /* Update root rect for current window size */
        Control* root = (Control*)g_scene_tree->root;
        root->rect.x = 0;
        root->rect.y = 0;
        root->rect.w = (float)g_window_w;
        root->rect.h = (float)g_window_h;

        scene_tree_process(g_scene_tree, 0.016f);
    }

    /* 5. Render via platform */
    if (g_platform->render && g_scene_tree) {
        DrawList* dl = scene_tree_get_draw_list(g_scene_tree);
        if (dl) {
            g_platform->render(dl, g_window_w, g_window_h);
        }
    }

    /* 6. Platform update (window swap, audio pump, etc.) */
    return g_platform->update();
}

void sol_shutdown(void) {
    if (g_scene_tree) {
        scene_tree_destroy(g_scene_tree);
        g_scene_tree = NULL;
    }

    if (g_platform) {
        g_platform->shutdown();
        g_platform = NULL;
    }

    input_state_set_active(NULL);
}

void sol_get_size(int* width, int* height) {
    if (width) *width = g_window_w;
    if (height) *height = g_window_h;
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

void* sol_get_scene_tree(void) {
    return g_scene_tree;
}
