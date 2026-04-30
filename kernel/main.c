#define UART0_BASE 0x09000000

static inline void uart_putc(char c) {
    volatile unsigned int *uart = (unsigned int *)UART0_BASE;
    *uart = (unsigned int)c;
}

void uart_puts(const char *s) {
    while (*s) uart_putc(*s++);
}

void uart_put_newline(void) {
    uart_putc('\r');
    uart_putc('\n');
}

void kernel_main(void) {
    uart_puts("==============================");
    uart_put_newline();
    uart_puts("       Welcome to AeonOS      ");
    uart_put_newline();
    uart_puts("  That which cannot be destroyed.");
    uart_put_newline();
    uart_puts("==============================");
    uart_put_newline();

    while (1) {}
}