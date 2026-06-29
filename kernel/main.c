#include "../drivers/uart.h"
#include "hw_detect.h"
#include "bench.h"

/*
 * x0 holds the DTB address at entry (Linux AArch64 boot protocol).
 * boot.S never touches x0, so it arrives here intact from QEMU.
 */

static hw_info_t    hw;
static bench_result_t bench;

void kernel_main(void *dtb_addr) {
    uart_puts("=============================="); uart_newline();
    uart_puts("       Welcome to AeonOS      "); uart_newline();
    uart_puts("  That which cannot be destroyed."); uart_newline();
    uart_puts("=============================="); uart_newline();

    hw_detect(&hw, dtb_addr);
    hw_print_report(&hw);

    bench_run(&hw, &bench);
    bench_print(&bench);

    uart_puts("Kernel initialized. Halting."); uart_newline();

    while (1) {}
}
