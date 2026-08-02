#ifndef SOL_DEBUG_MEM_H
#define SOL_DEBUG_MEM_H

#include <stddef.h>
#include <stdbool.h>
#include <stdint.h>

/* ------------------------------------------------------------------ */
/* Tracked memory allocation — drop-in replacement for malloc/free.    */
/*                                                                     */
/* Every allocation is recorded with file, line, and tag. At shutdown, */
/* sol_mem_dump_leaks() prints everything still allocated.             */
/*                                                                     */
/* Compile with SOL_MEM_TRACKING=0 to disable (passthrough to stdlib). */
/* ------------------------------------------------------------------ */

#ifndef SOL_MEM_TRACKING
#define SOL_MEM_TRACKING 1
#endif

/* --- Allocation records --- */
typedef struct SolAllocRecord {
    void*       ptr;
    size_t      size;
    const char* file;
    int         line;
    const char* tag;        /* "Node", "Control", "AudioPipeline", etc. */
    uint64_t    alloc_id;   /* monotonic, unique */
    uint64_t    alloc_time; /* timestamp (not implemented yet) */
} SolAllocRecord;

#if SOL_MEM_TRACKING

/* --- Tracked allocation --- */
void* sol_malloc_tag(size_t size, const char* file, int line, const char* tag);
void* sol_calloc_tag(size_t n, size_t size, const char* file, int line, const char* tag);
void* sol_realloc_tag(void* ptr, size_t size, const char* file, int line, const char* tag);
void  sol_free_tag(void* ptr, const char* file, int line);

/* --- Convenience macros --- */
#define sol_malloc(sz, tag)       sol_malloc_tag(sz, __FILE__, __LINE__, tag)
#define sol_calloc(n, sz, tag)    sol_calloc_tag(n, sz, __FILE__, __LINE__, tag)
#define sol_realloc(p, sz, tag)   sol_realloc_tag(p, sz, __FILE__, __LINE__, tag)
#define sol_free(ptr, tag)        sol_free_tag(ptr, __FILE__, __LINE__, tag)

/* --- Queries --- */
size_t         sol_mem_total_allocated(void);    /* bytes currently allocated */
size_t         sol_mem_allocation_count(void);    /* live allocations */
void           sol_mem_dump_leaks(void);          /* print to stderr */
SolAllocRecord* sol_mem_find(void* ptr);          /* lookup by pointer */
void           sol_mem_dump_by_tag(const char* tag); /* filter by tag */

#else /* !SOL_MEM_TRACKING — passthrough */

#include <stdlib.h>
#define sol_malloc(sz, tag)       malloc(sz)
#define sol_calloc(n, sz, tag)    calloc(n, sz)
#define sol_realloc(p, sz, tag)   realloc(p, sz)
#define sol_free(ptr, tag)        free(ptr)

#endif /* SOL_MEM_TRACKING */

#endif /* SOL_DEBUG_MEM_H */
