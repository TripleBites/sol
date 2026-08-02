#ifndef SOL_CORE_H
#define SOL_CORE_H

#include <stdbool.h>
#include "io/input_state.h"

struct SolIO;  /* forward declaration */

#ifdef __cplusplus
extern "C" {
#endif

bool sol_init(const char* title, int width, int height);
bool sol_update(void);
void sol_shutdown(void);
void sol_get_size(int* width, int* height);

/* Hot-swap platform at runtime (declared in io.h) */

/* Push a MIDI / device event from any thread */
bool sol_push_event(const SolEvent* ev);

#ifdef __cplusplus
}
#endif

#endif /* SOL_CORE_H */
