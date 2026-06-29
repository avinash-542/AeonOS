# AeonOS

> That which cannot be destroyed.

AeonOS is a custom operating system built from scratch targeting ARM AArch64 hardware — the same silicon that powers Android and iOS devices. Developed solo, no deadline, no limits.

## Status

**Phase 1 — Hardware Discovery.** The kernel boots, detects the CPU and its AI capabilities by reading ARM system registers, parses the Device Tree Blob for real memory size, runs an INT8 microbenchmark, and reports everything over UART before halting.

```
==============================
       Welcome to AeonOS
  That which cannot be destroyed.
==============================

==============================
    AeonOS Hardware Report
==============================

[CPU]
  Implementer : ARM
  Core        : Cortex-A57
  Variant/Rev : r1p1
  MIDR        : 0x411FD070...

[ISA — AI Capabilities]
  NEON        : YES
  NEON FP16   : NO
  FHM         : NO   (FP16 mul-accumulate)
  DOTPROD     : NO   (INT8 dot product)
  I8MM        : NO   (INT8 matrix multiply)
  BF16        : NO   (bfloat16)
  SVE         : NO
  SVE2        : NO

[Memory]
  RAM         : 128 MB

[AI Performance Estimate]
  Score       : 15 / 100
  INT8 TOPS   : ~0.5
  AI Tier     : MINIMAL   (basic inference only)

...

[INT8 Dot Product Benchmark]
  Elapsed     : 4200 us
  Measured    : 6.0 GOPS (INT8 MAC)
```

## Target

| Property | Value |
|---|---|
| Architecture | ARM AArch64 (64-bit) |
| Target hardware | Android-class devices (Snapdragon, Kirin, Exynos, Dimensity) |
| Base | Bare metal — no Linux, no BSD, no UEFI runtime |
| Dev machine | macOS (cross-compiled via Docker) |
| Test environment | QEMU `virt` machine |

## Quick Start

```bash
# Build the dev environment (first time only)
./tools/dev.sh

# Inside the container:
make          # compile → aeonos.img
make run      # launch in QEMU (Ctrl-A X to quit)
```

See [docs/build-guide.md](docs/build-guide.md) for full setup and GDB debugging.

## Repository Structure

```
AeonOS/
├── boot/
│   └── boot.S          ← ARM64 entry point, stack setup, BSS zero, → kernel_main
├── kernel/
│   ├── main.c          ← kernel_main: orchestrates boot, detect, bench, halt
│   ├── hw_detect.c/h   ← reads ARM system registers, scores AI capability
│   ├── dtb.c/h         ← parses Device Tree Blob for RAM size
│   └── bench.c/h       ← INT8 MAC microbenchmark via ARM generic timer
├── drivers/
│   └── uart.c/h        ← PL011 UART driver (QEMU 0x09000000)
├── lib/
│   ├── types.h         ← uint8_t / uint32_t / uint64_t (no libc)
│   └── endian.h        ← big-endian read helpers for FDT parsing
├── docs/               ← detailed documentation (start here)
├── tools/
│   └── dev.sh          ← Docker dev environment launcher
├── linker.ld           ← memory layout (kernel at 0x40000000)
├── Makefile
└── Dockerfile
```

## Documentation

| Document | Contents |
|---|---|
| [docs/architecture.md](docs/architecture.md) | Design philosophy, module map, roadmap |
| [docs/boot-sequence.md](docs/boot-sequence.md) | ARM64 boot protocol, boot.S walkthrough |
| [docs/hardware-detection.md](docs/hardware-detection.md) | System registers, AI ISA features, scoring |
| [docs/dtb-parsing.md](docs/dtb-parsing.md) | FDT binary format, memory node walkthrough |
| [docs/benchmarking.md](docs/benchmarking.md) | ARM generic timer, INT8 TOPS methodology |
| [docs/build-guide.md](docs/build-guide.md) | Build, QEMU, GDB, real device flashing |

## Commit Convention

See [docs/COMMITS.md](docs/COMMITS.md).
