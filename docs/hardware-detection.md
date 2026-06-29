# Hardware Detection

## Why Hardware Detection First?

Before the OS does anything else — schedules processes, mounts filesystems, starts services — it needs to know what hardware it is running on. AeonOS puts hardware detection at the very first step of `kernel_main` because every subsequent decision depends on it: which AI runtime to load, how to configure the memory allocator, whether to enable SVE kernel paths, how many cores to bring online.

The hardware report is also the primary debugging output during early OS development. If something is wrong with the kernel, the last thing it prints before crashing tells you exactly what CPU and memory you were running on.

## What We Detect

```
kernel/hw_detect.c
│
├── CPU Identity (MIDR_EL1)
│     implementer, part number, variant, revision
│
├── AI ISA Features
│     ISAR0: DOTPROD (INT8 dot product), FHM (FP16 multiply-accumulate)
│     ISAR1: I8MM (INT8 matrix multiply), BF16 (bfloat16)
│     PFR0:  NEON baseline, NEON FP16, SVE presence
│     ZCR:   SVE vector length (only if SVE present)
│     ZFR0:  SVE2 (only if SVE present)
│
├── Memory (via DTB — see dtb-parsing.md)
│     RAM in MB from /memory node
│
└── AI Score + TOPS estimate
      composite 0-100 score and INT8 TOPS extrapolation
```

## ARM System Registers

All detection is done by reading ARM system registers using `MRS` (Move Register from System register) instructions. No MMIO, no platform-specific probing — these registers are part of the ARMv8-A ISA and exist on every compliant implementation.

```c
static inline uint64_t read_midr(void) {
    uint64_t v;
    asm volatile("mrs %0, midr_el1" : "=r"(v));
    return v;
}
```

### MIDR_EL1 — Main ID Register

Identifies the CPU core. All fields are read-only and set by the implementer.

```
Bits [31:24] Implementer  — who made this core
Bits [23:20] Variant      — major revision (rN)
Bits [19:16] Architecture — 0xF = ARMv8 CPUID scheme
Bits [15:4]  PartNum      — which core within the implementer's lineup
Bits [3:0]   Revision     — minor revision (pN)
```

**Implementer codes** (non-exhaustive):

| Code | Vendor |
|---|---|
| `0x41` | ARM Holdings |
| `0x51` | Qualcomm (Snapdragon) |
| `0x61` | Apple |
| `0x48` | HiSilicon (Kirin) |
| `0x53` | Samsung (Exynos) |
| `0x4D` | MediaTek (Dimensity) |

**ARM PartNum codes** (selection):

| PartNum | Core | Generation | AI Extensions |
|---|---|---|---|
| `0xD03` | Cortex-A53 | ARMv8.0 | None |
| `0xD05` | Cortex-A55 | ARMv8.2 | DOTPROD |
| `0xD07` | Cortex-A57 | ARMv8.0 | None |
| `0xD0B` | Cortex-A76 | ARMv8.2 | DOTPROD |
| `0xD41` | Cortex-A78 | ARMv8.2 | DOTPROD, I8MM, BF16 |
| `0xD44` | Cortex-X1  | ARMv8.2 | DOTPROD, I8MM, BF16 |
| `0xD47` | Cortex-A710| ARMv9.0 | DOTPROD, I8MM, BF16, SVE |
| `0xD48` | Cortex-X2  | ARMv9.0 | DOTPROD, I8MM, BF16, SVE |
| `0xD4E` | Cortex-X3  | ARMv9.0 | DOTPROD, I8MM, BF16, SVE2 |
| `0xD82` | Cortex-X4  | ARMv9.2 | DOTPROD, I8MM, BF16, SVE2 |

---

### ID_AA64ISAR0_EL1 — ISA Feature Register 0

Reports which instruction set extensions are implemented.

```
Bits [63:60] RNDR  — random number (RNDR, RNDRRS)
Bits [59:56] TLB   — TLB range maintenance
Bits [55:52] TS    — flag manipulation (SETF8, SETF16)
Bits [51:48] FHM   — FP16 multiply-accumulate (FMLAL, FMLSL)
Bits [47:44] DP    — INT8 dot product (UDOT, SDOT)      ← AI-critical
Bits [43:40] SM4   — SM4 block cipher
Bits [39:36] SM3   — SM3 hash
Bits [35:32] SHA3  — SHA-3
Bits [31:28] RDM   — rounding double multiply (SQRDMLAH)
Bits [27:24] ATOMICS — large system atomics
Bits [23:20] CRC32
Bits [19:16] SHA2
Bits [15:12] SHA1
Bits [11:8]  AES
```

