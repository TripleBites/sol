#ifndef SOL_DEBUG_LOGGER_H
#define SOL_DEBUG_LOGGER_H

#include <stdarg.h>
#include <stdbool.h>

/* ------------------------------------------------------------------ */
/* Leveled logging — Godot-style with file:line and tag.               */
/*                                                                     */
/* Thread-safe. Last 256 messages retained in a ring buffer for        */
/* crash postmortem. Python callback hook for routing to logging lib.  */
/* ------------------------------------------------------------------ */

typedef enum {
    SOL_LOG_TRACE = 0,
    SOL_LOG_DEBUG,
    SOL_LOG_INFO,
    SOL_LOG_WARN,
    SOL_LOG_ERROR,
    SOL_LOG_FATAL,
    SOL_LOG_LEVEL_COUNT
} SolLogLevel;

/* --- Core log function --- */
void sol_log(SolLogLevel level, const char* file, int line,
             const char* tag, const char* fmt, ...)
#ifdef __GNUC__
    __attribute__((format(printf, 5, 6)))
#endif
    ;

void sol_log_va(SolLogLevel level, const char* file, int line,
                const char* tag, const char* fmt, va_list ap);

/* --- Convenience macros --- */
#define sol_trace(tag, ...) \
    sol_log(SOL_LOG_TRACE, __FILE__, __LINE__, tag, __VA_ARGS__)
#define sol_debug(tag, ...) \
    sol_log(SOL_LOG_DEBUG, __FILE__, __LINE__, tag, __VA_ARGS__)
#define sol_info(tag, ...) \
    sol_log(SOL_LOG_INFO,  __FILE__, __LINE__, tag, __VA_ARGS__)
#define sol_warn(tag, ...) \
    sol_log(SOL_LOG_WARN,  __FILE__, __LINE__, tag, __VA_ARGS__)
#define sol_error(tag, ...) \
    sol_log(SOL_LOG_ERROR, __FILE__, __LINE__, tag, __VA_ARGS__)
#define sol_fatal(tag, ...) \
    sol_log(SOL_LOG_FATAL, __FILE__, __LINE__, tag, __VA_ARGS__)

/* --- Configuration --- */
void sol_log_set_level(SolLogLevel min_level);    /* default: INFO */
void sol_log_set_quiet(bool quiet);               /* suppress stdout */

/* --- Ring buffer for crash postmortem --- */
void sol_log_dump_ring(void);                     /* print last 256 messages */

/* --- Python callback hook ---
   Set a function to receive all log messages (e.g. route to Python logging).
   Called from whatever thread produced the log. Must be signal-safe. */
typedef void (*SolLogCallback)(SolLogLevel level, const char* file, int line,
                                const char* tag, const char* message);
void sol_log_set_callback(SolLogCallback cb);

#endif /* SOL_DEBUG_LOGGER_H */
