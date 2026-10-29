#!/usr/bin/env python3
"""Validate CLI JSON output against the published schemas."""

from __future__ import annotations

import json
import subprocess
import sys
from pathlib import Path
from typing import Any

import jsonschema


def fail(message: str) -> int:
    print(f"error: {message}", file=sys.stderr)
    return 1


def run_engine(engine: str, arguments: list[str]) -> subprocess.CompletedProcess[str]:
    return subprocess.run(
        [engine, *arguments],
        check=False,
        capture_output=True,
        text=True,
    )


def load_schema(root: Path, name: str) -> dict[str, Any]:
    return json.loads((root / "schemas" / name).read_text(encoding="utf-8"))


def validate(schema: dict[str, Any], document: Any, label: str) -> None:
    try:
        jsonschema.Draft7Validator(schema).validate(document)
    except jsonschema.ValidationError as error:
        raise RuntimeError(f"{label} does not match its schema: {error.message}") from error


def main() -> int:
    if len(sys.argv) != 2:
        return fail("usage: validate_cli.py <engine-binary>")

    engine = sys.argv[1]
    root = Path(__file__).resolve().parents[2]

    version_result = run_engine(engine, ["--version", "--json"])
    if version_result.returncode != 0:
        return fail(
            f"engine --version --json exited with {version_result.returncode}: "
            f"{version_result.stderr.strip()}"
        )

    try:
        version_document = json.loads(version_result.stdout)
    except json.JSONDecodeError as error:
        return fail(f"version output is not valid JSON: {error}")

    try:
        validate(
            load_schema(root, "version@1.schema.json"),
            version_document,
            "version output",
        )
    except RuntimeError as error:
        return fail(str(error))

    doctor_result = run_engine(engine, ["doctor", "--json"])
    if doctor_result.returncode != 0:
        return fail(
            f"engine doctor --json exited with {doctor_result.returncode}: "
            f"{doctor_result.stderr.strip()}"
        )

    try:
        doctor_document = json.loads(doctor_result.stdout)
    except json.JSONDecodeError as error:
        return fail(f"doctor output is not valid JSON: {error}")

    try:
        validate(
            load_schema(root, "doctor@1.schema.json"),
            doctor_document,
            "doctor output",
        )
    except RuntimeError as error:
        return fail(str(error))

    if doctor_document.get("name") != "engine":
        return fail("doctor output does not identify the engine")

    if not doctor_document.get("checks"):
        return fail("doctor output has no checks")

    vulkan = doctor_document["vulkan"]
    vulkan_available = (
        vulkan["loader"] and vulkan["instanceCreated"] and bool(vulkan["devices"])
    )
    require_result = run_engine(engine, ["doctor", "--json", "--require", "vulkan"])
    expected_exit = 0 if vulkan_available else 8
    if require_result.returncode != expected_exit:
        return fail(
            "engine doctor --require vulkan exited with "
            f"{require_result.returncode}, expected {expected_exit}"
        )

    try:
        require_document = json.loads(require_result.stdout)
    except json.JSONDecodeError as error:
        return fail(f"required doctor output is not valid JSON: {error}")

    try:
        validate(
            load_schema(root, "doctor@1.schema.json"),
            require_document,
            "required doctor output",
        )
    except RuntimeError as error:
        return fail(str(error))

    bad_result = run_engine(engine, ["doctor", "--require", "unknown", "--json"])
    if bad_result.returncode != 2:
        return fail(
            "engine doctor --require unknown should exit with code 2, "
            f"got {bad_result.returncode}"
        )

    log_schema = load_schema(root, "log-record@1.schema.json")
    log_fixture = root / "tests" / "fixtures" / "log-record.jsonl"
    for line in log_fixture.read_text(encoding="utf-8").splitlines():
        if not line.strip():
            continue
        try:
            validate(log_schema, json.loads(line), "log fixture record")
        except (json.JSONDecodeError, RuntimeError) as error:
            return fail(str(error))

    print("CLI contract OK")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
