#ifndef UART_H
#define UART_H

void uart_putc(char c);
void uart_puts(const char *s);
void uart_newline(void);
void uart_sep(void);
void uart_print_dec(unsigned long long val);
void uart_print_hex64(unsigned long long val);

#endif
