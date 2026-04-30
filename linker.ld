ENTRY(_start)

SECTIONS {
    . = 0x40000000;

    .text : {
        *(.text.boot)
        *(.text)
    }

    .data : { *(.data) }

    .rodata : { *(.rodata) }

    . = ALIGN(8);
    __bss_start = .;
    .bss : { *(.bss) }
    . = ALIGN(8);
    __bss_end = .;
}