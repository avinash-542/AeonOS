# Benchmarking

## Goal

Phase 1 benchmarking has one objective: produce a **measured** INT8 throughput number instead of a purely estimated one. The estimate in the hardware report (based on ISA features alone) is useful for comparison, but actual silicon performance depends on microarchitectural details — execution unit count, pipeline depth, out-of-order depth, cache latency — that register reads cannot reveal.

## ARM Generic Timer

AArch64 provides a high-resolution, CPU-frequency-independent timer as part of the base ISA.

| Register | Description |
|---|---|
| `CNTFRQ_EL0` | Timer frequency in Hz (set by firmware at boot) |
| `CNTPCT_EL0` | Current physical count value (increases at `CNTFRQ` rate) |

These are available at EL0 and EL1 without any special setup. QEMU's `virt` machine initializes `CNTFRQ_EL0` automatically.

```c
static inline uint64_t timer_freq(void) {
    uint64_t f;
    asm volatile("mrs %0, cntfrq_el0" : "=r"(f));
    return f;
}

static inline uint64_t timer_count(void) {
    uint64_t c;
    asm volatile("isb\n\t"
                 "mrs %0, cntpct_el0" : "=r"(c) :: "memory");
    return c;
}
```

### Why `ISB` Before the Timer Read?

ARM CPUs execute instructions out-of-order. Without a barrier, the CPU might speculatively execute the `MRS CNTPCT_EL0` before the benchmark loop finishes — producing a falsely short elapsed time.

`ISB` (Instruction Synchronization Barrier) is a context-synchronizing operation: it flushes the CPU pipeline and ensures all preceding instructions complete before `MRS` executes. This is the ARM-recommended technique for performance measurement.

The `"memory"` clobber in the `asm` constraint additionally prevents the C compiler from reordering the timer read across surrounding C code.

### CNTFRQ on QEMU vs Real Hardware

| Platform | Typical CNTFRQ |
|---|---|
| QEMU `virt` | 62,500,000 Hz (62.5 MHz) |
| Snapdragon 8xx | 19,200,000 Hz (19.2 MHz) |
| Apple A-series | 24,000,000 Hz (24 MHz) |
| Kirin 9000 | 19,200,000 Hz |

The frequency varies by platform, which is why we always use `CNTFRQ_EL0` to convert ticks → seconds rather than assuming a fixed value.

## Benchmark Kernel

### What We Measure

An INT8 dot product: the inner loop of matrix multiplication and the dominant operation in modern neural network inference (both attention and feed-forward layers, when quantized to INT8 or INT4).

```
for iter in 1..BENCH_ITERS:
    for i in 0..VEC_LEN:
        acc += a[i] * b[i]    ← 1 multiply + 1 add = 1 MAC
```

- `VEC_LEN = 128` — one 128-element dot product per outer iteration
- `BENCH_ITERS = 100,000` — 100K outer iterations
- `total_macs = 100,000 × 128 × 2 = 25,600,000` MAC operations

### Preventing Dead Code Elimination

A sufficiently smart compiler would notice that `a[]` and `b[]` are constant across all `BENCH_ITERS` iterations and could hoist the entire inner loop, computing the result once and multiplying by `BENCH_ITERS`. This would produce a near-zero elapsed time — completely useless.

Two techniques prevent this:

1. **Global sink variable** — the accumulator result is written to a `volatile` global (`bench_sink`) after the loop. This forces the compiler to assume the result is externally observable, so it cannot eliminate the loop.

2. **Non-trivial initialization** — the arrays are filled with values that depend on the loop index (`(i * 7 + 3) & 0x7F`), not a compile-time constant, so the compiler cannot reduce the dot product to a constant at compile time.

### ISA Path Selection

The current benchmark uses the **scalar C path** — plain C `int32_t` accumulation that the compiler vectorizes using whatever NEON instructions it considers safe for the generic AArch64 target.

With `-O2 -ffreestanding` and no `-march` extension flags, GCC will emit `SMLAL`/`MLA` NEON instructions for the inner loop on AArch64, but will not emit `SDOT` (DOTPROD) or `SMMLA` (I8MM) without explicit enablement.

**Future ISA-native paths** (when the Makefile gains per-file march flags):

