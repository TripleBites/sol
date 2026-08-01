#ifndef SOL_CORE_H
#define SOL_CORE_H

#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

bool sol_init(const char* title, int width, int height);
bool sol_update(void);        /* pump events + render; returns false if should close */
void sol_shutdown(void);
void sol_get_size(int* width, int* height);

#ifdef __cplusplus
}
#endif

#endif /* SOL_CORE_H */
