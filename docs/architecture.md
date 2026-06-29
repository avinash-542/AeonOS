# AeonOS Architecture

## Goal

Build a complete operating system from scratch that can be installed on consumer Android-class ARM hardware, with a first-class focus on on-device AI performance.

The word "from scratch" is taken literally: no Linux kernel, no BSD, no UEFI runtime services, no libc. Every subsystem — memory management, scheduling, drivers, filesystem — will be written specifically for this OS.

## Target Hardware

AeonOS targets ARM AArch64 (ARMv8-A and later) — the architecture used in every modern Android phone, most embedded AI devices, and Apple Silicon.

Concrete targets:

| SoC Family | Examples | AI Extensions |
|---|---|---|
| Snapdragon 8xx | 8 Gen 1/2/3 | DOTPROD, I8MM, BF16, SVE |
| Kirin 9xx | Kirin 9000 | DOTPROD, I8MM |
| Exynos 21xx/22xx | Galaxy S21/S22 | DOTPROD, I8MM |
| Dimensity 9xxx | MT9300 | DOTPROD, I8MM, SVE |
| Apple A/M series | A15, A16, M1, M2 | AMX (Apple Matrix), NEON |

During development, QEMU's `virt` machine (Cortex-A57) is used for testing.

## Design Philosophy

**No dependencies.** Every piece of code in this repository either runs on bare metal or is a thin abstraction of hardware. This is a deliberate constraint — it keeps the OS portable, auditable, and fast.

**AI-first.** The hardware detection and benchmarking system is the very first thing the kernel does on boot. This is intentional: before we can schedule processes or mount filesystems, we need to know what compute resources are available, so the OS can make intelligent decisions about how to allocate them.

**Freestanding C.** All kernel code is compiled with:
```
-ffreestanding -nostdinc -nostdlib -nostartfiles
```
No standard headers. No runtime library. `lib/types.h` provides `uint8_t` through `uint64_t`. Everything else is built from primitives.

## Module Map

```
┌─────────────────────────────────────────────────────────────┐
│                         kernel/main.c                       │
│   kernel_main(dtb_addr)  ←── x0 from boot.S (QEMU DTB)     │
└──────────┬──────────────────────────────┬───────────────────┘
           │                              │
           ▼                              ▼
┌─────────────────────┐      ┌────────────────────────┐
│  kernel/hw_detect.c │      │    kernel/bench.c      │
│                     │      │                        │
│  Reads ARM system   │      │  INT8 MAC loop timed   │
│  registers:         │      │  via ARM generic timer │
│  · MIDR_EL1         │      │  (CNTPCT_EL0 / CNTFRQ) │
│  · ID_AA64ISAR0/1   │      │                        │
│  · ID_AA64PFR0      │      │  Reports GOPS          │
│  · ZCR_EL1          │      └────────────────────────┘
│                     │
│  Also calls:        │
│  kernel/dtb.c       │
│  └─ parses FDT blob │
│     for RAM size    │
└─────────────────────┘
           │
           ▼
┌─────────────────────┐
│   drivers/uart.c    │
│                     │
│  PL011 UART at      │
│  0x09000000 (QEMU)  │
│  Decimal + hex      │
│  printing           │
└─────────────────────┘

Support libraries:
  lib/types.h    — uint8_t .. uint64_t
  lib/endian.h   — be32_read / be64_read (for FDT)
```

## Memory Layout

Defined by `linker.ld`. The kernel loads at `0x40000000`, which is where QEMU's `virt` machine maps RAM.

```
0x09000000  PL011 UART (MMIO)
0x40000000  .text.boot  ← _start (ARM64 entry, first instruction)
            .text       ← C kernel code
            .data       ← initialized globals
            .rodata     ← string literals
            .bss        ← zero-initialized globals (zeroed by boot.S)
0x48000000  End of default 128 MB QEMU RAM
```

On a real Android device the RAM base is typically still `0x40000000` (or wherever the bootloader places the OS), but the size will be 4–16 GB. The DTB parser reads this at runtime.

## Dependency Graph

```
main.c
  ├── hw_detect.h  →  hw_detect.c
  │     └── dtb.h  →  dtb.c
  │           └── lib/endian.h  →  lib/types.h
  ├── bench.h      →  bench.c
  └── drivers/uart.h  →  drivers/uart.c
```

No circular dependencies. Each layer only calls downward.

## Roadmap

The project is built in phases. Hardware detection (Phase 1) is complete.

| Phase | Goal | Status |
|---|---|---|
| 1 | Boot + hardware detection + AI benchmark | **Done** |
| 2 | Exception vectors + MMU (identity map) | Next |
| 3 | Physical memory manager (buddy allocator) | — |
| 4 | Virtual memory (page tables, kernel heap) | — |
| 5 | Device drivers (interrupt controller, clock, GPIO) | — |
| 6 | Process model + scheduler | — |
| 7 | Filesystem (read-only initrd first) | — |
| 8 | System call interface | — |
| 9 | AI runtime integration | — |
| 10 | Android hardware abstraction layer | — |

Phase 2 is the most critical architectural decision: once the MMU is enabled, every subsequent subsystem depends on correct virtual memory. We will identity-map the kernel first, then add higher-half mapping.
