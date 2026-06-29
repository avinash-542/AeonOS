#include "bench.h"
#include "../drivers/uart.h"

/*
 * INT8 Multiply-Accumulate Benchmark
 * ───────────────────────────────────
 * Measures actual CPU throughput using the ARM generic timer.
 * Uses CNTPCT_EL0 (physical count) and CNTFRQ_EL0 (frequency in Hz).
 *
 * ISB before timer reads prevents out-of-order execution from moving
 * the timer read past the work we're trying to measure.
 *
 * Benchmark: 128-element INT8 dot product, repeated BENCH_ITERS times.
 *   total_macs = BENCH_ITERS * 128 * 2  (mul + add per element)
 *
 * TOPS * 10 = total_macs * 10 * cntfrq / (elapsed_ticks * 1_000_000_000_000)
 * GOPS * 10 = total_macs * 10 * cntfrq / (elapsed_ticks * 1_000_000_000)
 *
 * Overflow check (worst case):
 *   total_macs = 100000 * 128 * 2 = 25_600_000
 *   * 10 = 256_000_000
 *   * cntfrq (max 1 GHz) = 256_000_000_000_000_000
 *   uint64_t max ≈ 18_400_000_000_000_000_000  →  safe.
 */

#define BENCH_ITERS   100000  /* outer loop count                      */
#define VEC_LEN       128     /* INT8 elements per dot-product          */

/* Sink prevents the compiler from eliminating the benchmark loop */
static volatile int32_t bench_sink;

/* Timer helpers */
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

/* ── Benchmark kernels ──────────────────────────────────────────────────── */

/*
 * Scalar path — plain C INT8 multiply-accumulate.
 * With -O2 the compiler will emit NEON SMLAL or MLA for the inner loop,
 * but without architecture extensions it won't use SDOT or SMMLA.
 */
static void run_scalar(int32_t *out_acc) {
    /*
     * Non-trivial init values stop the compiler from folding the result
     * to a compile-time constant.  Stored in BSS so they survive across
     * the outer loop without reloading from stack each time.
     */
    static signed char a[VEC_LEN];
    static signed char b[VEC_LEN];

    for (int i = 0; i < VEC_LEN; i++) {
        a[i] = (signed char)((i * 7  + 3) & 0x7F);
        b[i] = (signed char)((i * 11 + 5) & 0x7F);
    }

    int32_t acc = 0;
    for (int iter = 0; iter < BENCH_ITERS; iter++) {
        for (int i = 0; i < VEC_LEN; i++) {
            acc += (int32_t)a[i] * (int32_t)b[i];
        }
    }
    *out_acc = acc;
}

/* ── Public API ─────────────────────────────────────────────────────────── */

void bench_run(const hw_info_t *hw, bench_result_t *result) {
    (void)hw; /* reserved: future ISA-specific paths (SDOT, SMMLA) */

    result->cntfrq = timer_freq();
    result->timer_ok = (result->cntfrq > 0) ? 1 : 0;

    if (!result->timer_ok) {
        result->elapsed_ticks = 0;
        result->total_macs    = 0;
        result->gops_x10      = 0;
        return;
    }

    int32_t acc;
    uint64_t t0 = timer_count();
    run_scalar(&acc);
    uint64_t t1 = timer_count();

    bench_sink = acc; /* consume result — prevents dead-code elimination */

    result->elapsed_ticks = t1 - t0;
    result->total_macs    = (uint64_t)BENCH_ITERS * VEC_LEN * 2;

    /*
     * GOPS * 10 = (total_macs * 10 * cntfrq) / (elapsed_ticks * 1e9)
     *
     * Divide cntfrq by 1000 first to keep the intermediate product
     * well within uint64_t range.
     */
    if (result->elapsed_ticks > 0) {
        uint64_t freq_khz = result->cntfrq / 1000;
        uint64_t numerator = result->total_macs * 10 * freq_khz;
        /* denominator = elapsed_ticks * 1e6  (because we used freq_khz) */
        uint64_t denominator = result->elapsed_ticks * 1000000ULL;
        result->gops_x10 = (uint32_t)(numerator / denominator);
    } else {
        result->gops_x10 = 0;
    }
}

void bench_print(const bench_result_t *result) {
    uart_newline();
    uart_puts("=============================="); uart_newline();
    uart_puts("    AeonOS Benchmark Report   "); uart_newline();
    uart_puts("=============================="); uart_newline();
    uart_newline();

    uart_puts("[ARM Generic Timer]"); uart_newline();
    uart_puts("  CNTFRQ_EL0  : "); uart_print_dec(result->cntfrq); uart_puts(" Hz"); uart_newline();

    if (!result->timer_ok) {
        uart_puts("  Status      : UNAVAILABLE (CNTFRQ = 0)"); uart_newline();
        uart_puts("  Note        : Timer may require EL2/EL3 setup on real HW"); uart_newline();
        uart_newline();
        return;
    }

    uart_puts("  Status      : OK"); uart_newline();
    uart_newline();

    uart_puts("[INT8 Dot Product Benchmark]"); uart_newline();
    uart_puts("  ISA path    : Scalar (compiler NEON)"); uart_newline();
    uart_puts("  Vector len  : "); uart_print_dec(VEC_LEN); uart_puts(" elements"); uart_newline();
    uart_puts("  Iterations  : "); uart_print_dec(BENCH_ITERS); uart_newline();
    uart_puts("  Total MACs  : "); uart_print_dec(result->total_macs); uart_newline();
    uart_puts("  Timer ticks : "); uart_print_dec(result->elapsed_ticks); uart_newline();

    /* Elapsed in microseconds: elapsed_ticks * 1_000_000 / cntfrq */
    if (result->cntfrq > 0) {
        uint64_t us = result->elapsed_ticks * 1000000ULL / result->cntfrq;
        uart_puts("  Elapsed     : "); uart_print_dec(us); uart_puts(" us"); uart_newline();
    }

    uart_newline();
    uart_puts("[Throughput]"); uart_newline();
    uart_puts("  Measured    : ");
    uart_print_dec(result->gops_x10 / 10); uart_puts(".");
    uart_print_dec(result->gops_x10 % 10); uart_puts(" GOPS (INT8 MAC)"); uart_newline();

    /* Extrapolate TOPS from DOTPROD/I8MM capability if applicable */
    uart_puts("  Note        : Scalar path — add +dotprod/+i8mm build flags"); uart_newline();
    uart_puts("                for ISA-native SDOT / SMMLA benchmarks."); uart_newline();
    uart_newline();

    uart_puts("=============================="); uart_newline();
}
