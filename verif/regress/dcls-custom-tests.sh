# SPDX-License-Identifier: Apache-2.0
#
# DCLS CVA6 — Custom DCLS Test Regression Script
#
# Purpose: Run DCLS-specific C tests targeting the UniBo hmr_unit/dmr_checker
#          modules. Exercises all three comparison paths in hmr_unit:
#            - core_nominal_outputs   (ALU results, CSR values, exceptions)
#            - dcache_req_core2hmr    (all 4 D-cache request ports)
#            - icache_dreq_core2hmr   (I-cache data requests, lockstep stress)
#
# Tests (5 sequential):
#   1. dcls_alu_verify    — RV32I+M ALU exhaustive (30 cases, bit-walking)
#   2. dcls_memory_stress — all load/store widths + 256-element array
#   3. dcls_csr_check     — mhartid/misa/mscratch/mcycle/csrrw in DMR mode
#   4. dcls_lockstep_stress — Fibonacci/sieve/matrix-mul/checksum
#   5. dcls_exception_basic — ECALL × 3, mcause/mepc verification
#
# Usage:  sourced by dcls-ci-tier1.yml (mode: script)
#         DV_TARGET and DV_SIMULATORS are set by the CI matrix before sourcing.
#
# Based on: verif/regress/smoke-tests-dcls.sh

if ! [ -n "$RISCV" ]; then
  echo "Error: RISCV variable undefined"
  return 1
fi

if ! [ -n "$DV_SIMULATORS" ]; then
  DV_SIMULATORS=veri-testharness,spike
fi

if ! [ -n "$DV_TARGET" ]; then
  DV_TARGET=cv32a60x
fi

# Install simulation tools
if [[ "$DV_SIMULATORS" == *"veri-testharness"* ]]; then
  source ./verif/regress/install-verilator.sh
fi
source ./verif/regress/install-spike.sh

# Setup environment
source ./verif/sim/setup-env.sh

echo "DCLS custom tests — target: $DV_TARGET, simulators: $DV_SIMULATORS"

if ! [ -n "$UVM_VERBOSITY" ]; then
    export UVM_VERBOSITY=UVM_NONE
fi

export DV_OPTS="$DV_OPTS --issrun_opts=+debug_disable=1+UVM_VERBOSITY=$UVM_VERBOSITY"

CC_OPTS="-static -mcmodel=medany -fvisibility=hidden -nostdlib -nostartfiles \
  -g ../tests/custom/common/syscalls.c ../tests/custom/common/crt.S \
  -I../tests/custom/env -I../tests/custom/common -lgcc"

LINKER=../../config/gen_from_riscv_config/$DV_TARGET/linker/link.ld

# Track overall failure status
RC=0

cd verif/sim/

# ── Test 1: dcls_alu_verify ──────────────────────────────────────────────────
# All RV32I+M ALU operations with known values; covers nominal_outputs path.
echo "--- [1/5] dcls_alu_verify ---"
make -C ../.. clean && make clean_all
python3 cva6.py \
  --c_tests ../tests/custom/dcls/dcls_alu_verify.c \
  --iss_yaml cva6.yaml \
  --target $DV_TARGET \
  --iss=$DV_SIMULATORS \
  --linker=$LINKER \
  --gcc_opts="$CC_OPTS" \
  $DV_OPTS || RC=$((RC + 1))
make -C ../.. clean && make clean_all

# ── Test 2: dcls_memory_stress ───────────────────────────────────────────────
# All load/store widths + 256-word sequential + stride access + store-then-load.
echo "--- [2/5] dcls_memory_stress ---"
make -C ../.. clean && make clean_all
python3 cva6.py \
  --c_tests ../tests/custom/dcls/dcls_memory_stress.c \
  --iss_yaml cva6.yaml \
  --target $DV_TARGET \
  --iss=$DV_SIMULATORS \
  --linker=$LINKER \
  --gcc_opts="$CC_OPTS" \
  $DV_OPTS || RC=$((RC + 1))
make -C ../.. clean && make clean_all

# ── Test 3: dcls_csr_check ───────────────────────────────────────────────────
# mhartid/misa/mscratch/mcycle/csrrw in M-mode DMR configuration.
echo "--- [3/5] dcls_csr_check ---"
make -C ../.. clean && make clean_all
python3 cva6.py \
  --c_tests ../tests/custom/dcls/dcls_csr_check.c \
  --iss_yaml cva6.yaml \
  --target $DV_TARGET \
  --iss=$DV_SIMULATORS \
  --linker=$LINKER \
  --gcc_opts="$CC_OPTS" \
  $DV_OPTS || RC=$((RC + 1))
make -C ../.. clean && make clean_all

# ── Test 4: dcls_lockstep_stress ─────────────────────────────────────────────
# Long-running: Fibonacci(25) + sieve(100) + 4×4 matmul + 512B checksum.
echo "--- [4/5] dcls_lockstep_stress ---"
make -C ../.. clean && make clean_all
python3 cva6.py \
  --c_tests ../tests/custom/dcls/dcls_lockstep_stress.c \
  --iss_yaml cva6.yaml \
  --target $DV_TARGET \
  --iss=$DV_SIMULATORS \
  --linker=$LINKER \
  --gcc_opts="$CC_OPTS" \
  $DV_OPTS || RC=$((RC + 1))
make -C ../.. clean && make clean_all

# ── Test 5: dcls_exception_basic ─────────────────────────────────────────────
# ECALL × 3 from M-mode; verifies mcause=11, mepc, and trap count.
echo "--- [5/5] dcls_exception_basic ---"
make -C ../.. clean && make clean_all
python3 cva6.py \
  --c_tests ../tests/custom/dcls/dcls_exception_basic.c \
  --iss_yaml cva6.yaml \
  --target $DV_TARGET \
  --iss=$DV_SIMULATORS \
  --linker=$LINKER \
  --gcc_opts="$CC_OPTS" \
  $DV_OPTS || RC=$((RC + 1))
make -C ../.. clean && make clean_all

cd -

if [ $RC -ne 0 ]; then
  echo "DCLS custom tests FAILED: $RC test(s) failed"
else
  echo "DCLS custom tests PASSED: all 5 tests passed"
fi

return $RC
