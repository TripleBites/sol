#ifndef SOL_H
#define SOL_H

#ifdef __cplusplus
extern "C" {
#endif

/* Initialize SDL3 subsystems. Returns 0 on success, -1 on error. */
bool sol_init(const char* title, int width, int height);

/* Shutdown SDL3. */
void sol_shutdown(void);

#ifdef __cplusplus
}
#endif

#endif /* SOL_H */