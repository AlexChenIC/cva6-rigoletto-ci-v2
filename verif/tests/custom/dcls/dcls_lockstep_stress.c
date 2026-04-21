// SPDX-License-Identifier: Apache-2.0
//
// DCLS CVA6 — Lockstep Stress Test
//
// Purpose: Long-running computation that maximises the cycle count and I/D-cache
//          activity to maximise the exposure window for any lockstep divergence
//          between the two DCLS cores (hmr_unit all comparison paths).
//
// Computations:
//   1. Fibonacci(25)     — verifies control flow + accumulator correctness
//   2. Sieve of Eratosthenes (up to 100) — array + branch-heavy algorithm
//   3. 4×4 integer matrix multiply       — structured D-cache + ALU stress
//   4. Byte-level checksum over 512 B    — I-cache pressure + sequential loads
//
// Target:  cv32a60x (rv32imc), tandem RTL vs Spike
// Return:  0 = PASS, non-zero = FAIL (error code indicates failing case)

#include <stdint.h>

// ── 1. Fibonacci ─────────────────────────────────────────────────────────────

static uint32_t fibonacci(uint32_t n) {
    if (n <= 1U) return n;
    uint32_t a = 0U, b = 1U, c;
    for (uint32_t i = 2U; i <= n; i++) {
        c = a + b;
        a = b;
        b = c;
    }
    return b;
}

// ── 2. Sieve of Eratosthenes ─────────────────────────────────────────────────

#define SIEVE_MAX 100
static volatile uint8_t sieve[SIEVE_MAX + 1];

static int count_primes(void) {
    int i, j, count = 0;
    for (i = 0; i <= SIEVE_MAX; i++) sieve[i] = 1U;
    sieve[0] = 0U;
    sieve[1] = 0U;
    for (i = 2; (i * i) <= SIEVE_MAX; i++) {
        if (sieve[i]) {
            for (j = i * i; j <= SIEVE_MAX; j += i)
                sieve[j] = 0U;
        }
    }
    for (i = 2; i <= SIEVE_MAX; i++) {
        if (sieve[i]) count++;
    }
    return count;
}

// ── 3. 4×4 Integer Matrix Multiply ───────────────────────────────────────────

typedef struct { int32_t m[4][4]; } Mat4;

static Mat4 mat_mul(const Mat4 *a, const Mat4 *b) {
    Mat4 c;
    int i, j, k;
    for (i = 0; i < 4; i++) {
        for (j = 0; j < 4; j++) {
            c.m[i][j] = 0;
            for (k = 0; k < 4; k++) {
                c.m[i][j] += a->m[i][k] * b->m[k][j];
            }
        }
    }
    return c;
}

// ── 4. Byte checksum ──────────────────────────────────────────────────────────

#define BUF_SIZE 512
static volatile uint8_t data[BUF_SIZE];

static uint32_t checksum(void) {
    uint32_t sum = 0U;
    for (int i = 0; i < BUF_SIZE; i++) {
        sum += (uint32_t)data[i];
    }
    return sum;
}

// ─────────────────────────────────────────────────────────────────────────────

int main(void) {
    // ── Fibonacci(25) = 75025 ────────────────────────────────────────────────
    uint32_t fib = fibonacci(25U);
    if (fib != 75025U) return 1;

    // Double-check with a few known values
    if (fibonacci(0U) != 0U)  return 2;
    if (fibonacci(1U) != 1U)  return 3;
    if (fibonacci(10U) != 55U) return 4;
    if (fibonacci(20U) != 6765U) return 5;

    // ── Sieve: 25 primes up to 100 ───────────────────────────────────────────
    int primes = count_primes();
    if (primes != 25) return 6;
    // Spot-check: specific primes must be set
    if (!sieve[2])  return 7;
    if (!sieve[97]) return 8;
    if (sieve[4])   return 9;   // 4 is not prime
    if (sieve[100]) return 10;  // 100 is not prime

    // ── 4×4 Matrix Multiply ──────────────────────────────────────────────────
    // A = identity matrix, B = known matrix → result must equal B
    const Mat4 I = { {{1,0,0,0}, {0,1,0,0}, {0,0,1,0}, {0,0,0,1}} };
    const Mat4 B = { {{1,2,3,4}, {5,6,7,8}, {9,10,11,12}, {13,14,15,16}} };
    Mat4 C = mat_mul(&I, &B);
    if (C.m[0][0] != 1)  return 11;
    if (C.m[1][1] != 6)  return 12;
    if (C.m[2][2] != 11) return 13;
    if (C.m[3][3] != 16) return 14;
    if (C.m[3][0] != 13) return 15;

    // A × A where A is [1,2;3,4] in top-left — known result
    const Mat4 A2 = { {{1,2,0,0}, {3,4,0,0}, {0,0,1,0}, {0,0,0,1}} };
    Mat4 A2sq = mat_mul(&A2, &A2);
    // [1,2;3,4]^2 = [1+6, 2+8; 3+12, 6+16] = [7,10;15,22]
    if (A2sq.m[0][0] != 7)  return 16;
    if (A2sq.m[0][1] != 10) return 17;
    if (A2sq.m[1][0] != 15) return 18;
    if (A2sq.m[1][1] != 22) return 19;

    // ── Byte checksum over 512-byte buffer ───────────────────────────────────
    // Fill: data[i] = i & 0xFF
    for (int i = 0; i < BUF_SIZE; i++) {
        data[i] = (uint8_t)(i & 0xFFU);
    }
    // Sum of 0..255 repeated twice = 2 * (255*256/2) = 2 * 32640 = 65280
    uint32_t csum = checksum();
    if (csum != 65280U) return 20;

    // Fill: alternating 0xAA/0x55
    for (int i = 0; i < BUF_SIZE; i++) {
        data[i] = (i & 1) ? 0x55U : 0xAAU;
    }
    // 256 * 0xAA + 256 * 0x55 = 256 * (170 + 85) = 256 * 255 = 65280
    csum = checksum();
    if (csum != 65280U) return 21;

    return 0;
}
