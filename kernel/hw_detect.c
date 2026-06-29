#include "hw_detect.h"
#include "dtb.h"
#include "../drivers/uart.h"

/* ── ARM system register reads ─────────────────────────────────────────── */

static inline uint64_t read_midr(void) {
    uint64_t v; asm volatile("mrs %0, midr_el1"          : "=r"(v)); return v;
}
static inline uint64_t read_isar0(void) {
    uint64_t v; asm volatile("mrs %0, id_aa64isar0_el1"  : "=r"(v)); return v;
}
static inline uint64_t read_isar1(void) {
    uint64_t v; asm volatile("mrs %0, id_aa64isar1_el1"  : "=r"(v)); return v;
}
static inline uint64_t read_pfr0(void) {
    uint64_t v; asm volatile("mrs %0, id_aa64pfr0_el1"   : "=r"(v)); return v;
}
static inline uint64_t read_zcr(void) {
    /* Only call when SVE is confirmed present — otherwise UNDEFINED */
    uint64_t v; asm volatile("mrs %0, zcr_el1"           : "=r"(v)); return v;
}
static inline uint64_t read_zfr0(void) {
    /* Only call when SVE is confirmed present */
    uint64_t v; asm volatile("mrs %0, id_aa64zfr0_el1"   : "=r"(v)); return v;
}

/* ── Feature extraction helpers ─────────────────────────────────────────── */

/* Extract a 4-bit field from a 64-bit register */
static inline uint8_t field4(uint64_t reg, int shift) {
    return (uint8_t)((reg >> shift) & 0xF);
}

/* ── Hardware detection ──────────────────────────────────────────────────── */

void hw_detect(hw_info_t *info, const void *dtb_addr) {
    uint64_t midr  = read_midr();
    uint64_t isar0 = read_isar0();
    uint64_t isar1 = read_isar1();
    uint64_t pfr0  = read_pfr0();

    /* Save raw values */
    info->midr_raw  = midr;
    info->isar0_raw = isar0;
    info->isar1_raw = isar1;
    info->pfr0_raw  = pfr0;

    /* ── MIDR_EL1 ─────────────────────────────────────────────────────────
     * [31:24] Implementer   [23:20] Variant
     * [19:16] Architecture  [15:4]  PartNum   [3:0] Revision
     */
    info->implementer = (uint8_t)((midr >> 24) & 0xFF);
    info->variant     = (uint8_t)((midr >> 20) & 0xF);
    info->part_num    = (uint16_t)((midr >> 4)  & 0xFFF);
    info->revision    = (uint8_t)(midr & 0xF);

    /* ── ID_AA64PFR0_EL1 ──────────────────────────────────────────────────
     * [35:32] SVE      — 0b0001 = SVE present
     * [23:20] AdvSIMD  — 0b0000 = NEON (no FP16), 0b0001 = NEON+FP16
     * [19:16] FP       — 0b0000 = FP present
     */
    uint8_t adv_simd = field4(pfr0, 20);
    info->has_neon      = (adv_simd != 0xF) ? 1 : 0; /* 0xF = not present */
    info->has_fp16_neon = (adv_simd == 1)   ? 1 : 0;
    info->has_sve       = (field4(pfr0, 32) >= 1) ? 1 : 0;

    /* ── ID_AA64ISAR0_EL1 ─────────────────────────────────────────────────
     * [51:48] FHM  — FP16 multiply-accumulate (FMLAL/FMLSL)
     * [47:44] DP   — INT8 dot product (UDOT/SDOT)
     */
    info->has_fhm     = (field4(isar0, 48) >= 1) ? 1 : 0;
    info->has_dotprod = (field4(isar0, 44) >= 1) ? 1 : 0;

    /* ── ID_AA64ISAR1_EL1 ─────────────────────────────────────────────────
     * [55:52] I8MM — INT8 8x8 matrix multiply (crucial for LLM inference)
     * [47:44] BF16 — BFloat16
     */
    info->has_i8mm = (field4(isar1, 52) >= 1) ? 1 : 0;
    info->has_bf16 = (field4(isar1, 44) >= 1) ? 1 : 0;

    /* ── SVE vector length & SVE2 ─────────────────────────────────────────
     * ZCR_EL1[3:0] = LEN  →  VL = (LEN + 1) * 128 bits
     * Only read if SVE confirmed — otherwise traps to UNDEFINED
     */
    info->has_sve2    = 0;
    info->sve_vl_bits = 0;
    if (info->has_sve) {
        uint64_t zcr = read_zcr();
        info->sve_vl_bits = ((uint32_t)(zcr & 0xF) + 1) * 128;

        /* ID_AA64ZFR0_EL1[3:0] = SVEver: 0b0001 = SVE2 */
        uint64_t zfr0 = read_zfr0();
        info->has_sve2 = (field4(zfr0, 0) >= 1) ? 1 : 0;
    }

    /* ── Memory ───────────────────────────────────────────────────────────
     * Parse the FDT blob that QEMU passes in x0 (Linux AArch64 boot
     * protocol). Falls back to 128 MB if DTB is absent or unreadable.
     */
    uint32_t dtb_mb = dtb_get_ram_mb(dtb_addr);
    info->ram_mb = (dtb_mb > 0) ? dtb_mb : 128;

    /* ── AI score (0-100) ─────────────────────────────────────────────────
     * Each feature contributes points reflecting its real-world speedup
     * impact on INT8/FP16 inference workloads.
     */
    uint32_t score = 10; /* base: any ARM64 system */

    if (info->has_neon)      score += 5;
    if (info->has_fp16_neon) score += 5;
    if (info->has_fhm)       score += 5;
    if (info->has_dotprod)   score += 15; /* ~4x INT8 throughput vs scalar */
    if (info->has_i8mm)      score += 25; /* INT8 matrix ops — biggest win  */
    if (info->has_bf16)      score += 15; /* BF16 cuts memory & improves acc */
    if (info->has_sve) {
        score += 8;
        if (info->sve_vl_bits >= 256) score += 4;
        if (info->sve_vl_bits >= 512) score += 4;
    }
    if (info->has_sve2) score += 4;

    /* RAM bonus */
    if (info->ram_mb >=  2048) score += 2;
    if (info->ram_mb >=  4096) score += 2;
    if (info->ram_mb >=  8192) score += 2;
    if (info->ram_mb >= 12288) score += 1;

    info->ai_score = (score > 100) ? 100 : score;

    /* ── Estimated INT8 TOPS (x10, so 15 = 1.5 TOPS) ─────────────────────
     * Very rough estimate based on ISA features only.
     * Real measurement requires microbenchmarks (future: bench/matmul.c).
     */
    uint32_t tops = 5; /* 0.5 TOPS: baseline NEON INT8 (4 cores, ~125 MHz eq) */
    if (info->has_dotprod) tops = 20;  /* ~2.0 TOPS */
    if (info->has_i8mm)    tops = 60;  /* ~6.0 TOPS */
    if (info->has_sve && info->sve_vl_bits >= 256) tops = tops * 2;
    if (info->has_sve2)                            tops = tops + (tops / 4);
    info->int8_tops_x10 = tops;
}

