#ifndef SOL_TEXT_UTF8_H
#define SOL_TEXT_UTF8_H

#include <stdbool.h>
#include <stdint.h>
#include <stddef.h>

/* ------------------------------------------------------------------ */
/* UTF-8 iterator — validate and iterate codepoints.                   */
/*                                                                     */
/* Usage:                                                              */
/*   const char* s = "Hello 世界 🌍!";                                 */
/*   Utf8Iter it;                                                      */
/*   for (const char* p = s; *p; ) {                                  */
/*       p = utf8_next(p, &it);                                      */
/*       if (it.valid) printf("U+%04X\n", it.codepoint);             */
/*   }                                                                 */
/* ------------------------------------------------------------------ */

typedef struct {
    uint32_t codepoint;   /* 0x0000 – 0x10FFFF, or 0xFFFD on error */
    int      bytes;       /* 1–4, or 0 at end of string */
    bool     valid;       /* true if valid UTF-8 */
} Utf8Iter;

/* --- Iterate one codepoint. Returns pointer to next byte. --- */
const char* utf8_next(const char* s, Utf8Iter* out);

/* --- Decode a single codepoint without advancing (returns byte count) --- */
int utf8_decode(const char* s, uint32_t* codepoint);

/* --- Encode a codepoint into a buffer (needs 4 bytes). Returns byte count. --- */
int utf8_encode(uint32_t cp, char* out);

/* --- Count codepoints in a UTF-8 string --- */
size_t utf8_strlen(const char* s);

/* --- Check if a byte is a UTF-8 continuation byte (10xxxxxx) --- */
bool utf8_is_continuation(char c);

/* --- Check if codepoint is emoji (basic detection, §emoji.h for full) --- */
bool utf8_is_emoji(uint32_t cp);

#endif /* SOL_TEXT_UTF8_H */
