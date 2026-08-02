#include "utf8.h"
#include <string.h>

/* ------------------------------------------------------------------ */
/* Decode one UTF-8 codepoint                                          */
/* ------------------------------------------------------------------ */
int utf8_decode(const char* s, uint32_t* cp) {
    if (!s || !*s) { *cp = 0; return 0; }

    unsigned char c = (unsigned char)s[0];

    if (c < 0x80) {
        *cp = c;
        return 1;
    }

    if ((c & 0xE0) == 0xC0) {
        /* 2-byte sequence */
        if (((unsigned char)s[1] & 0xC0) != 0x80) { *cp = 0xFFFD; return 1; }
        *cp = ((uint32_t)(c & 0x1F) << 6) | ((uint32_t)s[1] & 0x3F);
        if (*cp < 0x80) { *cp = 0xFFFD; return 1; }  /* overlong */
        return 2;
    }

    if ((c & 0xF0) == 0xE0) {
        /* 3-byte sequence */
        if (((unsigned char)s[1] & 0xC0) != 0x80 ||
            ((unsigned char)s[2] & 0xC0) != 0x80) {
            *cp = 0xFFFD; return 1;
        }
        *cp = ((uint32_t)(c & 0x0F) << 12) |
              ((uint32_t)(s[1] & 0x3F) << 6) |
              ((uint32_t)s[2] & 0x3F);
        if (*cp < 0x800) { *cp = 0xFFFD; return 1; }  /* overlong */
        if (*cp >= 0xD800 && *cp <= 0xDFFF) { *cp = 0xFFFD; return 1; } /* surrogate */
        return 3;
    }

    if ((c & 0xF8) == 0xF0) {
        /* 4-byte sequence */
        if (((unsigned char)s[1] & 0xC0) != 0x80 ||
            ((unsigned char)s[2] & 0xC0) != 0x80 ||
            ((unsigned char)s[3] & 0xC0) != 0x80) {
            *cp = 0xFFFD; return 1;
        }
        *cp = ((uint32_t)(c & 0x07) << 18) |
              ((uint32_t)(s[1] & 0x3F) << 12) |
              ((uint32_t)(s[2] & 0x3F) << 6) |
              ((uint32_t)s[3] & 0x3F);
        if (*cp < 0x10000) { *cp = 0xFFFD; return 1; }  /* overlong */
        if (*cp > 0x10FFFF) { *cp = 0xFFFD; return 1; } /* out of range */
        return 4;
    }

    /* Invalid leading byte */
    *cp = 0xFFFD;
    return 1;
}

/* ------------------------------------------------------------------ */
/* Iterator                                                            */
/* ------------------------------------------------------------------ */
const char* utf8_next(const char* s, Utf8Iter* out) {
    if (!s || !*s) {
        out->codepoint = 0;
        out->bytes     = 0;
        out->valid     = false;
        return s;
    }

    uint32_t cp;
    int n = utf8_decode(s, &cp);

    out->codepoint = cp;
    out->bytes     = n;
    out->valid     = (cp != 0xFFFD);

    return s + n;
}

/* ------------------------------------------------------------------ */
/* Encode                                                              */
/* ------------------------------------------------------------------ */
int utf8_encode(uint32_t cp, char* out) {
    if (cp < 0x80) {
        out[0] = (char)cp;
        return 1;
    }
    if (cp < 0x800) {
        out[0] = (char)(0xC0 | (cp >> 6));
        out[1] = (char)(0x80 | (cp & 0x3F));
        return 2;
    }
    if (cp < 0x10000) {
        out[0] = (char)(0xE0 | (cp >> 12));
        out[1] = (char)(0x80 | ((cp >> 6) & 0x3F));
        out[2] = (char)(0x80 | (cp & 0x3F));
        return 3;
    }
    out[0] = (char)(0xF0 | (cp >> 18));
    out[1] = (char)(0x80 | ((cp >> 12) & 0x3F));
    out[2] = (char)(0x80 | ((cp >> 6) & 0x3F));
    out[3] = (char)(0x80 | (cp & 0x3F));
    return 4;
}

/* ------------------------------------------------------------------ */
/* Helpers                                                             */
/* ------------------------------------------------------------------ */
size_t utf8_strlen(const char* s) {
    size_t count = 0;
    while (*s) {
        if (((unsigned char)*s & 0xC0) != 0x80) count++;  /* skip continuations */
        s++;
    }
    return count;
}

bool utf8_is_continuation(char c) {
    return ((unsigned char)c & 0xC0) == 0x80;
}

/* ------------------------------------------------------------------ */
/* Basic emoji detection — Unicode block ranges                        */
/* ------------------------------------------------------------------ */
bool utf8_is_emoji(uint32_t cp) {
    return (cp >= 0x1F600 && cp <= 0x1F64F) ||  /* Emoticons */
           (cp >= 0x1F300 && cp <= 0x1F5FF) ||  /* Misc Symbols & Pictographs */
           (cp >= 0x1F680 && cp <= 0x1F6FF) ||  /* Transport & Map */
           (cp >= 0x1F900 && cp <= 0x1F9FF) ||  /* Supplemental Symbols */
           (cp >= 0x2600 && cp <= 0x26FF)   ||  /* Misc Symbols */
           (cp >= 0x2700 && cp <= 0x27BF)   ||  /* Dingbats */
           (cp >= 0xFE00 && cp <= 0xFE0F)   ||  /* Variation Selectors */
           (cp >= 0x1F1E0 && cp <= 0x1F1FF) ||  /* Flags */
           (cp == 0x200D)                    ||  /* ZWJ (emoji sequences) */
           (cp == 0xFE0F);                       /* VS16 (emoji presentation) */
}
