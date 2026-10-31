#!/usr/bin/env python3
"""Smoke test for `engine --version --json`."""

from __future__ import annotations

import json
import subprocess
import sys


def fail(message: str) -> int:
    print(f"error: {message}", file=sys.stderr)
    return 1


def main() -> int:
    if len(sys.argv) != 2:
        return fail("usage: version_json.py <engine-binary>")

    result = subprocess.run(
        [sys.argv[1], "--version", "--json"],
        check=False,
        capture_output=True,
        text=True,
    )
    if result.returncode != 0:
        return fail(
            f"engine exited with {result.returncode}; stderr: {result.stderr.strip()}"
        )

    try:
        data = json.loads(result.stdout)
    except json.JSONDecodeError as exc:
        return fail(f"stdout is not valid JSON: {exc}")

    expected = {
        "name": "engine",
        "version": "0.4.0",
        "cxxStandard": "20",
    }
    for key, value in expected.items():
        if data.get(key) != value:
            return fail(f"expected {key}={value!r}, got {data.get(key)!r}")

    for key in ("gitCommit", "buildType", "compiler", "platform"):
        if not isinstance(data.get(key), str) or not data[key]:
            return fail(f"missing or empty string field: {key}")

    print(json.dumps(data, indent=2, sort_keys=True))
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
