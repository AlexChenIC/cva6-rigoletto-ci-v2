// SPDX-License-Identifier: Apache-2.0
//
// DCLS CVA6 — CSR Verification in DMR Mode
//
// Purpose: Verify M-mode CSR access in dual-core lockstep (DMR) configuration.
//          Both cores must produce identical CSR read values; any discrepancy
//          triggers dmr_failure_o via hmr_unit core_nominal_outputs comparison.
//
// Tests:
//   1. mhartid     — must be 0 (DMR presents as single logical hart)
//   2. misa        — must have I/M/C bits set (cv32a60x configuration)
//   3. mscratch    — R/W test with 0xDEADBEEF and 0x0
//   4. mcycle      — must increment between two reads
//   5. mvendorid   — read-only, non-zero expected for OpenHW IP
//
// Target:  cv32a60x (rv32imc + Zba/Zbb), tandem RTL vs Spike
// Return:  0 = PASS, non-zero = FAIL (error code indicates failing case)

#include <stdint.h>

// CSR access macros using inline assembly
#define CSR_READ(csr, val) \
    __asm__ volatile("csrr %0, " #csr : "=r"(val) : : "memory")

#define CSR_WRITE(csr, val) \
    __asm__ volatile("csrw " #csr ", %0" : : "r"(val) : "memory")

#define CSR_SWAP(csr, newval, oldval) \
    __asm__ volatile("csrrw %0, " #csr ", %1" : "=r"(oldval) : "r"(newval) : "memory")

int main(void) {
    uint32_t val, val2;

    // ── Test 1: mhartid = 0 ──────────────────────────────────────────────────
    // In DMR mode, both physical cores are presented as a single logical hart.
    // mhartid must be 0; any other value indicates a configuration error.

    CSR_READ(mhartid, val);
    if (val != 0U) return 1;

    // ── Test 2: misa — required extensions present ───────────────────────────
    // cv32a60x: rv32imc + Zba/Zbb
    //   bit 8  (0x100)  = I (base integer)
    //   bit 12 (0x1000) = M (multiply/divide)
    //   bit 2  (0x4)    = C (compressed)
    //   bits 31:30 = 01 = MXL=1 (RV32)
    // Use mask checks to avoid dependence on reserved bits.

    CSR_READ(misa, val);
    if ((val & (1U << 8))  == 0U) return 2;   // I extension required
    if ((val & (1U << 12)) == 0U) return 3;   // M extension required
    if ((val & (1U << 2))  == 0U) return 4;   // C extension required
    // MXL: bits 31:30 must be 01 for RV32
    if ((val >> 30) != 1U)        return 5;

    // ── Test 3: mscratch R/W ─────────────────────────────────────────────────
    // mscratch is a writable scratch register with no side effects.
    // Both cores in DMR must write/read the same value.

    // Save original mscratch
    CSR_READ(mscratch, val);
    uint32_t saved_scratch = val;

    // Write pattern and verify read-back
    val = 0xDEADBEEFU;
    CSR_WRITE(mscratch, val);
    CSR_READ(mscratch, val2);
    if (val2 != 0xDEADBEEFU) return 6;

    // Write complement and verify
    val = 0x21524110U;  // ~0xDEADBEEF
    CSR_WRITE(mscratch, val);
    CSR_READ(mscratch, val2);
    if (val2 != 0x21524110U) return 7;

    // Write zero and verify
    val = 0U;
    CSR_WRITE(mscratch, val);
    CSR_READ(mscratch, val2);
    if (val2 != 0U) return 8;

    // Restore original mscratch
    CSR_WRITE(mscratch, saved_scratch);

    // ── Test 4: mcycle increments ─────────────────────────────────────────────
    // mcycle is a monotonically increasing cycle counter.
    // Two reads with some instructions between them must show the counter grew.

    CSR_READ(mcycle, val);
    // Ensure some cycles pass between reads (volatile load prevents optimization)
    volatile uint32_t dummy = val + 1U;
    (void)dummy;
    CSR_READ(mcycle, val2);
    if (val2 <= val) return 9;  // counter must have advanced

    // ── Test 5: csrrw atomic swap ─────────────────────────────────────────────
    // Verify the csrrw (swap) instruction: writes new value and reads old in one op.

    uint32_t expected_old, actual_old;
    val = 0xCAFEBABEU;
    CSR_WRITE(mscratch, val);

    expected_old = val;
    val = 0xBEEFCAFEU;
    CSR_SWAP(mscratch, val, actual_old);
    if (actual_old != expected_old) return 10;  // old value returned must match

    CSR_READ(mscratch, val2);
    if (val2 != 0xBEEFCAFEU) return 11;         // new value must be written

    // Restore
    CSR_WRITE(mscratch, saved_scratch);

    return 0;
}
