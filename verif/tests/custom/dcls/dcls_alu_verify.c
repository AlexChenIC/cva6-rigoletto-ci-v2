// SPDX-License-Identifier: Apache-2.0
//
// DCLS CVA6 — ALU Exhaustive Verification Test
//
// Purpose: Verify all RV32I and RV32M ALU operations with known values.
//          Exercises hmr_unit core_nominal_outputs comparison path for all
//          arithmetic and logical results (both cores must agree on each op).
//
// Target:  cv32a60x (rv32imc + Zba/Zbb), tandem RTL vs Spike
// Return:  0 = PASS, non-zero = FAIL (error code indicates failing case)

#include <stdint.h>

int main(void) {
    volatile uint32_t a, b, r;
    volatile int32_t  sa, sb, sr;

    // ── RV32I Integer Register-Register ──────────────────────────────────────

    // ADD: 0xAAAAAAAA + 0x55555555 = 0xFFFFFFFF
    a = 0xAAAAAAAAU; b = 0x55555555U;
    r = a + b;
    if (r != 0xFFFFFFFFU) return 1;

    // ADD overflow: 0xFFFFFFFF + 1 = 0 (wraps)
    a = 0xFFFFFFFFU; b = 1U;
    r = a + b;
    if (r != 0U) return 2;

    // SUB: 0xAAAAAAAA - 0x55555555 = 0x55555555
    a = 0xAAAAAAAAU; b = 0x55555555U;
    r = a - b;
    if (r != 0x55555555U) return 3;

    // SUB underflow: 0 - 1 = 0xFFFFFFFF (unsigned wrap)
    a = 0U; b = 1U;
    r = a - b;
    if (r != 0xFFFFFFFFU) return 4;

    // AND: alternating patterns cancel out
    a = 0xAAAAAAAAU; b = 0x55555555U;
    r = a & b;
    if (r != 0U) return 5;

    // AND: same value
    a = 0xDEADBEEFU; b = 0xDEADBEEFU;
    r = a & b;
    if (r != 0xDEADBEEFU) return 6;

    // OR: alternating patterns merge
    a = 0xAAAAAAAAU; b = 0x55555555U;
    r = a | b;
    if (r != 0xFFFFFFFFU) return 7;

    // XOR: same value cancels
    a = 0xDEADBEEFU; b = 0xDEADBEEFU;
    r = a ^ b;
    if (r != 0U) return 8;

    // XOR: alternating patterns toggle
    a = 0xAAAAAAAAU; b = 0x55555555U;
    r = a ^ b;
    if (r != 0xFFFFFFFFU) return 9;

    // SLL: 1 << 31 = 0x80000000
    a = 1U; b = 31U;
    r = a << b;
    if (r != 0x80000000U) return 10;

    // SLL: 0x80000000 << 1 = 0 (overflow)
    a = 0x80000000U; b = 1U;
    r = a << b;
    if (r != 0U) return 11;

    // SRL: 0x80000000 >> 31 = 1 (logical, fills with 0)
    a = 0x80000000U; b = 31U;
    r = a >> b;
    if (r != 1U) return 12;

    // SRL: 0xFFFFFFFF >> 1 = 0x7FFFFFFF
    a = 0xFFFFFFFFU; b = 1U;
    r = a >> b;
    if (r != 0x7FFFFFFFU) return 13;

    // SRA: -1 >> 1 = -1 (arithmetic, fills with sign bit)
    sa = -1; sb = 1;
    sr = sa >> sb;
    if (sr != -1) return 14;

    // SRA: 0x80000000 >> 31 = -1 (all sign bits)
    sa = (int32_t)0x80000000U; sb = 31;
    sr = sa >> sb;
    if (sr != -1) return 15;

    // SLT: -1 < 0 = true (1)
    sa = -1; sb = 0;
    sr = (sa < sb) ? 1 : 0;
    if (sr != 1) return 16;

    // SLT: 0 < -1 = false (0)
    sa = 0; sb = -1;
    sr = (sa < sb) ? 1 : 0;
    if (sr != 0) return 17;

    // SLTU: 0 < 0xFFFFFFFF = true in unsigned
    a = 0U; b = 0xFFFFFFFFU;
    r = (a < b) ? 1U : 0U;
    if (r != 1U) return 18;

    // SLTU: 0xFFFFFFFF < 0 = false in unsigned
    a = 0xFFFFFFFFU; b = 0U;
    r = (a < b) ? 1U : 0U;
    if (r != 0U) return 19;

    // ── RV32M Multiply / Divide ───────────────────────────────────────────────

    // MUL: basic multiply
    a = 12U; b = 12U;
    r = a * b;
    if (r != 144U) return 20;

    // MUL: overflow wraps to lower 32 bits: 0x80000000 * 2 = 0
    a = 0x80000000U; b = 2U;
    r = a * b;
    if (r != 0U) return 21;

    // MUL: 0xFFFF * 0xFFFF = 0xFFFE0001
    a = 0xFFFFU; b = 0xFFFFU;
    r = a * b;
    if (r != 0xFFFE0001U) return 22;

    // DIV (signed): 100 / 7 = 14 (truncate toward zero)
    sa = 100; sb = 7;
    sr = sa / sb;
    if (sr != 14) return 23;

    // DIV (signed): -100 / 7 = -14
    sa = -100; sb = 7;
    sr = sa / sb;
    if (sr != -14) return 24;

    // REM (signed): 100 % 7 = 2
    sa = 100; sb = 7;
    sr = sa % sb;
    if (sr != 2) return 25;

    // REM (signed): -100 % 7 = -2 (sign follows dividend)
    sa = -100; sb = 7;
    sr = sa % sb;
    if (sr != -2) return 26;

    // DIVU: 0xFFFFFFFF / 2 = 0x7FFFFFFF
    a = 0xFFFFFFFFU; b = 2U;
    r = a / b;
    if (r != 0x7FFFFFFFU) return 27;

    // REMU: 100 % 7 = 2
    a = 100U; b = 7U;
    r = a % b;
    if (r != 2U) return 28;

    // ── Bit walking patterns (stress both cores' ALU carry chains) ────────────

    // Walking 1: shifts across all 32 bit positions
    a = 1U;
    for (int i = 0; i < 32; i++) {
        r = a << i;
        if (r != (1U << i)) return 29;
    }

    // Walking 0: AND with complement
    a = 0xFFFFFFFFU;
    for (int i = 0; i < 32; i++) {
        b = ~(1U << i);
        r = a & b;
        if (r != (0xFFFFFFFFU ^ (1U << i))) return 30;
    }

    return 0;
}
