#ifndef SOL_DEBUG_CRASH_H
#define SOL_DEBUG_CRASH_H

/* Crash handler — installs signal handlers for SIGSEGV, SIGABRT, SIGFPE.
   On crash, dumps the log ring buffer and memory stats before exiting.
   
   Usage:
     #include "debug/crash.h"
     crash_handler_install();   // call once at startup
*/

void crash_handler_install(void);

/* Manually trigger a crash dump (for debugging). */
void crash_dump(void);

/* Set a frame marker — logged before each frame so crash reports
   show which frame number was being processed. */
void crash_set_frame(int frame_number, const char* phase);

#endif /* SOL_DEBUG_CRASH_H */