**DP field (DOTPROD):** Value `0b0001` means `UDOT` and `SDOT` instructions are present.

`SDOT V0.4S, V1.16B, V2.16B` — performs 4 parallel INT8 dot products, each consuming 4 elements. One instruction = 16 multiply-accumulate operations. On a 3 GHz CPU with 2 NEON execution units: **~96 billion INT8 MACs per second** from this instruction alone.

**FHM field:** Value `0b0001` means `FMLAL`/`FMLSL` are present — FP16 multiply-accumulate used in mixed-precision training.

---

### ID_AA64ISAR1_EL1 — ISA Feature Register 1

```
Bits [63:60] LS64   — 64-byte atomics
Bits [59:56] XS     — XS attribute
Bits [55:52] I8MM   — INT8 matrix multiply (SMMLA, UMMLA, USMMLA) ← AI-critical
Bits [51:48] DGH    — Data Gather Hint
Bits [47:44] BF16   — BFloat16 (BFDOT, BFMMLA, BFMLAL)           ← AI-critical
Bits [43:40] SPECRES — speculation barrier
Bits [39:36] SB     — speculation barrier
Bits [35:32] FRINTTS — FRINT32X, FRINT64X
Bits [31:28] GPI    — generic auth (QARMA3)
Bits [27:24] GPA    — generic auth (QARMA5)
Bits [23:20] LRCPC  — load-acquire RCpc
Bits [19:16] FCMA   — complex number multiplication
Bits [15:12] JSCVT  — JavaScript FP→int conversion
Bits [11:8]  API    — pointer auth (QARMA3)
Bits [7:4]   APA    — pointer auth (QARMA5)
Bits [3:0]   DPB    — DC CVAP instruction
```

**I8MM field:** Value `0b0001` means `SMMLA` is present.

`SMMLA V0.4S, V1.16B, V2.16B` — signed 8×8 INT8 matrix multiply-accumulate. Computes a 4×4 outer product from two 4×4 INT8 matrices in a single instruction: **32 MACs per instruction**. This is the most important instruction for LLM inference on CPU.

**BF16 field:** Value `0b0001` means BFloat16 instructions are present. BFloat16 uses the same exponent range as FP32 but with fewer mantissa bits — this makes it numerically stable for ML training while halving memory bandwidth requirements. `BFMMLA` does BF16 matrix multiply-accumulate.

---

### ID_AA64PFR0_EL1 — Processor Feature Register 0

```
Bits [63:60] CSV3     — cache speculation vulnerability
Bits [59:56] CSV2     — branch target injection
Bits [55:52] RME      — Realm Management Extension
Bits [51:48] DIT      — Data Independent Timing
Bits [47:44] AMU      — Activity Monitor Unit
Bits [43:40] MPAM     — Memory Partitioning and Monitoring
Bits [39:36] SEL2     — Secure EL2
Bits [35:32] SVE      — Scalable Vector Extension              ← AI-critical
Bits [31:28] RAS      — Reliability, Availability, Serviceability
Bits [27:24] GIC      — GIC system registers
Bits [23:20] AdvSIMD  — Advanced SIMD (NEON)                  ← AI-critical
Bits [19:16] FP       — Floating point
Bits [15:12] EL3      — EL3 implemented
Bits [11:8]  EL2      — EL2 implemented
Bits [7:4]   EL1      — EL1 implemented
Bits [3:0]   EL0      — EL0 supported
```

**SVE field:** Value `0b0001` means SVE is present. SVE (Scalable Vector Extension) is ARM's answer to x86 AVX-512 — variable-length SIMD vectors from 128 to 2048 bits. Unlike NEON (fixed 128-bit), SVE code runs on any vector width without recompilation.

**AdvSIMD field:**
- `0b1111` — NEON not implemented
- `0b0000` — NEON present, no FP16
- `0b0001` — NEON present with FP16 arithmetic

---

### ZCR_EL1 — SVE Control Register (EL1)

Only read when `ID_AA64PFR0_EL1.SVE ≥ 1`.

```
Bits [3:0] LEN — SVE vector length: VL = (LEN + 1) × 128 bits
```

At reset, LEN = 0 → VL = 128 bits (minimum). Firmware or hypervisor may set it higher. Reading ZCR_EL1 when SVE is not implemented causes an UNDEFINED exception — which is fatal without exception vectors. We guard the read with the PFR0 check.

---

### ID_AA64ZFR0_EL1 — SVE Feature Register 0

Only exists when SVE is implemented.

```
Bits [3:0] SVEver — 0b0001 = SVE2 present
```

SVE2 adds structured gather/scatter, complex number support, and additional predicate instructions — all useful for signal processing and ML workloads.

---

## AI Score

The score is a composite 0–100 integer intended to quickly communicate how capable a device is for on-device AI relative to other AArch64 hardware.

| Feature | Points | Rationale |
|---|---|---|
| Base (any AArch64) | 10 | Can run inference at all |
| NEON baseline | 5 | 128-bit SIMD, 8 INT8/cycle baseline |
| NEON FP16 | 5 | FP16 SIMD, important for embedding layers |
| FHM | 5 | Mixed-precision attention (FP16 × FP16 → FP32) |
| DOTPROD | 15 | ~4× INT8 throughput vs scalar NEON |
| I8MM | 25 | ~8× INT8 throughput; largest single jump |
| BF16 | 15 | Halves bandwidth for activation tensors |
| SVE present | 8 | Variable-width vectors; adapts to VL |
| SVE ≥ 256-bit | 4 | 2× throughput over 128-bit SVE |
| SVE ≥ 512-bit | 4 | 4× throughput over 128-bit SVE |
| SVE2 | 4 | Structured scatter/gather |
| RAM ≥ 2 GB | 2 | Can hold 1B param quantized model |
| RAM ≥ 4 GB | 2 | Can hold 3B param quantized model |
| RAM ≥ 8 GB | 2 | Can hold 7B param quantized model |
| RAM ≥ 12 GB | 1 | Can hold 13B param quantized model |

### AI Tier Classification

| Score | Tier | Practical Capability |
|---|---|---|
| 0–19 | MINIMAL | Basic ops only — keyword detection, tiny classifiers |
| 20–39 | BASIC | Models < 500M params — MobileNet, DistilBERT |
| 40–59 | CAPABLE | 1–3B param models — Phi-2, LLaMA 7B with 4-bit quant |
| 60–79 | STRONG | 7B models quantized to INT4 — Mistral 7B, LLaMA 2 7B |
| 80+ | AI-NATIVE | Frontier on-device — 13B+ or real-time multimodal |

### Expected Scores for Real Devices

| Device | SoC | Key AI ISA | Score |
|---|---|---|---|
| Older Android (2019) | Snapdragon 855 | DOTPROD | 35–40 |
| Mid-range (2022) | Snapdragon 778G | DOTPROD | 35–40 |
| Flagship (2022) | Snapdragon 8 Gen 1 | DOTPROD, I8MM, BF16 | 65–70 |
| Flagship (2023) | Snapdragon 8 Gen 2 | +SVE | 75–80 |
| Flagship (2024) | Snapdragon 8 Gen 3 | +SVE2 | 80–85 |
| QEMU Cortex-A57 | — | NEON only | 15 |

Note: These scores reflect CPU ISA only. On real devices, the NPU (Hexagon, Kirin NPU, etc.) would contribute significant additional TOPS that this Phase 1 detector does not yet measure.

## Output Example

```
[CPU]
  Implementer : Qualcomm
  Core        : Qualcomm Kryo
  Variant/Rev : r13p0
  MIDR        : 0x518FD8C0...

[ISA — AI Capabilities]
  NEON        : YES
  NEON FP16   : YES
  FHM         : YES  (FP16 mul-accumulate)
  DOTPROD     : YES  (INT8 dot product)
  I8MM        : YES  (INT8 matrix multiply)
  BF16        : YES  (bfloat16)
  SVE         : YES  (128-bit vectors)
  SVE2        : YES

[Memory]
  RAM         : 8192 MB

[AI Performance Estimate]
  Score       : 82 / 100
  INT8 TOPS   : ~12.0
  AI Tier     : AI-NATIVE (frontier on-device)
```
