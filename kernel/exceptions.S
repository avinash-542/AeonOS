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

/* ── Context Save Macro ──────────────────────────────────────────────────
 *
 * Saves all general-purpose registers and key system registers onto the
 * kernel stack, producing an exc_frame_t at the new SP value.
 *
 * Frame layout (288 bytes total, 16-byte aligned):
 *   [sp +   0] x0  .. [sp + 240] x30
 *   [sp + 248] ELR_EL1   (exception return address)
 *   [sp + 256] SPSR_EL1  (saved processor state)
 *   [sp + 264] ESR_EL1   (exception syndrome register)
 *   [sp + 272] FAR_EL1   (fault address register)
 *   [sp + 280] (8-byte pad — keeps 288 divisible by 16)
 *
 * x9 and x10 are used as scratch registers after their values
 * are already saved to the frame.
 */
.macro exc_save
    sub  sp,  sp,  #288
    stp  x0,  x1,  [sp, #0]
    stp  x2,  x3,  [sp, #16]
    stp  x4,  x5,  [sp, #32]
    stp  x6,  x7,  [sp, #48]
    stp  x8,  x9,  [sp, #64]
    stp  x10, x11, [sp, #80]
    stp  x12, x13, [sp, #96]
    stp  x14, x15, [sp, #112]
    stp  x16, x17, [sp, #128]
    stp  x18, x19, [sp, #144]
    stp  x20, x21, [sp, #160]
    stp  x22, x23, [sp, #176]
    stp  x24, x25, [sp, #192]
    stp  x26, x27, [sp, #208]
    stp  x28, x29, [sp, #224]
    str  x30,      [sp, #240]
    mrs  x9,  elr_el1
    mrs  x10, spsr_el1
    stp  x9,  x10, [sp, #248]
    mrs  x9,  esr_el1
    mrs  x10, far_el1
    stp  x9,  x10, [sp, #264]
.endm

/* ── Exception Entry Stubs ───────────────────────────────────────────────
 *
 * Each stub:
 *   1. Saves full CPU context via exc_save macro
 *   2. Passes frame pointer (sp) in x0 and exception type in x1
 *   3. Calls the C handler exc_handler(frame, type)
 *   4. Halts — we do not return from exceptions yet
 */

_el1h_sync:
    exc_save
    mov  x0, sp
    mov  x1, #EXC_SYNC_EL1H_VAL
    bl   exc_handler
    b    _exc_halt

_el1h_irq:
    exc_save
    mov  x0, sp
    mov  x1, #EXC_IRQ_EL1H_VAL
    bl   exc_handler
    b    _exc_halt

_el1h_fiq:
    exc_save
    mov  x0, sp
    mov  x1, #EXC_FIQ_EL1H_VAL
    bl   exc_handler
    b    _exc_halt

_el1h_serror:
    exc_save
    mov  x0, sp
    mov  x1, #EXC_SERROR_EL1H_VAL
    bl   exc_handler
    b    _exc_halt

_el0_sync:
    exc_save
    mov  x0, sp
    mov  x1, #EXC_SYNC_EL0_VAL
    bl   exc_handler
    b    _exc_halt

_el0_irq:
    exc_save
    mov  x0, sp
    mov  x1, #EXC_IRQ_EL0_VAL
    bl   exc_handler
    b    _exc_halt

_el0_fiq:
    exc_save
    mov  x0, sp
    mov  x1, #EXC_FIQ_EL0_VAL
    bl   exc_handler
    b    _exc_halt

_el0_serror:
    exc_save
    mov  x0, sp
    mov  x1, #EXC_SERROR_EL0_VAL
    bl   exc_handler
    b    _exc_halt

_exc_invalid:
    exc_save
    mov  x0, sp
    mov  x1, #EXC_INVALID_VAL
    bl   exc_handler
    b    _exc_halt

_exc_halt:
1:  wfe
    b 1b