| ISA Extension | Instruction | MACs/Instruction | Notes |
|---|---|---|---|
| Baseline NEON | SMLAL.8H | 4 (8-bit pairs → 16-bit) | Available everywhere |
| DOTPROD | SDOT V0.4S, V1.16B, V2.16B | 16 | Requires `-march=+dotprod` |
| I8MM | SMMLA V0.4S, V1.16B, V2.16B | 32 | Requires `-march=+i8mm` |
| SVE + DOTPROD | SDOT Z0.S, Z1.B, Z2.B | 16 × (VL/128) | Scales with vector width |

Adding ISA-native benchmark paths is a Phase 2 task. It requires either separate compilation units with per-file march flags, or `__attribute__((target(...)))` function attributes.

## Throughput Calculation

```
elapsed_ticks = CNTPCT_after - CNTPCT_before

elapsed_seconds = elapsed_ticks / CNTFRQ

GOPS = total_macs / elapsed_seconds / 1,000,000,000
     = total_macs × CNTFRQ / (elapsed_ticks × 1,000,000,000)
```

To avoid floating point (we have no FPU-backed division in freestanding mode) and prevent integer overflow, the implementation divides `CNTFRQ` by 1,000 first:

```c
uint64_t freq_khz  = cntfrq / 1000;
uint64_t numerator = total_macs * 10 * freq_khz;
uint64_t denominator = elapsed_ticks * 1_000_000;
gops_x10 = numerator / denominator;
```

`gops_x10` is GOPS × 10, so `gops_x10 = 25` means 2.5 GOPS. Reported as:
```c
uart_print_dec(gops_x10 / 10);
uart_puts(".");
uart_print_dec(gops_x10 % 10);
uart_puts(" GOPS");
```

### Overflow Safety

Worst-case intermediate value: `total_macs * 10 * freq_khz`
- `total_macs = 25,600,000`
- `* 10 = 256,000,000`
- `* freq_khz` at 1 GHz timer → `freq_khz = 1,000,000`
- Product = `256,000,000,000,000,000` ≈ 2.56 × 10¹⁷

`uint64_t` maximum ≈ 1.84 × 10¹⁹ → safe with headroom.

## Expected Results

### QEMU (Cortex-A57 emulation)

QEMU executes instructions by software translation. Throughput is determined by QEMU's translator speed, not the host CPU's ARM performance.

| Metric | Typical QEMU Value |
|---|---|
| CNTFRQ | 62,500,000 Hz |
| Elapsed | 4,000–15,000 µs |
| Measured GOPS | 2–8 GOPS (emulation artifact) |

These numbers are **not representative of real hardware**. They reflect how fast QEMU can translate AArch64 instructions to native x86 on your Mac, not how fast an actual ARM core would run the benchmark.

### Real Hardware (approximate, scalar C path)

| SoC | Core | Est. GOPS (scalar INT8) |
|---|---|---|
| Snapdragon 855 | Cortex-A76 | 25–40 GOPS |
| Snapdragon 8 Gen 1 | Cortex-X2 | 40–60 GOPS |
| Snapdragon 8 Gen 2 | Cortex-X3 | 50–80 GOPS |
| Kirin 9000 | Cortex-A77 | 20–35 GOPS |

With DOTPROD-enabled paths, throughput typically 3–5× higher. With I8MM, 6–10× higher.

## What Comes After

The benchmark module is designed to grow. Planned additions:

1. **ISA-native paths** — compile `bench_dotprod.c` with `+dotprod` and `bench_i8mm.c` with `+i8mm`, linked into the kernel. Run the best available path and report separately.

2. **Multi-iteration warmup** — run one warmup pass before measurement to bring instruction caches and branch predictors to steady state.

3. **NPU probe** — on real Android hardware, probe the NPU MMIO region (Hexagon DSP for Qualcomm, Kirin NPU for HiSilicon). Report NPU TOPS separately.

4. **Memory bandwidth** — measure LPDDR5/LPDDR4X bandwidth (sequential read/write), which is often the bottleneck for large model inference.

5. **End-to-end model benchmark** — once the OS can load data from storage, run a quantized transformer block and measure end-to-end token throughput.
