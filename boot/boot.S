.section ".text.boot"
.global _start

_start:
    // Only let CPU core 0 run, halt all others
    mrs x1, mpidr_el1
    and x1, x1, #3
    cbz x1, 2f
1:  wfe
    b 1b

2:
    // Set up stack pointer
    ldr x1, =stack_top
    mov sp, x1

    // Clear BSS section
    ldr x1, =__bss_start
    ldr x2, =__bss_end
3:  cmp x1, x2
    b.ge 4f
    str xzr, [x1], #8
    b 3b

4:
    // Jump into C kernel
    bl kernel_main

    // If kernel ever returns, hang forever
hang:
    wfe
    b hang

.section ".bss"
.align 16
stack_bottom:
    .skip 4096
stack_top: