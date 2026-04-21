#!/usr/bin/env python3
# SPDX-License-Identifier: Apache-2.0
"""Generate DCLS CVA6 CI Dashboard Version 2 HTML from collected JSON data.

Reads per-workflow JSON files and renders a Jinja2 template into
a self-contained static HTML file.

Workflows displayed:
  ci              — ci.yml (Legacy Base)
  dcls-ci-tier1-v2   — DCLS Tier 1 (PR Gate, V2)
  dcls-ci-tier2-v2   — DCLS Tier 2 (Daily Verilator, future)
"""

import argparse
import json
import os
from datetime import datetime, timezone
from pathlib import Path

from jinja2 import Environment, FileSystemLoader

# Workflow display info (order = left-to-right in UI overview cards)
WORKFLOW_INFO = [
    {"key": "ci",            "display_name": "ci.yml (Legacy Base)",        "file": "runs_ci.json"},
    {"key": "dcls-ci-tier1-v2", "display_name": "Tier 1 (PR Gate, V2)",       "file": "runs_dcls-ci-tier1-v2.json"},
    {"key": "dcls-ci-tier2-v2", "display_name": "Tier 2 (Daily Verilator, V2)", "file": "runs_dcls-ci-tier2-v2.json"},
]

# Preferred display order for configs in the matrix.
# DCLS primary configs first, then others alphabetically.
MATRIX_CONFIGS_ORDER = [
    "cv32a60x",
    "cv32a65x",
    "cv64a6_imafdc_sv39_hpdcache_wb",
    "cv64a60ax",
    "cv64a6_imafdch_sv39_wb",
    "cv64a6_imafdch_sv39",
    "cv64a6_imafdcv_sv39",
    "cv64a6_imafdc_sv39_openpiton",
    "cv32a6_ima_sv32_fpga",
]

# Preferred display order for test suites in the matrix.
# DCLS suites first, then OpenHW/legacy suites.
MATRIX_SUITES_ORDER = [
    "smoke-tests-dcls",
    "dv-riscv-arch-test",
    "dv-riscv-arch-test-rvc",
    "dv-riscv-arch-test-rvm",
    "dv-riscv-tests-p",
    "dv-riscv-tests-v",
    "smoke-tests-cv32a65x",
    "cv32a6_tests",
    "cv64a6_imafdc_tests",
    "dv-riscv-compliance",
    "hello-world",
]

TREND_COUNT = 20


def is_valid_matrix_job(job: dict) -> bool:
    """Return True if a job has usable config/testcase for matrix rendering."""
    config = job.get("config", "")
    testcase = job.get("testcase", "")
    if not config or not testcase:
        return False
    if "${{" in config or "${{" in testcase:
        return False
    return True


def format_duration(seconds: int) -> str:
    """Format seconds into human-readable duration."""
    if seconds <= 0:
        return "N/A"
    minutes = seconds // 60
    secs = seconds % 60
    if minutes >= 60:
        hours = minutes // 60
        mins = minutes % 60
        return f"{hours}h {mins}m"
    return f"{minutes}m {secs}s"


def format_datetime(iso_str: str) -> str:
    """Format ISO datetime to readable string."""
    if not iso_str:
        return "N/A"
    try:
        dt = datetime.fromisoformat(iso_str.replace("Z", "+00:00"))
        return dt.strftime("%Y-%m-%d %H:%M UTC")
    except (ValueError, TypeError):
        return iso_str


def load_workflow_data(data_dir: Path) -> dict:
    """Load all workflow JSON data files."""
    result = {}
    for wf in WORKFLOW_INFO:
        path = data_dir / wf["file"]
        if path.exists():
            with open(path) as f:
                result[wf["key"]] = json.load(f)
        else:
            result[wf["key"]] = []
    return result


def build_matrix(all_data: dict) -> tuple:
    """Build unified config x testsuite matrix from ALL workflows.

    Returns: (matrix_data, configs_used, suites_used)
    """
    matrix = {}
    all_configs = set()
    all_suites = set()

    for wf in WORKFLOW_INFO:
        key = wf["key"]
        runs = all_data.get(key, [])
        if not runs:
            continue

        latest = next(
            (run for run in runs if any(is_valid_matrix_job(job) for job in run.get("jobs", []))),
            None,
        )
        if latest is None:
            continue

        for job in latest.get("jobs", []):
            if not is_valid_matrix_job(job):
                continue
            config = job.get("config", "")
            testcase = job.get("testcase", "")

            all_configs.add(config)
            all_suites.add(testcase)

            if config not in matrix:
                matrix[config] = {}
            if testcase not in matrix[config]:
                matrix[config][testcase] = {}

            matrix[config][testcase][key] = {
                "conclusion": job.get("conclusion", "unknown"),
                "html_url": job.get("html_url", ""),
            }

    def ordered(items, preferred):
        result = [c for c in preferred if c in items]
        extras = sorted(items - set(preferred))
        return result + extras

    configs_used = ordered(all_configs, MATRIX_CONFIGS_ORDER)
    suites_used = ordered(all_suites, MATRIX_SUITES_ORDER)

    return matrix, configs_used, suites_used


