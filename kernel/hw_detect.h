#ifndef HW_DETECT_H
#define HW_DETECT_H

#include "../lib/types.h"

/* CPU implementer codes (MIDR_EL1[31:24]) */
#define CPU_IMPL_ARM        0x41
#define CPU_IMPL_QUALCOMM   0x51
#define CPU_IMPL_APPLE      0x61
#define CPU_IMPL_HISILICON  0x48
#define CPU_IMPL_SAMSUNG    0x53
#define CPU_IMPL_MEDIATEK   0x4D
#define CPU_IMPL_NVIDIA     0x4E
#define CPU_IMPL_AMPERE     0xC0

/* ARM PartNum codes (MIDR_EL1[15:4]) */
#define CPU_PART_A53   0xD03
#define CPU_PART_A55   0xD05
#define CPU_PART_A57   0xD07
#define CPU_PART_A72   0xD08
#define CPU_PART_A73   0xD09
#define CPU_PART_A75   0xD0A
#define CPU_PART_A76   0xD0B
#define CPU_PART_A77   0xD0D
#define CPU_PART_A78   0xD41
#define CPU_PART_X1    0xD44
#define CPU_PART_A710  0xD47
#define CPU_PART_X2    0xD48
#define CPU_PART_A715  0xD4D
#define CPU_PART_X3    0xD4E
#define CPU_PART_X4    0xD82

typedef struct {
    /* CPU identity */
    uint64_t midr_raw;
    uint8_t  implementer;
    uint16_t part_num;
    uint8_t  variant;
    uint8_t  revision;

    /* Raw ISA registers (for full inspection) */
    uint64_t isar0_raw;
    uint64_t isar1_raw;
    uint64_t pfr0_raw;

    /* AI-relevant ISA features */
    uint8_t  has_neon;      /* Advanced SIMD (NEON) baseline            */
    uint8_t  has_fp16_neon; /* FP16 in NEON (ID_AA64PFR0.AdvSIMD = 1)  */
    uint8_t  has_fhm;       /* FP16 mul-accumulate (FMLAL/FMLSL)       */
    uint8_t  has_dotprod;   /* INT8 dot product  (UDOT/SDOT)           */
    uint8_t  has_i8mm;      /* INT8 8x8 matrix multiply                */
    uint8_t  has_bf16;      /* BFloat16 arithmetic                     */
    uint8_t  has_sve;       /* Scalable Vector Extension               */
    uint8_t  has_sve2;      /* SVE2                                    */
    uint32_t sve_vl_bits;   /* SVE vector length in bits (128..2048)   */

    /* Memory */
    uint32_t ram_mb;        /* Estimated RAM in MB                     */

    /* AI performance estimate */
    uint32_t ai_score;      /* 0-100 composite score                   */
    uint32_t int8_tops_x10; /* Estimated INT8 TOPS * 10  (15 = 1.5 T) */
} hw_info_t;

typedef enum {
    AI_TIER_MINIMAL   = 0, /* score  0-19: basic inference only        */
    AI_TIER_BASIC     = 1, /* score 20-39: small models <500M params   */
    AI_TIER_CAPABLE   = 2, /* score 40-59: 1-3B param models           */
    AI_TIER_STRONG    = 3, /* score 60-79: 7B quant models             */
    AI_TIER_AI_NATIVE = 4, /* score 80+  : frontier on-device          */
} ai_tier_t;

void        hw_detect(hw_info_t *info, const void *dtb_addr);
void        hw_print_report(const hw_info_t *info);
ai_tier_t   hw_ai_tier(const hw_info_t *info);
const char *hw_implementer_name(uint8_t impl);
const char *hw_cpu_name(uint8_t impl, uint16_t part);
const char *hw_ai_tier_name(ai_tier_t tier);

#endif
