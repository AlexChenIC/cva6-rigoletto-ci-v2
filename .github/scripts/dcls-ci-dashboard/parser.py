#!/usr/bin/env python3
# SPDX-License-Identifier: Apache-2.0
"""Parse CI job names into structured data for the DCLS CVA6 CI Dashboard Version 2.

Handles three job name formats:

  Format 1 — ci.yml (Legacy):
    execute-riscv64-tests (testcase, config, simulator)
    execute-riscv32-tests (testcase, config, simulator)

  Format 2 — OpenHW upstream Tier 1 / Tier 2:
    RV32 Tier1 cv32a65x / smoke-tests-cv32a65x
    RV64 Tier2 cv64a6_imafdc_sv39_hpdcache_wb / dv-riscv-arch-test

  Format 3 — DCLS Version 2 Tier 1 (dcls-ci-tier1-v2.yml):
    smoke-tests-dcls (cv32a60x)
    dv-riscv-arch-test (cv32a65x)
    dv-riscv-tests-p (cv32a60x)
    (job name set explicitly via matrix.testcase + matrix.config)
"""

import re
from typing import Optional

# Utility/infrastructure jobs to skip (not actual test executions)
SKIP_JOBS = {
    "Setup Tools",
    "Test Summary",
    "Resolve Tier",
    "Prepare Matrix",
    "build-riscv-tests",
    "report-summary",
    "setup-tools",
    "resolve-tier",
    "prepare-matrix",
    "Build Dashboard",
    "Deploy to Pages",
    "Persist Data",
}

# All known CVA6 configs — DCLS primary configs listed first.
# Longest names first for greedy matching in Format 2.
KNOWN_CONFIGS = [
    "cv32a60x",
    "cv32a65x",
    "cv64a6_imafdc_sv39_hpdcache_wb",
    "cv64a6_imafdc_sv39_hpdcache",
    "cv64a6_imafdch_sv39_wb",
    "cv64a6_imafdch_sv39",
    "cv64a6_imafdcv_sv39",
    "cv64a6_imafdc_sv39_openpiton",
    "cv64a60ax",
    "cv32a6_ima_sv32_fpga",
]


def arch_from_config(config: str) -> str:
    """Infer architecture from config name."""
    if config.startswith("cv64"):
        return "rv64"
    elif config.startswith("cv32"):
        return "rv32"
    return "unknown"


def parse_job_name(job_name: str, workflow_name: str) -> Optional[dict]:
    """Parse a CI job name into structured fields.

    Returns:
        dict with keys: arch, config, testcase, simulator (optional)
        None if the job should be skipped (utility/infrastructure job)
    """
    name = job_name.strip()

    # Skip utility jobs
    if name in SKIP_JOBS:
        return None

    # ------------------------------------------------------------------
    # Format 3 — DCLS Version 2 Tier 1 (dcls-ci-tier1-v2.yml / dcls-ci-tier2-v2.yml)
    # Pattern: "testcase (config)"  — single value in parens, no commas
    # Example: "smoke-tests-dcls (cv32a60x)"
    #          "dv-riscv-arch-test (cv32a65x)"
    # ------------------------------------------------------------------
    m = re.match(r"^(.+?)\s+\(([^),]+)\)$", name)
    if m:
        testcase = m.group(1).strip()
        config = m.group(2).strip()
        # Verify config looks like a CVA6 config (starts with cv32/cv64)
        # to avoid false-matches on other single-param job names
        if config.startswith(("cv32", "cv64")):
            return {
                "arch": arch_from_config(config),
                "config": config,
                "testcase": testcase,
            }

    # ------------------------------------------------------------------
    # Format 2 — OpenHW upstream Tier 1 / Tier 2
    # Pattern: "RV32 Tier1 cv32a65x / smoke-tests-cv32a65x"
    # ------------------------------------------------------------------
    m = re.match(r"^(RV(?:32|64))\s+Tier[12]\s+(.+?)\s+/\s+(.+)$", name)
    if m:
        arch = m.group(1).lower()
        config = m.group(2).strip()
        testcase = m.group(3).strip()
        # Skip unresolved matrix placeholders
        if "${{" in config or "${{" in testcase:
            return None
        return {
            "arch": arch,
            "config": config,
            "testcase": testcase,
        }

    # ------------------------------------------------------------------
    # Format 1 — ci.yml (Legacy)
    # Pattern: "execute-riscv64-tests (testcase, config, simulator)"
    # ------------------------------------------------------------------
    m = re.match(r"^execute-riscv(32|64)-tests\s+\(([^)]+)\)$", name)
    if m:
        arch = f"rv{m.group(1)}"
        params = [p.strip() for p in m.group(2).split(",")]

        if len(params) == 3:
            testcase, config, simulator = params
            return {
                "arch": arch,
                "config": config,
                "testcase": testcase,
                "simulator": simulator,
            }
        elif len(params) >= 2:
            return {
                "arch": arch,
                "config": params[1],
                "testcase": params[0],
            }

    # Unrecognized job name — skip it
    return None


if __name__ == "__main__":
    # Quick self-test
    test_cases = [
        # Format 3 — DCLS Version 2 Tier 1
        ("smoke-tests-dcls (cv32a60x)", "dcls-ci-tier1-v2"),
        ("dv-riscv-arch-test (cv32a60x)", "dcls-ci-tier1-v2"),
        ("dv-riscv-arch-test (cv32a65x)", "dcls-ci-tier1-v2"),
        ("dv-riscv-tests-p (cv32a60x)", "dcls-ci-tier1-v2"),
        # Format 2 — OpenHW upstream Tier 1/2
        ("RV32 Tier1 cv32a65x / smoke-tests-cv32a65x", "openhw-cva6-ci-tier1"),
        ("RV64 Tier2 cv64a6_imafdc_sv39_hpdcache_wb / dv-riscv-arch-test", "openhw-cva6-ci-tier2"),
        # Format 1 — ci.yml Legacy
        ("execute-riscv64-tests (cv64a6_imafdc_tests, cv64a6_imafdc_sv39_hpdcache, veri-testharness)", "ci"),
        ("execute-riscv32-tests (dv-riscv-arch-test, cv32a65x, veri-testharness)", "ci"),
        # Skip jobs
        ("Setup Tools", "dcls-ci-tier1-v2"),
        ("Test Summary", "dcls-ci-tier1-v2"),
        ("Build Dashboard", "dcls-ci-dashboard-v2"),
    ]

    for job_name, wf in test_cases:
        result = parse_job_name(job_name, wf)
        print(f"  {job_name!r:70s} -> {result}")