/* ── Reporting ──────────────────────────────────────────────────────────── */

const char *hw_implementer_name(uint8_t impl) {
    switch (impl) {
        case CPU_IMPL_ARM:       return "ARM";
        case CPU_IMPL_QUALCOMM:  return "Qualcomm";
        case CPU_IMPL_APPLE:     return "Apple";
        case CPU_IMPL_HISILICON: return "HiSilicon";
        case CPU_IMPL_SAMSUNG:   return "Samsung";
        case CPU_IMPL_MEDIATEK:  return "MediaTek";
        case CPU_IMPL_NVIDIA:    return "NVIDIA";
        case CPU_IMPL_AMPERE:    return "Ampere";
        default:                 return "Unknown";
    }
}

const char *hw_cpu_name(uint8_t impl, uint16_t part) {
    if (impl == CPU_IMPL_ARM) {
        switch (part) {
            case CPU_PART_A53:  return "Cortex-A53";
            case CPU_PART_A55:  return "Cortex-A55";
            case CPU_PART_A57:  return "Cortex-A57";
            case CPU_PART_A72:  return "Cortex-A72";
            case CPU_PART_A73:  return "Cortex-A73";
            case CPU_PART_A75:  return "Cortex-A75";
            case CPU_PART_A76:  return "Cortex-A76";
            case CPU_PART_A77:  return "Cortex-A77";
            case CPU_PART_A78:  return "Cortex-A78";
            case CPU_PART_X1:   return "Cortex-X1";
            case CPU_PART_A710: return "Cortex-A710";
            case CPU_PART_X2:   return "Cortex-X2";
            case CPU_PART_A715: return "Cortex-A715";
            case CPU_PART_X3:   return "Cortex-X3";
            case CPU_PART_X4:   return "Cortex-X4";
            default:            return "Unknown ARM Core";
        }
    }
    if (impl == CPU_IMPL_QUALCOMM)  return "Qualcomm Kryo";
    if (impl == CPU_IMPL_APPLE)     return "Apple Silicon";
    if (impl == CPU_IMPL_HISILICON) return "Kirin Core";
    if (impl == CPU_IMPL_SAMSUNG)   return "Samsung Mongoose/Cortex";
    if (impl == CPU_IMPL_MEDIATEK)  return "MediaTek Core";
    return "Unknown";
}

