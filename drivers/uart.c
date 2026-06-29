#include "uart.h"

#define UART0_BASE 0x09000000

void uart_putc(char c) {
    volatile unsigned int *uart = (volatile unsigned int *)UART0_BASE;
    *uart = (unsigned int)c;
}

void uart_puts(const char *s) {
    while (*s) uart_putc(*s++);
}

void uart_newline(void) {
    uart_putc('\r');
    uart_putc('\n');
}

void uart_sep(void) {
    uart_puts("------------------------------");
    uart_newline();
}

void uart_print_dec(unsigned long long val) {
    if (val == 0) {
        uart_putc('0');
        return;
    }
    char buf[20];
    int len = 0;
    while (val > 0) {
        buf[len++] = '0' + (int)(val % 10);
        val /= 10;
    }
    while (len > 0) uart_putc(buf[--len]);
}

void uart_print_hex64(unsigned long long val) {
    static const char hex[] = "0123456789ABCDEF";
    uart_puts("0x");
    for (int i = 60; i >= 0; i -= 4)
        uart_putc(hex[(val >> i) & 0xF]);
}
