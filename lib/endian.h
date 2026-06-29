#ifndef ENDIAN_H
#define ENDIAN_H

#include "types.h"

/* Read a big-endian value byte-by-byte — safe on any alignment */
static inline uint32_t be32_read(const void *p) {
    const unsigned char *b = (const unsigned char *)p;
    return ((uint32_t)b[0] << 24) | ((uint32_t)b[1] << 16) |
           ((uint32_t)b[2] <<  8) | (uint32_t)b[3];
}

static inline uint64_t be64_read(const void *p) {
    const unsigned char *b = (const unsigned char *)p;
    return ((uint64_t)b[0] << 56) | ((uint64_t)b[1] << 48) |
           ((uint64_t)b[2] << 40) | ((uint64_t)b[3] << 32) |
           ((uint64_t)b[4] << 24) | ((uint64_t)b[5] << 16) |
           ((uint64_t)b[6] <<  8) | (uint64_t)b[7];
}

#endif
