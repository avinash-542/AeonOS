# Build Guide

## Prerequisites

The kernel is cross-compiled on macOS using a Linux toolchain inside Docker. You do not need to install anything directly on your Mac except Docker Desktop.

| Tool | Where it runs | Purpose |
|---|---|---|
| `gcc-aarch64-linux-gnu` | Inside Docker | Cross-compiler: macOS x86/ARM → AArch64 ELF |
| `aarch64-linux-gnu-objcopy` | Inside Docker | Strip ELF → raw binary `.img` |
| `qemu-system-aarch64` | Inside Docker | ARM64 virtual machine for testing |
| `gdb-multiarch` | Inside Docker | Source-level debugging over QEMU's GDB stub |

## Development Environment

### First Time Setup

```bash
# From the repo root on your Mac:
./tools/dev.sh
```

This builds the Docker image (once) and drops you into an interactive shell inside the container. The repo root is mounted at `/aeonos`, so any edits you make on your Mac are immediately visible inside the container.

What the Dockerfile installs:
```dockerfile
apt-get install -y \
    gcc-aarch64-linux-gnu      # AArch64 cross-compiler
    binutils-aarch64-linux-gnu # objcopy, objdump, readelf
    qemu-system-arm            # QEMU ARM64 emulator
    make                       # build system
    gdb-multiarch              # cross-architecture debugger
    xxd                        # hex dump utility
    git                        # version control
```

### Subsequent Sessions

```bash
./tools/dev.sh     # reopens the container with your latest changes
```

## Building

Inside the container:

```bash
make          # compile and link → aeonos.elf, then strip → aeonos.img
make clean    # remove aeonos.elf and aeonos.img
```

### What the Build Does

```
Step 1: Compile + link
  aarch64-linux-gnu-gcc \
    -ffreestanding -nostdinc -nostdlib -nostartfiles -O2 -I. \
    -T linker.ld \
    -o aeonos.elf \
    boot/boot.S kernel/main.c kernel/hw_detect.c \
    kernel/dtb.c kernel/bench.c drivers/uart.c

Step 2: Strip ELF headers → raw binary
  aarch64-linux-gnu-objcopy -O binary aeonos.elf aeonos.img
```

**Why raw binary?** QEMU's `-kernel` flag expects a raw binary (or a Linux Image header). The ELF format contains metadata and section headers that QEMU does not need and would interpret as executable code if encountered at the load address.

**Compiler flags explained:**

| Flag | Purpose |
|---|---|
| `-ffreestanding` | Disable standard library assumptions |
| `-nostdinc` | Do not search system include paths |
| `-nostdlib` | Do not link against any standard library |
| `-nostartfiles` | Do not link `crt0.o` / startup files |
| `-O2` | Optimize: important for tight benchmark loops |
| `-I.` | Add repo root to include search path (for `lib/`, `drivers/`) |
| `-T linker.ld` | Use our custom memory layout |

### Linker Script Summary

