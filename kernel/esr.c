#include "esr.h"
#include "../drivers/uart.h"

static const char *ec_name(uint32_t ec) {
    switch (ec) {
        case ESR_EC_UNKNOWN:     return "Unknown reason";
        case ESR_EC_WFx:         return "WFI/WFE instruction trapped";
        case ESR_EC_FP_ACCESS:   return "FP/SIMD access (CPACR disabled)";
        case ESR_EC_ILL_STATE:   return "Illegal execution state";
        case ESR_EC_SVC_AA32:    return "SVC (AArch32 system call)";
        case ESR_EC_SVC_AA64:    return "SVC (AArch64 system call)";
        case ESR_EC_MRS_TRAP:    return "MSR/MRS/System instruction trap";
        case ESR_EC_SVE_TRAP:    return "SVE access (disabled in CPACR)";
        case ESR_EC_IABORT_LO:   return "Instruction abort (lower EL)";
        case ESR_EC_IABORT_CUR:  return "Instruction abort (current EL)";
        case ESR_EC_PC_ALIGN:    return "PC alignment fault";
        case ESR_EC_DABORT_LO:   return "Data abort (lower EL)";
        case ESR_EC_DABORT_CUR:  return "Data abort (current EL)";
        case ESR_EC_SP_ALIGN:    return "SP alignment fault";
        case ESR_EC_FP_EXC_AA64: return "FP/SIMD exception (AArch64)";
        case ESR_EC_SERROR:      return "SError (async system error)";
        case ESR_EC_BRK_CUR:     return "Breakpoint (current EL)";
        case ESR_EC_STEP_CUR:    return "Software step (current EL)";
        case ESR_EC_WATCH_CUR:   return "Watchpoint (current EL)";
        case ESR_EC_BRK_AA64:    return "BRK instruction";
        default:                 return "Unknown EC";
    }
}

static void decode_fsc(uint32_t iss) {
    uint32_t fsc   = iss & 0x3F;
    uint32_t type  = fsc & FSC_TYPE_MASK;
    uint32_t level = fsc & FSC_LEVEL_MASK;

    uart_puts("  Fault  : ");
    switch (type) {
        case FSC_ADDR_SIZE:   uart_puts("Address size fault"); break;
        case FSC_TRANSLATION: uart_puts("Translation fault");  break;
        case FSC_ACCESS_FLAG: uart_puts("Access flag fault");  break;
        case FSC_PERMISSION:  uart_puts("Permission fault");   break;
        default:              uart_puts("Sync external / other"); break;
    }
    uart_puts(" at level "); uart_print_dec(level); uart_newline();

    /* WnR bit [6] for data aborts: 0=read, 1=write */
    if (iss & (1 << 6))
        uart_puts("  Access : WRITE");
    else
        uart_puts("  Access : READ");
    uart_newline();
}

void esr_decode(uint64_t esr) {
    uint32_t ec  = (uint32_t)((esr >> ESR_EC_SHIFT) & ESR_EC_MASK);
    uint32_t il  = (uint32_t)((esr >> ESR_IL_SHIFT) & 1);
    uint32_t iss = (uint32_t)(esr & ESR_ISS_MASK);

    uart_puts("[ESR Decode]"); uart_newline();

    uart_puts("  EC     : 0x");
    uart_print_hex64((uint64_t)ec);
    uart_puts("  →  ");
    uart_puts(ec_name(ec));
    uart_newline();

    uart_puts("  IL     : ");
    uart_puts(il ? "32-bit instruction" : "16-bit instruction");
    uart_newline();

    uart_puts("  ISS    : 0x");
    uart_print_hex64((uint64_t)iss);
    uart_newline();

    /* Detailed ISS decode for the most common fault classes */
    switch (ec) {
        case ESR_EC_DABORT_LO:
        case ESR_EC_DABORT_CUR:
        case ESR_EC_IABORT_LO:
        case ESR_EC_IABORT_CUR:
            decode_fsc(iss);
            break;

        case ESR_EC_SVC_AA64:
            uart_puts("  SVC #  : ");
            uart_print_dec((uint64_t)(iss & 0xFFFF));
            uart_newline();
            break;

        case ESR_EC_BRK_AA64:
            uart_puts("  BRK #  : ");
            uart_print_dec((uint64_t)(iss & 0xFFFF));
            uart_newline();
            break;

        default:
            break;
    }
}
