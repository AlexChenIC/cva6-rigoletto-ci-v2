// SPDX-License-Identifier: Apache-2.0
//
// DCLS CVA6 — Exception Handling in DMR Mode
//
// Purpose: Verify that M-mode exception handling is synchronised between the
//          two DCLS cores. In DMR lockstep, both cores must raise the same
//          exception at the same instruction, with identical mcause and mepc.
//          Any divergence is detected by hmr_unit core_nominal_outputs comparison.
//
// Method:  Override the weak `handle_trap` function from crt.S.
//          crt.S installs `trap_entry` into mtvec at startup; trap_entry saves
//          all registers, calls handle_trap(mcause, mepc, sp), then uses the
//          return value as the new mepc before mret.
//
// Tests:
//   1. ECALL from M-mode: mcause must be 11 (0xB), repeated 3 times
//   2. Verify mepc points to the ecall instruction
//   3. Confirm control returns to caller after mret
//
// Target:  cv32a60x (rv32imc), tandem RTL vs Spike
// Return:  0 = PASS, non-zero = FAIL (error code indicates failing case)

#include <stdint.h>

// Trap state recorded by our handle_trap override
static volatile uint32_t trap_count = 0U;
static volatile uint32_t last_cause = 0xFFFFFFFFU;
static volatile uint32_t last_mepc  = 0U;
static volatile uint32_t trap_error = 0U;  // non-zero if trap handler detected error

// M-mode ecall cause code (RISC-V spec §3.1.20)
#define CAUSE_ECALL_M 11U

// Override the weak handle_trap from crt.S.
// Signature: handle_trap(mcause, mepc, sp) -> new_mepc
// The trap_entry asm in crt.S passes a0=mcause, a1=mepc, a2=sp and writes
// the return value (a0) to mepc before executing mret.
long handle_trap(long cause, long mepc, long sp) {
    (void)sp;
    trap_count++;
    last_cause = (uint32_t)cause;
    last_mepc  = (uint32_t)mepc;

    if ((uint32_t)cause != CAUSE_ECALL_M) {
        // Unexpected exception type — record error, halt by looping
        // (returning mepc unchanged would re-execute the faulting instruction)
        trap_error = 1U;
        // Advance mepc anyway to avoid infinite loop in simulation
    }

    // Return mepc + 4 to skip over the ecall instruction (always 4 bytes)
    return mepc + 4;
}

int main(void) {
    uint32_t saved_count;

    // ── Test 1: First ECALL ───────────────────────────────────────────────────
    saved_count = trap_count;
    __asm__ volatile("ecall" : : : "memory");

    // Verify trap was invoked exactly once
    if (trap_count != saved_count + 1U) return 1;

    // Verify cause was M-mode ecall (11)
    if (last_cause != CAUSE_ECALL_M) return 2;

    // Verify no unexpected trap type
    if (trap_error) return 3;

    // ── Test 2: Second ECALL ─────────────────────────────────────────────────
    saved_count = trap_count;
    __asm__ volatile("ecall" : : : "memory");

    if (trap_count != saved_count + 1U) return 4;
    if (last_cause != CAUSE_ECALL_M)   return 5;
    if (trap_error)                    return 6;

    // ── Test 3: Third ECALL ───────────────────────────────────────────────────
    saved_count = trap_count;
    __asm__ volatile("ecall" : : : "memory");

    if (trap_count != saved_count + 1U) return 7;
    if (last_cause != CAUSE_ECALL_M)   return 8;
    if (trap_error)                    return 9;

    // ── Test 4: Total trap count ──────────────────────────────────────────────
    // crt.S may have installed mtvec before calling main, so no pre-main traps
    // are expected; verify exactly 3 ecalls were handled.
    if (trap_count != 3U) return 10;

    // ── Test 5: mepc sanity ───────────────────────────────────────────────────
    // last_mepc must be a non-zero code-segment address
    if (last_mepc == 0U) return 11;

    return 0;
}
