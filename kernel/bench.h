#ifndef BENCH_H
#define BENCH_H

#include "../lib/types.h"
#include "hw_detect.h"

typedef struct {
    uint8_t  timer_ok;       /* 1 if CNTFRQ_EL0 is non-zero                */
    uint64_t cntfrq;         /* ARM generic timer frequency in Hz           */
    uint64_t elapsed_ticks;  /* timer ticks the benchmark ran for           */
    uint64_t total_macs;     /* total multiply-accumulate ops executed      */
    uint32_t gops_x10;       /* measured GOPS * 10  (e.g. 25 = 2.5 GOPS)  */
} bench_result_t;

/*
 * Run an INT8 multiply-accumulate benchmark.
 * Uses the ARM generic timer for measurement.
 * Chooses the best available ISA path based on hw_info.
 */
void bench_run(const hw_info_t *hw, bench_result_t *result);
void bench_print(const bench_result_t *result);

#endif
