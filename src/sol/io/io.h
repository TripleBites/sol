#ifndef SOL_IO_H
#define SOL_IO_H

#include <stdbool.h>

typedef struct SolPlatform {
    bool (*init)(const char* title, int width, int height);
    void (*shutdown)(void);
    bool (*update)(void);          /* returns false if should close */
    void (*get_size)(int* w, int* h);
} SolPlatform;

const SolPlatform* sol_io_sdl3(void);
const SolPlatform* sol_io_headless(void);

#endif /* SOL_IO_H */
