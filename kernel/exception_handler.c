#include "exceptions.h"
#include "esr.h"
#include "../drivers/uart.h"

static const char *exc_type_name(uint64_t type) {
    switch ((exc_type_t)type) {
        case EXC_SYNC_EL1H:    return "Synchronous (kernel)";
        case EXC_IRQ_EL1H:     return "IRQ (kernel)";
        case EXC_FIQ_EL1H:     return "FIQ (kernel)";
        case EXC_SERROR_EL1H:  return "SError (kernel)";
        case EXC_SYNC_EL0:     return "Synchronous (user)";
        case EXC_IRQ_EL0:      return "IRQ (user)";
        case EXC_FIQ_EL0:      return "FIQ (user)";
        case EXC_SERROR_EL0:   return "SError (user)";
        default:               return "Invalid / Unexpected";
    }
}

static void print_reg(const char *name, uint64_t val) {
    uart_puts("  ");
    uart_puts(name);
    uart_puts(": ");
    uart_print_hex64(val);
    uart_newline();
}

void exc_handler(const exc_frame_t *frame, uint64_t type) {
    uart_newline();
    uart_puts("=============================="); uart_newline();
    uart_puts("    !! KERNEL EXCEPTION !!    "); uart_newline();
    uart_puts("=============================="); uart_newline();
    uart_newline();

    uart_puts("Type : "); uart_puts(exc_type_name(type)); uart_newline();
    uart_newline();

    /* Key system registers */
    uart_puts("[Exception State]"); uart_newline();
    print_reg("ELR  ", frame->elr);    /* where we came from       */
    print_reg("SPSR ", frame->spsr);   /* processor state at fault */
    print_reg("ESR  ", frame->esr);    /* why it happened          */
    print_reg("FAR  ", frame->far);    /* faulting address         */
    uart_newline();

    /* General purpose registers */
    uart_puts("[Registers]"); uart_newline();
    for (int i = 0; i <= 28; i += 2) {
        uart_puts("  x");
        uart_print_dec((unsigned long long)i);
        uart_puts(i < 10 ? " : " : ": ");
        uart_print_hex64(frame->x[i]);
        uart_puts("    x");
        uart_print_dec((unsigned long long)(i + 1));
        uart_puts(i + 1 < 10 ? " : " : ": ");
        uart_print_hex64(frame->x[i + 1]);
        uart_newline();
    }
    print_reg("x29", frame->x[29]);   /* frame pointer */
    print_reg("x30", frame->x[30]);   /* link register */
    uart_newline();

    esr_decode(frame->esr);
    uart_newline();

    uart_puts("Kernel halted. No recovery possible at this stage."); uart_newline();
    uart_puts("=============================="); uart_newline();

    /* Control returns to _exc_halt in exceptions.S */
}
