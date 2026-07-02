#include "exceptions.h"

.section ".text"

/* ── exceptions_init ─────────────────────────────────────────────────────
 * Install the vector table address into VBAR_EL1.
 * ADR gives a PC-relative address — no need for absolute addressing.
 * ISB ensures the CPU sees the new VBAR before the next exception.
 */
.global exceptions_init
exceptions_init:
    adr  x0, exception_vectors
    msr  vbar_el1, x0
    isb
    ret

/* ── Exception Vector Table ──────────────────────────────────────────────
 *
 * AArch64 requires the vector table to be aligned to 2^11 = 2048 bytes.
 * There are 16 entries, each 2^7 = 128 bytes apart, organised in 4 groups:
 *
 *   +0x000  Group 1: Current EL, SP_EL0  (EL1t) — not used in AeonOS
 *   +0x200  Group 2: Current EL, SP_EL1  (EL1h) — kernel exceptions
 *   +0x400  Group 3: Lower EL, AArch64   (EL0)  — future user space
 *   +0x600  Group 4: Lower EL, AArch32   (EL0)  — future 32-bit compat
 *
 * Each entry is a single branch instruction (4 bytes). The remaining
 * 124 bytes of each 128-byte slot are unused padding.
 * The .align 7 directive advances to the next 128-byte boundary.
 */

.align 11
.global exception_vectors
exception_vectors:

/* Group 1 — EL1t: SP_EL0 while at EL1 (should never occur in AeonOS) */
.align 7;  b _exc_invalid       /* +0x000  Synchronous */
.align 7;  b _exc_invalid       /* +0x080  IRQ         */
.align 7;  b _exc_invalid       /* +0x100  FIQ         */
.align 7;  b _exc_invalid       /* +0x180  SError      */

/* Group 2 — EL1h: kernel-mode exceptions (SP_EL1 in use) */
.align 7;  b _el1h_sync         /* +0x200  Synchronous */
.align 7;  b _el1h_irq          /* +0x280  IRQ         */
.align 7;  b _el1h_fiq          /* +0x300  FIQ         */
.align 7;  b _el1h_serror       /* +0x380  SError      */

/* Group 3 — EL0 AArch64: future 64-bit user-space exceptions */
.align 7;  b _el0_sync          /* +0x400  Synchronous */
.align 7;  b _el0_irq           /* +0x480  IRQ         */
.align 7;  b _el0_fiq           /* +0x500  FIQ         */
.align 7;  b _el0_serror        /* +0x580  SError      */

/* Group 4 — EL0 AArch32: future 32-bit compat user-space exceptions */
.align 7;  b _exc_invalid       /* +0x600  Synchronous */
.align 7;  b _exc_invalid       /* +0x680  IRQ         */
.align 7;  b _exc_invalid       /* +0x700  FIQ         */
.align 7;  b _exc_invalid       /* +0x780  SError      */

/* ── Placeholder stubs ───────────────────────────────────────────────────
 * All handlers currently halt. Context saving and UART reporting
 * will be added in the next commit.
 */
_el1h_sync:
_el1h_irq:
_el1h_fiq:
_el1h_serror:
_el0_sync:
_el0_irq:
_el0_fiq:
_el0_serror:
_exc_invalid:
1:  wfe
    b 1b
