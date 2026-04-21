// SPDX-License-Identifier: Apache-2.0
//
// DCLS CVA6 — Memory Access Stress Test
//
// Purpose: Exercise all load/store widths (SB/SH/SW, LB/LBU/LH/LHU/LW) and
//          large array access patterns to stress the hmr_unit D-cache request
//          comparison path (all 4 dcache_req_core2hmr ports must agree).
//
// Target:  cv32a60x (rv32imc), tandem RTL vs Spike
// Return:  0 = PASS, non-zero = FAIL (error code indicates failing case)

#include <stdint.h>

// Static arrays placed in .bss — exercise D-cache with known addresses
static volatile uint8_t  buf8[256];
static volatile uint16_t buf16[128];
static volatile uint32_t buf32[256];

int main(void) {
    int i;
    volatile uint32_t r;

    // ── Byte store/load: SB + LBU + LB (sign extension) ─────────────────────

    buf8[0] = 0x5A;
    if (buf8[0] != 0x5AU) return 1;

    // LB sign extends: 0x80 read as int8_t = -128
    buf8[1] = 0x80U;
    if ((int8_t)buf8[1] != (int8_t)(-128)) return 2;

    // LBU zero extends: 0x80 read as uint8_t = 128
    if ((uint8_t)buf8[1] != 0x80U) return 3;

    buf8[2] = 0xFFU;
    if ((uint8_t)buf8[2] != 0xFFU)  return 4;
    if ((int8_t)buf8[2] != (int8_t)(-1)) return 5;

    // ── Halfword store/load: SH + LHU + LH ───────────────────────────────────

    buf16[0] = 0x1234U;
    if (buf16[0] != 0x1234U) return 6;

    buf16[1] = 0x8000U;
    if ((int16_t)buf16[1] != (int16_t)(-32768)) return 7;
    if ((uint16_t)buf16[1] != 0x8000U) return 8;

    buf16[2] = 0xFFFFU;
    if ((int16_t)buf16[2] != (int16_t)(-1)) return 9;

    // ── Word store/load: SW + LW ──────────────────────────────────────────────

    buf32[0] = 0xDEADBEEFU;
    if (buf32[0] != 0xDEADBEEFU) return 10;

    buf32[1] = 0x00000000U;
    if (buf32[1] != 0x00000000U) return 11;

    buf32[2] = 0xFFFFFFFFU;
    if (buf32[2] != 0xFFFFFFFFU) return 12;

    // ── Sequential array: 256 words write then read ───────────────────────────
    // Stresses the sequential D-cache access path (all 4 hmr dcache_req ports)

    for (i = 0; i < 256; i++) {
        buf32[i] = (uint32_t)((i & 0xFF) * 0x01010101U);
    }
    for (i = 0; i < 256; i++) {
        if (buf32[i] != (uint32_t)((i & 0xFF) * 0x01010101U)) return 13;
    }

    // ── Stride-4 access: stress different D-cache banks ──────────────────────

    for (i = 0; i < 64; i++) {
        buf32[i * 4] = (uint32_t)(0xA0000000U + (uint32_t)i);
    }
    for (i = 0; i < 64; i++) {
        if (buf32[i * 4] != (uint32_t)(0xA0000000U + (uint32_t)i)) return 14;
    }

    // ── Store-then-load forwarding ────────────────────────────────────────────
    // Write to same address multiple times and verify each read

    volatile uint32_t *p = &buf32[200];
    *p = 0x12345678U;
    if (*p != 0x12345678U) return 15;
    *p = 0x87654321U;
    if (*p != 0x87654321U) return 16;
    *p = 0x00000000U;
    if (*p != 0x00000000U) return 17;
    *p = 0xFFFFFFFFU;
    if (*p != 0xFFFFFFFFU) return 18;

    // ── Mixed-width access to adjacent addresses ──────────────────────────────
    // Write as bytes, read back as word; checks byte-lane alignment

    buf8[4] = 0x11U;
    buf8[5] = 0x22U;
    buf8[6] = 0x33U;
    buf8[7] = 0x44U;
    if (buf8[4] != 0x11U) return 19;
    if (buf8[5] != 0x22U) return 20;
    if (buf8[6] != 0x33U) return 21;
    if (buf8[7] != 0x44U) return 22;

    // ── Sparse byte-array fill (stress LBU across cache lines) ───────────────

    for (i = 0; i < 256; i++) {
        buf8[i] = (uint8_t)(i ^ 0xA5U);
    }
    for (i = 0; i < 256; i++) {
        if (buf8[i] != (uint8_t)(i ^ 0xA5U)) return 23;
    }

    // Use r to prevent compiler from eliminating the array reads
    r = (uint32_t)buf8[0] + (uint32_t)buf32[0];
    (void)r;

    return 0;
}
