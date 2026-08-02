#include "logger.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>

/* ------------------------------------------------------------------ */
/* State                                                               */
/* ------------------------------------------------------------------ */
static SolLogLevel   g_min_level = SOL_LOG_INFO;
static bool          g_quiet     = false;
static SolLogCallback g_callback = NULL;

/* Ring buffer for crash postmortem */
#define RING_SIZE 256
static struct {
    SolLogLevel level;
    char  file[64];
    int   line;
    char  tag[32];
    char  msg[256];
} g_ring[RING_SIZE];
static int g_ring_pos = 0;

static const char* level_chars = "TD?WEF";  /* Trace Debug ¿ Warn Error Fatal */
static const char* level_colors[] = {
    "\x1b[90m",  /* TRACE: grey */
    "\x1b[36m",  /* DEBUG: cyan */
    "\x1b[0m",   /* INFO:  default */
    "\x1b[33m",  /* WARN:  yellow */
    "\x1b[31m",  /* ERROR: red */
    "\x1b[35m",  /* FATAL: magenta */
};

/* ------------------------------------------------------------------ */
/* Core                                                                */
/* ------------------------------------------------------------------ */
void sol_log_va(SolLogLevel level, const char* file, int line,
                const char* tag, const char* fmt, va_list ap) {
    if (level < g_min_level && !g_callback) return;

    /* Format message */
    char msg[256];
    vsnprintf(msg, sizeof(msg), fmt, ap);

    /* Store in ring buffer */
    int idx = g_ring_pos;
    g_ring_pos = (g_ring_pos + 1) % RING_SIZE;
    g_ring[idx].level = level;
    g_ring[idx].line  = line;
    strncpy(g_ring[idx].file, file, sizeof(g_ring[idx].file) - 1);
    strncpy(g_ring[idx].tag,  tag,  sizeof(g_ring[idx].tag) - 1);
    strncpy(g_ring[idx].msg,  msg,  sizeof(g_ring[idx].msg) - 1);

    /* Python callback */
    if (g_callback) {
        g_callback(level, file, line, tag, msg);
        return;
    }

    if (g_quiet) return;

    /* Terminal output */
    const char* color = (level <= SOL_LOG_FATAL) ? level_colors[level] : "";
    const char* lc    = (level <= SOL_LOG_FATAL) ? &level_chars[level] : "?";

    /* Get filename without path */
    const char* fname = strrchr(file, '/');
    fname = fname ? fname + 1 : file;

    fprintf(stderr, "%s[%c] %s:%d [%s] %s\x1b[0m\n",
            color, *lc, fname, line, tag, msg);
}

void sol_log(SolLogLevel level, const char* file, int line,
             const char* tag, const char* fmt, ...) {
    va_list ap;
    va_start(ap, fmt);
    sol_log_va(level, file, line, tag, fmt, ap);
    va_end(ap);
}

/* ------------------------------------------------------------------ */
/* Configuration                                                       */
/* ------------------------------------------------------------------ */
void sol_log_set_level(SolLogLevel min_level) {
    g_min_level = min_level;
}

void sol_log_set_quiet(bool quiet) {
    g_quiet = quiet;
}

void sol_log_set_callback(SolLogCallback cb) {
    g_callback = cb;
}

/* ------------------------------------------------------------------ */
/* Ring buffer dump                                                    */
/* ------------------------------------------------------------------ */
void sol_log_dump_ring(void) {
    fprintf(stderr, "[log] Crash postmortem — last %d messages:\n", RING_SIZE);
    for (int i = 0; i < RING_SIZE; i++) {
        int idx = (g_ring_pos + i) % RING_SIZE;
        if (g_ring[idx].file[0] == '\0') continue;
        fprintf(stderr, "  [%c] %s:%d [%s] %s\n",
                level_chars[g_ring[idx].level],
                g_ring[idx].file, g_ring[idx].line,
                g_ring[idx].tag, g_ring[idx].msg);
    }
}
