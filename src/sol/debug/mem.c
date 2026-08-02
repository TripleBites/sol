#include "mem.h"

#if SOL_MEM_TRACKING

#include <stdlib.h>
#include <string.h>
#include <stdio.h>

/* ------------------------------------------------------------------ */
/* Simple hash table for pointer → record lookup                       */
/* ------------------------------------------------------------------ */
#define HASH_SIZE 1024

typedef struct MemEntry {
    SolAllocRecord record;
    struct MemEntry* next;   /* chaining for collisions */
} MemEntry;

static MemEntry* g_hash[HASH_SIZE];
static size_t   g_total_allocated = 0;
static size_t   g_alloc_count     = 0;
static uint64_t g_next_id         = 1;

static unsigned hash_ptr(void* p) {
    uintptr_t v = (uintptr_t)p;
    return (unsigned)((v >> 3) & (HASH_SIZE - 1));
}

static void hash_insert(MemEntry* e) {
    unsigned bucket = hash_ptr(e->record.ptr);
    e->next = g_hash[bucket];
    g_hash[bucket] = e;
}

static MemEntry* hash_find(void* p) {
    unsigned bucket = hash_ptr(p);
    for (MemEntry* e = g_hash[bucket]; e; e = e->next) {
        if (e->record.ptr == p) return e;
    }
    return NULL;
}

static void hash_remove(void* p) {
    unsigned bucket = hash_ptr(p);
    MemEntry** prev = &g_hash[bucket];
    for (MemEntry* e = g_hash[bucket]; e; e = e->next) {
        if (e->record.ptr == p) {
            *prev = e->next;
            free(e);
            return;
        }
        prev = &e->next;
    }
}

/* ------------------------------------------------------------------ */
/* Public API                                                          */
/* ------------------------------------------------------------------ */
void* sol_malloc_tag(size_t size, const char* file, int line, const char* tag) {
    void* ptr = malloc(size);
    if (!ptr) return NULL;

    MemEntry* e = malloc(sizeof(MemEntry));
    if (!e) { free(ptr); return NULL; }

    e->record.ptr      = ptr;
    e->record.size     = size;
    e->record.file     = file;
    e->record.line     = line;
    e->record.tag      = tag;
    e->record.alloc_id = g_next_id++;

    hash_insert(e);
    g_total_allocated += size;
    g_alloc_count++;

    return ptr;
}

void* sol_calloc_tag(size_t n, size_t size, const char* file, int line,
                      const char* tag) {
    void* ptr = sol_malloc_tag(n * size, file, line, tag);
    if (ptr) memset(ptr, 0, n * size);
    return ptr;
}

void* sol_realloc_tag(void* ptr, size_t size, const char* file, int line,
                       const char* tag) {
    if (!ptr) return sol_malloc_tag(size, file, line, tag);
    if (size == 0) { sol_free_tag(ptr, file, line); return NULL; }

    MemEntry* old = hash_find(ptr);
    size_t old_size = old ? old->record.size : 0;

    void* new_ptr = realloc(ptr, size);
    if (!new_ptr) return NULL;

    if (old) {
        hash_remove(ptr);
        old->record.ptr  = new_ptr;
        old->record.size = size;
        hash_insert(old);
        g_total_allocated = g_total_allocated - old_size + size;
    }
    return new_ptr;
}

void sol_free_tag(void* ptr, const char* file, int line) {
    (void)file; (void)line;
    if (!ptr) return;

    MemEntry* e = hash_find(ptr);
    if (e) {
        g_total_allocated -= e->record.size;
        g_alloc_count--;
        hash_remove(ptr);
    }
    free(ptr);
}

/* ------------------------------------------------------------------ */
/* Queries                                                             */
/* ------------------------------------------------------------------ */
size_t sol_mem_total_allocated(void) {
    return g_total_allocated;
}

size_t sol_mem_allocation_count(void) {
    return g_alloc_count;
}

void sol_mem_dump_leaks(void) {
    size_t leak_count = 0;
    size_t leak_bytes = 0;

    for (int i = 0; i < HASH_SIZE; i++) {
        for (MemEntry* e = g_hash[i]; e; e = e->next) {
            leak_count++;
            leak_bytes += e->record.size;
            fprintf(stderr,
                    "[mem] LEAK: %zu bytes at %p [%s] %s:%d (id=%llu)\n",
                    e->record.size, e->record.ptr, e->record.tag,
                    e->record.file, e->record.line,
                    (unsigned long long)e->record.alloc_id);
        }
    }

    if (leak_count > 0) {
        fprintf(stderr, "[mem] %zu leaks, %zu bytes total\n",
                leak_count, leak_bytes);
    } else {
        fprintf(stderr, "[mem] No leaks detected.\n");
    }
}

SolAllocRecord* sol_mem_find(void* ptr) {
    MemEntry* e = hash_find(ptr);
    return e ? &e->record : NULL;
}

void sol_mem_dump_by_tag(const char* tag) {
    size_t count = 0;
    for (int i = 0; i < HASH_SIZE; i++) {
        for (MemEntry* e = g_hash[i]; e; e = e->next) {
            if (strcmp(e->record.tag, tag) == 0) {
                count++;
                fprintf(stderr, "[mem] TAG '%s': %zu bytes @ %p (%s:%d)\n",
                        tag, e->record.size, e->record.ptr,
                        e->record.file, e->record.line);
            }
        }
    }
    if (count == 0) {
        fprintf(stderr, "[mem] No allocations with tag '%s'\n", tag);
    }
}

#endif /* SOL_MEM_TRACKING */
