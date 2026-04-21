# Copyright 2021 Thales DIS design services SAS
#
# Licensed under the Solderpad Hardware Licence, Version 2.0 (the "License");
# you may not use this file except in compliance with the License.
# SPDX-License-Identifier: Apache-2.0 WITH SHL-2.0
# You may obtain a copy of the License at https://solderpad.org/licenses/
#
# Adapted for DCLS CVA6 (rv32, multi-config) by Junchao Chen
# Based on: verif/regress/smoke-tests-cv32a65x.sh
#
# Supported targets (ADR-003):
#   cv32a60x  — rv32imc + Zba/Zbb, no MMU (primary, 47/47 ISA PASS)
#   cv32a65x  — rv32imc variant 65x, no MMU (upstream CI green)
#
# Tests included:
#   1. hello_world   — basic C execution, tohost=0 pass criterion
#   2. return0       — verifies clean program exit (return 0)

# where are the tools
if ! [ -n "$RISCV" ]; then
  echo "Error: RISCV variable undefined"
  return
fi

if ! [ -n "$DV_SIMULATORS" ]; then
  DV_SIMULATORS=veri-testharness,spike
fi

# Target config: use DV_TARGET if set, default to cv32a60x
if ! [ -n "$DV_TARGET" ]; then
  DV_TARGET=cv32a60x
fi

# install the required tools
if [[ "$DV_SIMULATORS" == *"veri-testharness"* ]]; then
  source ./verif/regress/install-verilator.sh
fi
source ./verif/regress/install-spike.sh

# setup sim env
source ./verif/sim/setup-env.sh

echo "DCLS smoke test — target: $DV_TARGET, simulators: $DV_SIMULATORS"

if ! [ -n "$UVM_VERBOSITY" ]; then
    export UVM_VERBOSITY=UVM_NONE
fi

export DV_OPTS="$DV_OPTS --issrun_opts=+debug_disable=1+UVM_VERBOSITY=$UVM_VERBOSITY"

CC_OPTS="-static -mcmodel=medany -fvisibility=hidden -nostdlib -nostartfiles -g ../tests/custom/common/syscalls.c ../tests/custom/common/crt.S -I../tests/custom/env -I../tests/custom/common -lgcc"

LINKER=../../config/gen_from_riscv_config/$DV_TARGET/linker/link.ld

cd verif/sim/

# ── Test 1: hello_world ─────────────────────────────────────────────────────
# Basic C execution + RTL/ISS tandem comparison
make -C ../.. clean
make clean_all
python3 cva6.py --c_tests ../tests/custom/hello_world/hello_world.c \
  --iss_yaml cva6.yaml --target $DV_TARGET --iss=$DV_SIMULATORS \
  --linker=$LINKER --gcc_opts="$CC_OPTS" $DV_OPTS
make -C ../.. clean
make clean_all

# ── Test 2: return0 ──────────────────────────────────────────────────────────
# Verifies clean program exit path (return 0 → tohost=0)
make -C ../.. clean
make clean_all
python3 cva6.py --c_tests ../tests/custom/return0/return0.c \
  --iss_yaml cva6.yaml --target $DV_TARGET --iss=$DV_SIMULATORS \
  --linker=$LINKER --gcc_opts="$CC_OPTS" $DV_OPTS
make -C ../.. clean
make clean_all

cd -
