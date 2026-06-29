#include "dtb.h"
#include "../lib/endian.h"

/* ── FDT constants ───────────────────────────────────────────────────────── */

#define FDT_MAGIC       0xD00DFEED
#define FDT_BEGIN_NODE  0x00000001
#define FDT_END_NODE    0x00000002
#define FDT_PROP        0x00000003
#define FDT_NOP         0x00000004
#define FDT_END         0x00000009

/*
 * FDT header layout (all fields big-endian uint32):
 *   offset  0: magic
 *   offset  4: totalsize
 *   offset  8: off_dt_struct
 *   offset 12: off_dt_strings
 *   offset 16: off_mem_rsvmap
 *   offset 20: version
 *   offset 24: last_comp_version
 *   offset 28: boot_cpuid_phys
 *   offset 32: size_dt_strings
 *   offset 36: size_dt_struct
 */

/* ── Helpers ─────────────────────────────────────────────────────────────── */

/* Align n up to the next 4-byte boundary */
static inline int align4(int n) {
    return (n + 3) & ~3;
}

static int str_eq(const char *a, const char *b) {
    while (*a && *b) {
        if (*a++ != *b++) return 0;
    }
    return *a == *b;
}

/* Does string s start with prefix? */
static int str_starts(const char *s, const char *prefix) {
    while (*prefix) {
        if (*s++ != *prefix++) return 0;
    }
    return 1;
}

/* Length of null-terminated string (no libc) */
static int str_len(const char *s) {
    int n = 0;
    while (s[n]) n++;
    return n;
}

/* ── DTB walker ──────────────────────────────────────────────────────────── */

uint32_t dtb_get_ram_mb(const void *fdt) {
    if (!fdt) return 0;

    /* Validate magic */
    if (be32_read(fdt) != FDT_MAGIC) return 0;

    const unsigned char *base = (const unsigned char *)fdt;

    uint32_t off_struct  = be32_read(base + 8);
    uint32_t off_strings = be32_read(base + 12);

    const unsigned char *strings = base + off_strings;
    const unsigned char *p       = base + off_struct;

    int depth      = 0;
    int in_memory  = 0;
    int guard      = 65536; /* max tokens — prevents infinite loop on corrupt DTB */

    while (guard-- > 0) {
        uint32_t token = be32_read(p);
        p += 4;

        switch (token) {

        case FDT_BEGIN_NODE: {
            const char *name = (const char *)p;
            int len = str_len(name);
            p += align4(len + 1);
            depth++;

            /*
             * The root node is depth 1 (empty name).
             * Memory node is depth 2, named "memory" or "memory@XXXXXXXX".
             */
            if (depth == 2 && str_starts(name, "memory"))
                in_memory = 1;
            break;
        }

        case FDT_END_NODE:
            if (in_memory && depth == 2)
                in_memory = 0;
            depth--;
            if (depth < 0) return 0; /* malformed */
            break;

        case FDT_PROP: {
            uint32_t prop_len = be32_read(p);
            uint32_t name_off = be32_read(p + 4);
            const unsigned char *data = p + 8;
            p += 8 + align4((int)prop_len);

            if (!in_memory || depth != 2)
                break;

            const char *prop_name = (const char *)(strings + name_off);
            if (!str_eq(prop_name, "reg"))
                break;

            /*
             * QEMU virt memory node reg layout (2 cells per field):
             *   cell[0] = addr high (uint32)
             *   cell[1] = addr low  (uint32) = 0x40000000
             *   cell[2] = size high (uint32)
             *   cell[3] = size low  (uint32) = <ram bytes>
             *
             * 4 cells × 4 bytes = 16 bytes minimum.
             */
            if (prop_len < 16) break;

            uint64_t size = ((uint64_t)be32_read(data + 8) << 32)
                          | (uint64_t)be32_read(data + 12);

            uint32_t mb = (uint32_t)(size >> 20);
            return mb > 0 ? mb : 0;
        }

        case FDT_NOP:
            break;

        case FDT_END:
        default:
            return 0;
        }
    }

    return 0; /* walked entire tree, no memory node found */
}
