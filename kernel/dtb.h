#ifndef DTB_H
#define DTB_H

#include "../lib/types.h"

/*
 * Parse a Flattened Device Tree blob to extract hardware memory size.
 * QEMU passes the DTB address in x0 at boot (Linux AArch64 boot protocol).
 *
 * Returns RAM size in MB, or 0 if the DTB is absent / unreadable.
 */
uint32_t dtb_get_ram_mb(const void *fdt);

#endif
