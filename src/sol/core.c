#include <stddef.h>
#include "core.h"

#include "io/io.h"

static const SolPlatform* platform = NULL;

bool sol_init(const char* title, int width, int height) {
    platform = sol_io_sdl3();
    if (!platform) return false;
    return platform->init(title, width, height);
}

bool sol_update(void) {
    if (!platform) return false;
    return platform->update();
}

void sol_shutdown(void) {
    if (platform) {
        platform->shutdown();
        platform = NULL;
    }
}

void sol_get_size(int* width, int* height) {
    if (platform) {
        platform->get_size(width, height);
    } else {
        *width = 0;
        *height = 0;
    }
}