`linker.ld` places the kernel at `0x40000000` (QEMU's RAM base for the `virt` machine):

```ld
ENTRY(_start)

SECTIONS {
    . = 0x40000000;

    .text : {
        *(.text.boot)    ← boot.S _start goes first
        *(.text)         ← all other code
    }
    .data   : { *(.data) }
    .rodata : { *(.rodata) }

    . = ALIGN(8);
    __bss_start = .;
    .bss : { *(.bss) }
    . = ALIGN(8);
    __bss_end = .;
}
```

`*(.text.boot)` is placed before `*(.text)` to guarantee `_start` is the first instruction at `0x40000000`. If this order were reversed, the CPU would jump into the middle of some C function at boot — instant crash.

## Running in QEMU

```bash
make run
```

This executes:
```bash
qemu-system-aarch64 \
    -M virt \
    -cpu cortex-a57 \
    -nographic \
    -kernel aeonos.img
```

| Flag | Effect |
|---|---|
| `-M virt` | Generic virtual ARM64 machine with PL011 UART, GIC |
| `-cpu cortex-a57` | Emulate a Cortex-A57 (ARMv8.0, no AI extensions) |
| `-nographic` | Redirect UART output to your terminal (no window) |
| `-kernel aeonos.img` | Load the binary at 0x40000000, set x0 = DTB address |

**To quit QEMU:** Press `Ctrl-A` then `X`.

### Testing with More RAM

```bash
qemu-system-aarch64 -M virt -cpu cortex-a57 -m 512m -nographic -kernel aeonos.img
```

The `-m 512m` flag changes QEMU's RAM to 512 MB. The DTB parser will detect this automatically and the hardware report will show `RAM: 512 MB` instead of `128 MB`.

### Testing a Modern CPU Profile

To simulate a CPU with DOTPROD + I8MM + BF16 (similar to a Cortex-A76 generation device):

```bash
qemu-system-aarch64 -M virt \
    -cpu cortex-a76 \
    -nographic -kernel aeonos.img
```

This will change the ISA capabilities shown in the hardware report. Note: not all QEMU CPU models accurately emulate all extension bits — verify with the raw `ISAR` registers printed in the report.

## Debugging with GDB

QEMU has a built-in GDB stub. In one terminal:

```bash
qemu-system-aarch64 -M virt -cpu cortex-a57 -nographic \
    -kernel aeonos.img -s -S
```

`-s` opens a GDB server on port 1234. `-S` freezes the CPU at startup (waits for GDB to connect before executing).

In a second terminal inside the container:

```bash
gdb-multiarch aeonos.elf
```

Inside GDB:
```
(gdb) target remote :1234       # connect to QEMU
(gdb) layout src                # show source code
(gdb) b kernel_main             # breakpoint at kernel entry
(gdb) b hw_detect               # breakpoint before register reads
(gdb) c                         # run until breakpoint
(gdb) info registers            # dump all registers
(gdb) x/16xw 0x40000000        # examine 16 words at kernel base
(gdb) p hw                      # print hw_info_t struct (if at main)
```

The `.elf` file contains DWARF debug symbols so GDB can show source lines, variable values, and function names.

## Inspecting the Binary

```bash
# Disassemble boot entry
aarch64-linux-gnu-objdump -d aeonos.elf | head -60

# Show section sizes
aarch64-linux-gnu-size aeonos.elf

# Hex dump of first 64 bytes (should be boot.S instructions)
xxd aeonos.img | head -4

# Show all symbols and their addresses
aarch64-linux-gnu-nm -n aeonos.elf
```

## Flashing to Real Hardware (Future)

AeonOS is not yet ready for real hardware — the MMU, exception vectors, and hardware abstraction layer are needed first. When that time comes, the process will depend on the device:

**Android devices (unlocked bootloader):**
```bash
# Boot from fastboot (non-persistent, for testing)
fastboot boot aeonos.img

# Flash to boot partition (persistent)
fastboot flash boot aeonos.img
```

**UART output on real Android:**  
Real Android devices typically route debug UART through the headphone jack or a dedicated debug connector. Output will appear using the same UART driver since most Qualcomm/MediaTek/HiSilicon SoCs map the debug UART to the same PL011-compatible interface (though at a different base address read from the DTB).

## Source Tree After Phase 1

```
AeonOS/
├── boot/
│   └── boot.S              1.2 KB   ARM64 entry point
├── kernel/
│   ├── main.c              0.5 KB   Entry, orchestration
│   ├── hw_detect.c         6.5 KB   ARM register reads + report
│   ├── hw_detect.h         2.5 KB
│   ├── dtb.c               3.5 KB   FDT parser
│   ├── dtb.h               0.3 KB
│   ├── bench.c             4.0 KB   INT8 benchmark
│   └── bench.h             0.5 KB
├── drivers/
│   ├── uart.c              0.8 KB   PL011 UART
│   └── uart.h              0.3 KB
├── lib/
│   ├── types.h             0.4 KB   uint8/32/64_t
│   └── endian.h            0.4 KB   be32/be64 reads
├── docs/
│   ├── architecture.md
│   ├── boot-sequence.md
│   ├── hardware-detection.md
│   ├── dtb-parsing.md
│   ├── benchmarking.md
│   └── build-guide.md      ← this file
├── linker.ld               0.4 KB
├── Makefile                0.5 KB
├── Dockerfile              0.4 KB
└── tools/
    └── dev.sh              0.2 KB
```

Compiled binary: `aeonos.img` — approximately 8–16 KB depending on optimization. Small enough to flash to anything.