def build_chart_data(all_data: dict) -> dict:
    """Build Chart.js data for trend charts."""
    chart_data = {}

    for wf in WORKFLOW_INFO:
        key = wf["key"]
        runs = all_data.get(key, [])
        trend_runs = list(reversed(runs[:TREND_COUNT]))

        labels = []
        pass_rates = []
        durations = []

        for run in trend_runs:
            labels.append(str(run.get("run_number", "")))
            total = run.get("total_jobs", 0)
            passed = run.get("passed_jobs", 0)
            rate = round(passed / total * 100, 1) if total > 0 else 0
            pass_rates.append(rate)
            dur_min = round(run.get("duration_seconds", 0) / 60, 1)
            durations.append(dur_min)

        chart_data[key] = {
            "labels": labels,
            "pass_rates": pass_rates,
            "durations": durations,
        }

    return chart_data


def enrich_run(run: dict) -> dict:
    """Add display-friendly fields to a run dict."""
    run["duration_display"] = format_duration(run.get("duration_seconds", 0))
    run["created_at_display"] = format_datetime(run.get("created_at", ""))
    for job in run.get("jobs", []):
        job["duration_display"] = format_duration(job.get("duration_seconds", 0))
    return run


def build_workflows_context(all_data: dict) -> list:
    """Build the workflows list for the template context."""
    workflows = []

    for wf in WORKFLOW_INFO:
        key = wf["key"]
        runs = all_data.get(key, [])

        for run in runs:
            enrich_run(run)

        if runs:
            latest = runs[0]
        else:
            latest = {
                "conclusion": "unknown",
                "head_branch": "N/A",
                "head_sha": "N/A",
                "passed_jobs": 0,
                "failed_jobs": 0,
                "skipped_jobs": 0,
                "total_jobs": 0,
                "duration_display": "N/A",
                "run_number": 0,
                "html_url": "#",
            }

        workflows.append(
            {
                "key": key,
                "display_name": wf["display_name"],
                "latest": latest,
                "runs": runs,
            }
        )

    return workflows


def main():
    parser = argparse.ArgumentParser(
        description="Generate DCLS CVA6 CI Dashboard Version 2 HTML"
    )
    parser.add_argument(
        "--data-dir",
        default="data",
        help="Directory containing JSON data files",
    )
    parser.add_argument(
        "--output-dir",
        default="site",
        help="Output directory for generated HTML",
    )
    parser.add_argument(
        "--repo",
        default=os.environ.get("GITHUB_REPOSITORY", "AlexChenIC/cva6-rigoletto-ci-v2"),
        help="GitHub repository (owner/name)",
    )
    args = parser.parse_args()

    data_dir = Path(args.data_dir)
    output_dir = Path(args.output_dir)
    output_dir.mkdir(parents=True, exist_ok=True)

    all_data = load_workflow_data(data_dir)

    now = datetime.now(timezone.utc)
    workflows = build_workflows_context(all_data)
    matrix_data, matrix_configs, matrix_suites = build_matrix(all_data)
    chart_data = build_chart_data(all_data)

    # Prefer Tier 1 as the default matrix view (Tier 2 may not exist yet)
    default_matrix_wf = "dcls-ci-tier1-v2"
    if not all_data.get("dcls-ci-tier1-v2"):
        for wf in WORKFLOW_INFO:
            if all_data.get(wf["key"]):
                default_matrix_wf = wf["key"]
                break

    context = {
        "generated_at": now.strftime("%Y-%m-%d %H:%M UTC"),
        "year": now.year,
        "repo": args.repo,
        "workflows": workflows,
        "matrix_data": matrix_data,
        "matrix_data_json": json.dumps(matrix_data),
        "matrix_configs": matrix_configs,
        "matrix_configs_json": json.dumps(matrix_configs),
        "matrix_suites": matrix_suites,
        "matrix_suites_json": json.dumps(matrix_suites),
        "default_matrix_wf": default_matrix_wf,
        "chart_data_json": json.dumps(chart_data),
        "trend_count": TREND_COUNT,
    }

    template_dir = Path(__file__).parent / "templates"
    env = Environment(loader=FileSystemLoader(str(template_dir)), autoescape=True)
    template = env.get_template("index.html")
    html = template.render(**context)

    output_file = output_dir / "index.html"
    with open(output_file, "w") as f:
        f.write(html)

    # Copy static assets (logo, etc.) alongside the HTML
    import shutil
    static_dir = Path(__file__).parent / "static"
    if static_dir.exists():
        for asset in static_dir.iterdir():
            if asset.is_file():
                shutil.copy(asset, output_dir / asset.name)

    print(f"Dashboard generated: {output_file}")
    print(f"  Workflows: {len(workflows)}")
    for wf in workflows:
        print(f"    - {wf['display_name']}: {len(wf['runs'])} runs")
    print(f"  Matrix: {len(matrix_configs)} configs x {len(matrix_suites)} suites")


if __name__ == "__main__":
    main()
