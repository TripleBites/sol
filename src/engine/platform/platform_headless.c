#include "platform.h"
#include <stdio.h>

static int g_width = 800, g_height = 600;
static bool should_close = false;

static bool headless_init(const char* title, int width, int height) {
    (void)title;
    g_width = width;
    g_height = height;
    printf("[headless] Initialized %dx%d\n", width, height);
    return true;
}

static void headless_shutdown(void) {
    printf("[headless] Shutdown\n");
}

static bool headless_update(void) {
    /* In headless mode, close after one frame */
    if (should_close) return false;
    should_close = true;
    return true;
}

static void headless_get_size(int* w, int* h) {
    *w = g_width;
    *h = g_height;
}

static SolPlatform headless_platform = {
    .init = headless_init,
    .shutdown = headless_shutdown,
    .update = headless_update,
    .get_size = headless_get_size,
};

const SolPlatform* sol_platform_headless(void) {
    return &headless_platform;
}
