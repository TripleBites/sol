#define _POSIX_C_SOURCE 200809L
#include "crash.h"
#include "logger.h"
#include "mem.h"
#include <stdio.h>
#include <stdlib.h>
#include <signal.h>
#include <string.h>
#include <unistd.h>

/* Current frame info for crash context */
static int    g_crash_frame = -1;
static char   g_crash_phase[64] = "init";

/* ------------------------------------------------------------------ */
/* Signal handler                                                      */
/* ------------------------------------------------------------------ */
static const char* signal_name(int sig) {
    switch (sig) {
    case SIGSEGV: return "SIGSEGV (invalid memory access)";
    case SIGABRT: return "SIGABRT (abort)";
    case SIGFPE:  return "SIGFPE (arithmetic error)";
    case SIGILL:  return "SIGILL (illegal instruction)";
    case SIGBUS:  return "SIGBUS (bus error)";
    default:      return "UNKNOWN";
    }
}

static void crash_signal_handler(int sig, siginfo_t* info, void* ctx) {
    (void)ctx;

    fprintf(stderr, "\n");
    fprintf(stderr, "╔══════════════════════════════════════════╗\n");
    fprintf(stderr, "║  ENGINE CRASH                            ║\n");
    fprintf(stderr, "╠══════════════════════════════════════════╣\n");
    fprintf(stderr, "║  Signal: %-31s ║\n", signal_name(sig));

    if (info && sig == SIGSEGV) {
        fprintf(stderr, "║  Address: %p                     ║\n", info->si_addr);
        if (info->si_code == SEGV_MAPERR) {
            fprintf(stderr, "║  Cause: unmapped memory                 ║\n");
        } else if (info->si_code == SEGV_ACCERR) {
            fprintf(stderr, "║  Cause: permission denied               ║\n");
        }
    }

    fprintf(stderr, "║  Frame:  %-31d ║\n", g_crash_frame);
    fprintf(stderr, "║  Phase:  %-31s ║\n", g_crash_phase);
    fprintf(stderr, "╠══════════════════════════════════════════╣\n");

    /* Memory stats */
    size_t total = sol_mem_total_allocated();
    size_t count = sol_mem_allocation_count();
    fprintf(stderr, "║  Memory: %zu bytes in %zu allocations      ║\n",
            total, count);

    fprintf(stderr, "╠══════════════════════════════════════════╣\n");

    /* Log ring buffer */
    fprintf(stderr, "║  Recent log messages:                    ║\n");
    sol_log_dump_ring();

    fprintf(stderr, "╚══════════════════════════════════════════╝\n");
    fflush(stderr);

    /* Re-raise default handler */
    signal(sig, SIG_DFL);
    raise(sig);
}

/* ------------------------------------------------------------------ */
/* Install / API                                                       */
/* ------------------------------------------------------------------ */
void crash_handler_install(void) {
    struct sigaction sa;
    memset(&sa, 0, sizeof(sa));
    sa.sa_sigaction = crash_signal_handler;
    sa.sa_flags = SA_SIGINFO;

    sigaction(SIGSEGV, &sa, NULL);
    sigaction(SIGABRT, &sa, NULL);
    sigaction(SIGFPE,  &sa, NULL);
    sigaction(SIGILL,  &sa, NULL);
    sigaction(SIGBUS,  &sa, NULL);

    sol_info("crash", "signal handlers installed");
}

void crash_dump(void) {
    crash_signal_handler(SIGABRT, NULL, NULL);
}

void crash_set_frame(int frame_number, const char* phase) {
    g_crash_frame = frame_number;
    if (phase) {
        strncpy(g_crash_phase, phase, sizeof(g_crash_phase) - 1);
        g_crash_phase[sizeof(g_crash_phase) - 1] = '\0';
    }
}
