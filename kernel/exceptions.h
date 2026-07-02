#ifndef EXCEPTIONS_H
#define EXCEPTIONS_H

/*
 * Shared between C and assembly — guard C-only declarations with
 * __ASSEMBLER__ so the assembler does not trip over typedefs.
 */

#ifndef __ASSEMBLER__
#include "../lib/types.h"

/*
 * Exception frame saved on the kernel stack when any exception fires.
 * Assembly saves registers in this exact order; the layout must not
 * change without updating the save/restore macros in exceptions.S.
 *
 * Stack layout (sp = frame base after sub sp, sp, #288):
 *   offset   0 : x0
 *   offset   8 : x1
 *   ...
 *   offset 240 : x30
 *   offset 248 : ELR_EL1  (exception return address)
 *   offset 256 : SPSR_EL1 (saved processor state)
 *   offset 264 : ESR_EL1  (exception syndrome)
 *   offset 272 : FAR_EL1  (fault address)
 *   offset 280 : (8-byte pad — keeps frame 16-byte aligned at 288 bytes)
 */
typedef struct {
    uint64_t x[31];   /* x0-x30              offsets   0..240 */
    uint64_t elr;     /* ELR_EL1             offset   248     */
    uint64_t spsr;    /* SPSR_EL1            offset   256     */
    uint64_t esr;     /* ESR_EL1             offset   264     */
    uint64_t far;     /* FAR_EL1             offset   272     */
} exc_frame_t;

typedef enum {
    EXC_SYNC_EL1H   = 0,   /* synchronous exception, kernel (SP_EL1) */
    EXC_IRQ_EL1H    = 1,   /* IRQ,    kernel                          */
    EXC_FIQ_EL1H    = 2,   /* FIQ,    kernel                          */
    EXC_SERROR_EL1H = 3,   /* SError, kernel                          */
    EXC_SYNC_EL0    = 4,   /* synchronous, user space (AArch64)        */
    EXC_IRQ_EL0     = 5,   /* IRQ,    user space                       */
    EXC_FIQ_EL0     = 6,   /* FIQ,    user space                       */
    EXC_SERROR_EL0  = 7,   /* SError, user space                       */
    EXC_INVALID     = 0xFF, /* entry that should never fire            */
} exc_type_t;

/* Install exception_vectors into VBAR_EL1 */
void exceptions_init(void);

/* C-level exception handler — called from assembly stubs */
void exc_handler(const exc_frame_t *frame, uint64_t type);

#endif /* __ASSEMBLER__ */

/* Numeric constants — also usable in assembly */
#define EXC_SYNC_EL1H_VAL    0
#define EXC_IRQ_EL1H_VAL     1
#define EXC_FIQ_EL1H_VAL     2
#define EXC_SERROR_EL1H_VAL  3
#define EXC_SYNC_EL0_VAL     4
#define EXC_IRQ_EL0_VAL      5
#define EXC_FIQ_EL0_VAL      6
#define EXC_SERROR_EL0_VAL   7
#define EXC_INVALID_VAL      0xFF

#endif /* EXCEPTIONS_H */
