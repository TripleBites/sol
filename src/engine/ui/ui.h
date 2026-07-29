#ifndef ENGINE_H
#define ENGINE_H

#include <stdbool.h>

bool engine_init(const char* title, int width, int height);
void engine_update(void);
void engine_shutdown(void);

#endif // ENGINE_H