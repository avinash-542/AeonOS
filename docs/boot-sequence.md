# Boot Sequence

## Overview

When QEMU launches with `-kernel aeonos.img`, it loads the binary into RAM at `0x40000000` and jumps to the first byte. That first byte is the `_start` label in `boot/boot.S`. From there, the ARM64 boot sequence runs in four steps before reaching C code:

```
QEMU firmware
    │
    │  loads aeonos.img → 0x40000000
    │  sets x0 = DTB address
    │  jumps to _start
    ▼
boot/boot.S  _start
    │  1. Park secondary CPU cores
    │  2. Set up stack pointer
    │  3. Zero BSS section
    │  4. bl kernel_main   ← x0 still = DTB address
    ▼
kernel/main.c  kernel_main(void *dtb_addr)
```

## The Linux AArch64 Boot Protocol

QEMU follows the Linux AArch64 boot protocol when using `-kernel`. At the moment of the jump to `_start`:

| Register | Value |
|---|---|
| `x0` | Physical address of the DTB (Flattened Device Tree blob) |
| `x1` | 0 (reserved) |
| `x2` | 0 (reserved) |
| `x3` | 0 (reserved) |

This is how QEMU communicates hardware information — RAM size, peripheral addresses, CPU topology — to the kernel. See [docs/dtb-parsing.md](dtb-parsing.md) for how we use it.

## boot/boot.S — Line by Line

```asm
.section ".text.boot"
.global _start
```

Places this code in the `.text.boot` section. The linker script places `.text.boot` first inside `.text`, so this is guaranteed to be at exactly `0x40000000` — the first instruction the CPU runs.

---

### Step 1: Park Secondary Cores

```asm
_start:
    mrs x1, mpidr_el1      // read Multiprocessor Affinity Register
    and x1, x1, #3         // isolate bits [1:0] = core ID (within cluster)
    cbz x1, 2f             // if core ID == 0, skip to label 2
1:  wfe                    // else: Wait For Event (low-power sleep)
    b 1b                   // loop forever
```

Modern ARM SoCs boot **all CPU cores simultaneously**. Only one core should run kernel initialization — otherwise we'd have multiple CPUs racing to clear BSS, set up the stack, and call `kernel_main`.

`MPIDR_EL1` is the Multiprocessor Affinity Register. Bits `[1:0]` give the core ID within a cluster. Core 0 continues; all others enter `WFE` (Wait For Event), a low-power halt that can be woken later with `SEV` once the OS is ready to bring them online.

On a real Android device with 8 cores (e.g., 1×X3 + 3×A715 + 4×A510), 7 cores park here at boot.

---

### Step 2: Set Up the Stack

```asm
2:
    ldr x1, =stack_top     // load address of stack_top symbol
    mov sp, x1             // set stack pointer
```

The C kernel needs a valid stack before it can call functions or use local variables. `stack_top` is defined later in the `.bss` section:

```asm
.section ".bss"
.align 16
stack_bottom:
    .skip 4096             // 4 KB stack
stack_top:
```

The stack grows downward in AArch64, so the stack pointer starts at `stack_top` (the high address) and grows toward `stack_bottom`.

4 KB is enough for Phase 1 (no recursion, no complex stack frames). This will grow when we add exception handlers and process stacks.

**Note:** `x0` is **not touched** in this step or any step above. The DTB address from QEMU is preserved.

---

### Step 3: Zero the BSS Section

```asm
    ldr x1, =__bss_start   // start of BSS (from linker script)
    ldr x2, =__bss_end     // end of BSS
3:  cmp x1, x2             // have we reached the end?
    b.ge 4f                // if x1 >= x2, done
    str xzr, [x1], #8      // store zero quad-word, advance x1 by 8 bytes
    b 3b                   // loop
```

The C standard guarantees that global variables with no explicit initializer start at zero. The ELF format handles this via the `.bss` section — it records the size but not the data (zero bytes don't need to be stored in the binary). At runtime, *someone* has to actually zero this memory. On Linux, the kernel does it for processes. Here, we are the kernel, so `boot.S` does it.

`__bss_start` and `__bss_end` are symbols defined by `linker.ld`:
```ld
. = ALIGN(8);
__bss_start = .;
.bss : { *(.bss) }
. = ALIGN(8);
__bss_end = .;
```

We write 8 bytes (one quad-word) per iteration using `xzr` (the always-zero register), advancing `x1` with post-increment addressing `[x1], #8`. This is the fastest way to zero memory without NEON or cache prefetching.

---

### Step 4: Jump to C Kernel

```asm
4:
    bl kernel_main         // branch-with-link (saves return address to LR)

hang:
    wfe                    // if kernel_main ever returns, sleep forever
    b hang
```

`bl` saves the return address in the Link Register (`x30`) and jumps to `kernel_main`. Since `kernel_main` never returns (it ends in `while(1) {}`), the `hang` label is just a safety net.

At this point, `x0` still holds the DTB address from QEMU. The AArch64 calling convention passes the first argument in `x0`, so `kernel_main(void *dtb_addr)` receives it automatically — no explicit register manipulation needed.

## Why BSS Zeroing Matters

Our `hw_detect.c` declares:
```c
static hw_info_t hw;
```

This is a global struct that lives in BSS. Before `boot.S` zeros BSS, the memory contains whatever QEMU happened to put there (typically zeros, but not guaranteed). If we called `hw_detect` without zeroing BSS first, `hw.has_dotprod` might appear to be `1` even on a machine that doesn't have DOTPROD — leading to wrong benchmark paths and wrong scores.

## Execution State

When `kernel_main` is entered, the CPU is in:

| Property | Value |
|---|---|
| Exception level | EL1 (kernel mode) |
| Endianness | Little-endian (SCTLR_EL1.EE = 0 by QEMU default) |
| MMU | **Off** (SCTLR_EL1.M = 0) |
| Caches | **Off** (SCTLR_EL1.C = 0, .I = 0) |
| NEON/FP | Enabled (CPACR_EL1.FPEN = 0b11 set by QEMU) |
| Interrupts | Enabled (PSTATE.I = 0) |

The MMU being off is critical: all addresses are physical addresses. This is fine for Phase 1 since we're just reading registers and writing to UART. Phase 2 will enable the MMU after setting up page tables.