const char *hw_ai_tier_name(ai_tier_t tier) {
    switch (tier) {
        case AI_TIER_MINIMAL:   return "MINIMAL   (basic inference only)";
        case AI_TIER_BASIC:     return "BASIC     (<500M param models)";
        case AI_TIER_CAPABLE:   return "CAPABLE   (1-3B param models)";
        case AI_TIER_STRONG:    return "STRONG    (7B models, quantized)";
        case AI_TIER_AI_NATIVE: return "AI-NATIVE (frontier on-device)";
        default:                return "UNKNOWN";
    }
}

ai_tier_t hw_ai_tier(const hw_info_t *info) {
    if (info->ai_score >= 80) return AI_TIER_AI_NATIVE;
    if (info->ai_score >= 60) return AI_TIER_STRONG;
    if (info->ai_score >= 40) return AI_TIER_CAPABLE;
    if (info->ai_score >= 20) return AI_TIER_BASIC;
    return AI_TIER_MINIMAL;
}

static void yesno(uint8_t v) {
    uart_puts(v ? "YES" : "NO ");
}

void hw_print_report(const hw_info_t *info) {
    ai_tier_t tier = hw_ai_tier(info);

    uart_newline();
    uart_puts("=============================="); uart_newline();
    uart_puts("    AeonOS Hardware Report    "); uart_newline();
    uart_puts("=============================="); uart_newline();
    uart_newline();

    /* CPU */
    uart_puts("[CPU]"); uart_newline();
    uart_puts("  Implementer : "); uart_puts(hw_implementer_name(info->implementer)); uart_newline();
    uart_puts("  Core        : "); uart_puts(hw_cpu_name(info->implementer, info->part_num)); uart_newline();
    uart_puts("  Variant/Rev : r"); uart_print_dec(info->variant);
    uart_puts("p"); uart_print_dec(info->revision); uart_newline();
    uart_puts("  MIDR        : "); uart_print_hex64(info->midr_raw); uart_newline();
    uart_newline();

    /* ISA features */
    uart_puts("[ISA — AI Capabilities]"); uart_newline();
    uart_puts("  NEON        : "); yesno(info->has_neon);      uart_newline();
    uart_puts("  NEON FP16   : "); yesno(info->has_fp16_neon); uart_newline();
    uart_puts("  FHM         : "); yesno(info->has_fhm);       uart_puts("  (FP16 mul-accumulate)"); uart_newline();
    uart_puts("  DOTPROD     : "); yesno(info->has_dotprod);   uart_puts("  (INT8 dot product)");    uart_newline();
    uart_puts("  I8MM        : "); yesno(info->has_i8mm);      uart_puts("  (INT8 matrix multiply)"); uart_newline();
    uart_puts("  BF16        : "); yesno(info->has_bf16);      uart_puts("  (bfloat16)");             uart_newline();
    uart_puts("  SVE         : "); yesno(info->has_sve);
    if (info->has_sve) {
        uart_puts("  ("); uart_print_dec(info->sve_vl_bits); uart_puts("-bit vectors)");
    }
    uart_newline();
    uart_puts("  SVE2        : "); yesno(info->has_sve2); uart_newline();
    uart_newline();

    /* Memory */
    uart_puts("[Memory]"); uart_newline();
    uart_puts("  RAM         : "); uart_print_dec(info->ram_mb); uart_puts(" MB"); uart_newline();
    uart_newline();

    /* AI estimate */
    uart_puts("[AI Performance Estimate]"); uart_newline();
    uart_puts("  Score       : "); uart_print_dec(info->ai_score); uart_puts(" / 100"); uart_newline();
    uart_puts("  INT8 TOPS   : ~"); uart_print_dec(info->int8_tops_x10 / 10);
    uart_puts("."); uart_print_dec(info->int8_tops_x10 % 10); uart_newline();
    uart_puts("  AI Tier     : "); uart_puts(hw_ai_tier_name(tier)); uart_newline();
    uart_newline();

    uart_puts("[Raw ISA Registers]"); uart_newline();
    uart_puts("  ISAR0 : "); uart_print_hex64(info->isar0_raw); uart_newline();
    uart_puts("  ISAR1 : "); uart_print_hex64(info->isar1_raw); uart_newline();
    uart_puts("  PFR0  : "); uart_print_hex64(info->pfr0_raw);  uart_newline();
    uart_newline();

    uart_puts("=============================="); uart_newline();
}
