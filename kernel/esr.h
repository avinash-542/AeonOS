#ifndef ESR_H
#define ESR_H

#include "../lib/types.h"

/*
 * ESR_EL1 — Exception Syndrome Register (EL1)
 *
 * Bits [31:26] EC  — Exception Class: what type of exception occurred
 * Bit  [25]    IL  — Instruction Length: 0=16-bit, 1=32-bit instruction
 * Bits [24:0]  ISS — Instruction Specific Syndrome: fault details
 */

#define ESR_EC_SHIFT        26
#define ESR_EC_MASK         0x3F
#define ESR_IL_SHIFT        25
#define ESR_ISS_MASK        0x1FFFFFF

/* Exception Class (EC) values */
#define ESR_EC_UNKNOWN      0x00  /* unknown reason                          */
#define ESR_EC_WFx          0x01  /* WFI or WFE trapped                      */
#define ESR_EC_MCR_CP15     0x03  /* MCR/MRC to CP15 (AArch32)               */
#define ESR_EC_MCRR_CP15    0x04  /* MCRR/MRRC to CP15 (AArch32)             */
#define ESR_EC_MCR_CP14     0x05  /* MCR/MRC to CP14 (AArch32)               */
#define ESR_EC_LDC_CP14     0x06  /* LDC/STC to CP14 (AArch32)               */
#define ESR_EC_FP_ACCESS    0x07  /* FP/SIMD access disabled                 */
#define ESR_EC_LD64B        0x0A  /* LD64B or ST64B* trapped                 */
#define ESR_EC_BRANCH_TGT   0x0D  /* Branch target exception                 */
#define ESR_EC_ILL_STATE    0x0E  /* Illegal execution state                 */
#define ESR_EC_SVC_AA32     0x11  /* SVC from AArch32                        */
#define ESR_EC_SVC_AA64     0x15  /* SVC from AArch64 (system call)          */
#define ESR_EC_MRS_TRAP     0x18  /* MSR/MRS/System instruction trap         */
#define ESR_EC_SVE_TRAP     0x19  /* SVE access disabled                     */
#define ESR_EC_IABORT_LO    0x20  /* Instruction abort from lower EL         */
#define ESR_EC_IABORT_CUR   0x21  /* Instruction abort from current EL       */
#define ESR_EC_PC_ALIGN     0x22  /* PC alignment fault                      */
#define ESR_EC_DABORT_LO    0x24  /* Data abort from lower EL                */
#define ESR_EC_DABORT_CUR   0x25  /* Data abort from current EL              */
#define ESR_EC_SP_ALIGN     0x26  /* SP alignment fault                      */
#define ESR_EC_FP_EXC_AA32  0x28  /* FP exception (AArch32)                  */
#define ESR_EC_FP_EXC_AA64  0x2C  /* FP exception (AArch64)                  */
#define ESR_EC_SERROR       0x2F  /* SError interrupt                        */
#define ESR_EC_BRK_LO       0x30  /* Breakpoint from lower EL                */
#define ESR_EC_BRK_CUR      0x31  /* Breakpoint from current EL              */
#define ESR_EC_STEP_LO      0x32  /* Software step from lower EL             */
#define ESR_EC_STEP_CUR     0x33  /* Software step from current EL           */
#define ESR_EC_WATCH_LO     0x34  /* Watchpoint from lower EL                */
#define ESR_EC_WATCH_CUR    0x35  /* Watchpoint from current EL              */
#define ESR_EC_BRK_AA64     0x3C  /* BRK instruction (AArch64)               */

/*
 * Data/Instruction Fault Status Code (DFSC/IFSC) — ISS bits [5:0]
 * Bits [5:4] = fault type, bits [3:2] = reserved, bits [1:0] = level
 */
#define FSC_ADDR_SIZE       0x00  /* address size fault (bits[5:2] = 0b00xx) */
#define FSC_TRANSLATION     0x04  /* translation fault  (bits[5:2] = 0b01xx) */
#define FSC_ACCESS_FLAG     0x08  /* access flag fault  (bits[5:2] = 0b10xx) */
#define FSC_PERMISSION      0x0C  /* permission fault   (bits[5:2] = 0b11xx) */
#define FSC_TYPE_MASK       0x3C  /* mask for fault type (bits [5:2])        */
#define FSC_LEVEL_MASK      0x03  /* mask for translation level (bits [1:0]) */

/*
 * Decode ESR_EL1 and print a human-readable fault description over UART.
 * Called from exc_handler() after the register dump.
 */
void esr_decode(uint64_t esr);

#endif /* ESR_H */
