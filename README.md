# AeonOS

> That which cannot be destroyed.

AeonOS is a futuristic, Apple-like operating system built from scratch,
targeting ARM (AArch64) hardware. Developed solo, no deadline, no limits.

## Status
Day 1. The journey begins.

## Architecture
- Target: ARM AArch64 (Android-class hardware)
- Built on: Bare metal (no Linux, no BSD — pure scratch)
- Dev machine: macOS (cross-compiled via aarch64-elf-gcc)
- Testing: QEMU

## Structure
```
AeonOS/
├── boot/        ← Bootloader & ARM entry point
├── kernel/      ← Core kernel code
├── drivers/     ← Hardware drivers
├── lib/         ← Shared libraries
├── tools/       ← Build & development tools
└── docs/        ← Documentation
```

## Building
Coming soon.
