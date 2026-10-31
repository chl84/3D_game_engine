#!/usr/bin/env python3
"""Measure fresh-process startup and memory for the two unadopted VM topologies."""
from __future__ import annotations

import argparse
import hashlib
import json
import platform
import statistics
import subprocess
import sys
from pathlib import Path


def one(binary: str, model: str, instances: int) -> dict:
    # This helper is a fresh process with exactly one child, so RUSAGE_CHILDREN
    # is this sample's peak, rather than a cumulative maximum across samples.
    run = subprocess.run([binary, "--benchmark", model, str(instances)],
                         capture_output=True, text=True, timeout=60, check=True)
    result = json.loads(run.stdout)
    if sys.platform.startswith("linux"):
        import resource
        result["wholeProcessPeakRssKiB"] = resource.getrusage(resource.RUSAGE_CHILDREN).ru_maxrss
    else:
        result["wholeProcessPeakRssKiB"] = None
    return result


def main() -> None:
    if len(sys.argv) == 5 and sys.argv[1] == "--one":
        print(json.dumps(one(sys.argv[2], sys.argv[3], int(sys.argv[4]))))
        return
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("binary", type=Path)
    parser.add_argument("--runs", type=int, default=5, choices=range(1, 16))
    args = parser.parse_args()
    binary = args.binary.resolve()
    subprocess.run([str(binary), "--verify-lua"], check=True, capture_output=True, timeout=30)
    samples = {"per-instance": [], "per-session": []}
    for repetition in range(args.runs):
        order = list(samples) if repetition % 2 == 0 else list(reversed(samples))
        for model in order:
            run = subprocess.run([sys.executable, __file__, "--one", str(binary), model, "1000"],
                                 check=True, capture_output=True, text=True, timeout=65)
            sample = json.loads(run.stdout)
            assert sample["instances"] == 1000 and sample["status"] == "ok"
            assert sample["buildType"] in ("Release", "RelWithDebInfo"), "use an optimized probe"
            samples[model].append(sample)
    summary = {}
    for model, values in samples.items():
        summary[model] = {
            "instances": 1000,
            "medianStartupMilliseconds": statistics.median(v["startupMilliseconds"] for v in values),
            "medianRetainedLuaBytes": statistics.median(v["retainedLuaBytes"] for v in values),
            "maximumPeakLuaBytes": max(v["peakLuaBytes"] for v in values),
            "exceeds64MiBRuntimeBudget": any(v["exceedsAggregateRuntimeBudget"] for v in values),
            "medianWholeProcessPeakRssKiB": statistics.median(
                v["wholeProcessPeakRssKiB"] for v in values
            ) if all(v["wholeProcessPeakRssKiB"] is not None for v in values) else None,
        }
    profile_path = Path("/sys/firmware/acpi/platform_profile")
    ac = [p.read_text().strip() for p in Path("/sys/class/power_supply").glob("*/online")]
    result = {
        "instancesPerModel": 1000, "freshProcessesPerModel": args.runs,
        "order": "alternating", "host": platform.platform(), "machine": platform.machine(),
        "platformProfile": profile_path.read_text().strip() if profile_path.exists() else None,
        "powerSupplyOnlineValues": ac,
        "binarySha256": hashlib.sha256(binary.read_bytes()).hexdigest(),
        "measurementScope": {
            "startup": "VM setup, allowlist/bindings, environments and equal script initialization",
            "luaMemory": "requested allocator bytes, excluding host C++ allocations/allocator overhead",
            "rss": "Linux launched-child peak including exec/validation/output/cleanup; "
                   "Python launch may impose a baseline",
            "budget": "diagnostic comparison; aggregate limit not used to truncate 1000 instances",
            "adoption": "neither topology is accepted; results require owner decision",
        },
        "summary": summary, "samples": samples,
    }
    print(json.dumps(result, indent=2))


if __name__ == "__main__":
    main()
