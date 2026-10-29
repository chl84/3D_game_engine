#!/usr/bin/env python3
"""Validate CLI JSON output against the published schemas."""

from __future__ import annotations

import json
import subprocess
import sys
import tempfile
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

    command_schema = load_schema(root, "command-result@1.schema.json")

    def run_command(arguments: list[str], label: str) -> dict[str, Any]:
        result = run_engine(engine, [*arguments, "--json"])
        if result.returncode != 0:
            raise RuntimeError(
                f"{label} exited with {result.returncode}: {result.stderr.strip()}"
            )
        try:
            document = json.loads(result.stdout)
        except json.JSONDecodeError as error:
            raise RuntimeError(f"{label} output is not valid JSON: {error}") from error
        validate(command_schema, document, label)
        if document.get("status") != "ok":
            raise RuntimeError(f"{label} did not report status ok")
        return document

    def run_failure(arguments: list[str], label: str, expected_exit: int) -> dict[str, Any]:
        result = run_engine(engine, [*arguments, "--json"])
        if result.returncode != expected_exit:
            raise RuntimeError(
                f"{label} exited with {result.returncode}, expected {expected_exit}: "
                f"{result.stderr.strip()}"
            )
        try:
            document = json.loads(result.stdout)
        except json.JSONDecodeError as error:
            raise RuntimeError(f"{label} output is not valid JSON: {error}") from error
        validate(command_schema, document, label)
        if document.get("status") != "error":
            raise RuntimeError(f"{label} did not report status error")
        return document

    try:
        schema_document = run_command(["schema"], "schema")
        components = schema_document["data"].get("components")
        if not isinstance(components, dict):
            return fail("schema output has no components object")
        for name in ("engine.scene.Transform", "engine.scene.Camera", "engine.scene.Motion"):
            if name not in components:
                return fail(f"schema output is missing {name}")
        commands = schema_document["data"].get("commands")
        if not isinstance(commands, dict) or "scene.entity.create@1" not in commands:
            return fail("schema output has no command schemas")

        with tempfile.TemporaryDirectory(prefix="engine-cli-contract-") as temporary:
            project_dir = Path(temporary) / "demo"
            created = run_command(
                ["project", "create", str(project_dir), "--name", "Demo"],
                "project create",
            )
            if created["data"].get("name") != "Demo":
                return fail("project create did not report the project name")

            inspected = run_command(
                ["scene", "inspect", str(project_dir)], "scene inspect"
            )
            if inspected["data"].get("entities") != []:
                return fail("new project should have an empty scene")

            main_scene = project_dir / "scenes" / "main.scene.json"
            main_scene_before = main_scene.read_bytes()
            conflict = run_failure(
                [
                    "scene",
                    "create",
                    str(project_dir),
                    "--name",
                    "Overwrite",
                    "--path",
                    "scenes/main.scene.json",
                ],
                "scene create existing file",
                5,
            )
            if conflict["error"]["code"] != "E_CONFLICT":
                return fail("scene create existing file did not report E_CONFLICT")
            if main_scene.read_bytes() != main_scene_before:
                return fail("failed scene create changed the existing scene")

            second_scene = project_dir / "scenes" / "second.scene.json"
            run_command(
                [
                    "scene",
                    "create",
                    str(project_dir),
                    "--name",
                    "second",
                    "--path",
                    "scenes/second.scene.json",
                ],
                "scene create second",
            )
            second_scene_before = second_scene.read_bytes()
            second_scene.unlink()
            listed_conflict = run_failure(
                [
                    "scene",
                    "create",
                    str(project_dir),
                    "--name",
                    "second",
                    "--path",
                    "scenes/second.scene.json",
                ],
                "scene create listed but missing",
                5,
            )
            if listed_conflict["error"]["code"] != "E_CONFLICT":
                return fail("scene create listed but missing did not report E_CONFLICT")
            if second_scene.exists():
                return fail("failed scene create recreated a missing listed scene")
            second_scene.write_bytes(second_scene_before)

            dry_project = Path(temporary) / "dry-project"
            run_command(
                [
                    "project",
                    "create",
                    str(dry_project),
                    "--name",
                    "Dry",
                    "--dry-run",
                ],
                "project create dry-run",
            )
            if dry_project.exists():
                return fail("project create --dry-run created files")

            root_created = run_command(
                ["entity", "create", str(project_dir), "--name", "Root"],
                "entity create root",
            )
            root_id = root_created["data"]["result"]["id"]
            if not isinstance(root_id, str):
                return fail("entity create did not return an entity id")

            run_command(
                [
                    "entity",
                    "create",
                    str(project_dir),
                    "--name",
                    "DryEntity",
                    "--dry-run",
                ],
                "entity create dry-run",
            )
            inspected = run_command(
                ["scene", "inspect", str(project_dir)], "scene inspect after dry-run"
            )
            if len(inspected["data"]["entities"]) != 1:
                return fail("entity create --dry-run changed the scene")

            extra_scene = project_dir / "scenes" / "extra.scene.json"
            run_command(
                [
                    "scene",
                    "create",
                    str(project_dir),
                    "--name",
                    "extra",
                    "--path",
                    "scenes/extra.scene.json",
                    "--dry-run",
                ],
                "scene create dry-run",
            )
            if extra_scene.exists():
                return fail("scene create --dry-run created a file")

            child_created = run_command(
                [
                    "entity",
                    "create",
                    str(project_dir),
                    "--name",
                    "Child",
                    "--parent",
                    root_id,
                ],
                "entity create child",
            )
            child_id = child_created["data"]["result"]["id"]

            run_command(
                [
                    "component",
                    "set",
                    str(project_dir),
                    "--id",
                    child_id,
                    "--component",
                    "engine.scene.Camera",
                    "--value",
                    json.dumps({"fovYRadians": 0.5}),
                    "--dry-run",
                ],
                "component set dry-run",
            )
            inspected = run_command(
                ["scene", "inspect", str(project_dir)], "scene inspect after component dry-run"
            )
            child = next(
                (entity for entity in inspected["data"]["entities"] if entity.get("id") == child_id),
                None,
            )
            if child is None or "engine.scene.Camera" in child["components"]:
                return fail("component set --dry-run changed the scene")

            run_command(
                [
                    "component",
                    "set",
                    str(project_dir),
                    "--id",
                    child_id,
                    "--component",
                    "engine.scene.Camera",
                    "--value",
                    json.dumps(
                        {
                            "fovYRadians": 0.75,
                            "near": 0.1,
                            "far": 1000.0,
                            "enabled": True,
                        }
                    ),
                ],
                "component set",
            )

            before_undo = run_command(
                ["scene", "inspect", str(project_dir)], "scene inspect before component undo"
            )
            before_revision = before_undo["data"]["revision"]
            undo_file = Path(temporary) / "component-undo.json"
            undo_file.write_text(
                json.dumps(
                    {
                        "commands": [
                            {
                                "name": "scene.component.set@1",
                                "arguments": {
                                    "id": child_id,
                                    "component": "engine.scene.Camera",
                                    "value": {"fovYRadians": 0.9},
                                },
                            }
                        ]
                    }
                ),
                encoding="utf-8",
            )
            run_command(
                [
                    "batch",
                    str(project_dir),
                    "--file",
                    str(undo_file),
                    "--undo",
                    "1",
                ],
                "component set undo",
            )
            after_undo = run_command(
                ["scene", "inspect", str(project_dir)], "scene inspect after component undo"
            )
            if after_undo["data"]["revision"] <= before_revision:
                return fail("component undo did not bump the revision")
            undone_child = next(
                (
                    entity
                    for entity in after_undo["data"]["entities"]
                    if entity.get("id") == child_id
                ),
                None,
            )
            if undone_child is None or (
                undone_child["components"]["engine.scene.Camera"]["fovYRadians"] != 0.75
            ):
                return fail("component undo did not restore the previous value")

            run_command(
                [
                    "component",
                    "add",
                    str(project_dir),
                    "--id",
                    child_id,
                    "--component",
                    "engine.scene.Motion",
                    "--dry-run",
                ],
                "component add dry-run",
            )
            run_command(
                [
                    "component",
                    "remove",
                    str(project_dir),
                    "--id",
                    child_id,
                    "--component",
                    "engine.scene.Camera",
                    "--dry-run",
                ],
                "component remove dry-run",
            )
            run_command(
                [
                    "entity",
                    "delete",
                    str(project_dir),
                    "--id",
                    root_id,
                    "--dry-run",
                ],
                "entity delete dry-run",
            )
            run_command(
                [
                    "entity",
                    "reparent",
                    str(project_dir),
                    "--id",
                    child_id,
                    "--parent",
                    "null",
                    "--dry-run",
                ],
                "entity reparent dry-run",
            )
            dry_checked = run_command(
                ["scene", "inspect", str(project_dir)], "scene inspect after more dry-runs"
            )
            dry_child = next(
                (
                    entity
                    for entity in dry_checked["data"]["entities"]
                    if entity.get("id") == child_id
                ),
                None,
            )
            if dry_child is None or "engine.scene.Motion" in dry_child["components"]:
                return fail("component add --dry-run changed the scene")
            if "engine.scene.Camera" not in dry_child["components"]:
                return fail("component remove --dry-run changed the scene")
            if dry_child.get("parentId") != root_id:
                return fail("entity reparent --dry-run changed the hierarchy")
            if len(dry_checked["data"]["entities"]) != 2:
                return fail("entity delete --dry-run changed the scene")

            run_command(
                [
                    "component",
                    "add",
                    str(project_dir),
                    "--id",
                    child_id,
                    "--component",
                    "engine.scene.Motion",
                ],
                "component add motion",
            )
            run_command(
                [
                    "component",
                    "set",
                    str(project_dir),
                    "--id",
                    child_id,
                    "--component",
                    "engine.scene.Motion",
                    "--value",
                    json.dumps({"linearVelocity": [1.0, 0.0, 0.0]}),
                ],
                "component set motion",
            )
            simulation_one = run_command(
                ["simulate", str(project_dir), "--ticks", "60"], "simulate first"
            )
            simulation_two = run_command(
                ["simulate", str(project_dir), "--ticks", "60"], "simulate second"
            )
            if simulation_one["data"] != simulation_two["data"]:
                return fail("two identical simulations produced different state dumps")

            inspected = run_command(
                ["scene", "inspect", str(project_dir)], "scene inspect after changes"
            )
            entities = inspected["data"]["entities"]
            if len(entities) != 2:
                return fail(f"expected two entities, got {len(entities)}")
            child = next(
                (entity for entity in entities if entity.get("id") == child_id), None
            )
            if child is None:
                return fail("child entity is missing from scene inspect")
            if "engine.scene.Camera" not in child["components"]:
                return fail("camera component is missing from child entity")

            run_command(["project", "validate", str(project_dir)], "project validate")
            run_command(["scene", "validate", str(project_dir)], "scene validate")

            batch_file = Path(temporary) / "batch.json"
            batch_file.write_text(
                json.dumps(
                    {
                        "commands": [
                            {
                                "name": "scene.entity.create@1",
                                "arguments": {"name": "Temporary"},
                            }
                        ]
                    }
                ),
                encoding="utf-8",
            )
            batch_result = run_command(
                [
                    "batch",
                    str(project_dir),
                    "--file",
                    str(batch_file),
                    "--undo",
                    "1",
                ],
                "batch",
            )
            if batch_result["data"].get("undoCount") != 1:
                return fail("batch did not report the undo count")
            inspected = run_command(
                ["scene", "inspect", str(project_dir)], "scene inspect after batch"
            )
            if len(inspected["data"]["entities"]) != 2:
                return fail("batch undo did not restore the entity count")

            delete_file = Path(temporary) / "delete.json"
            delete_file.write_text(
                json.dumps(
                    {
                        "commands": [
                            {
                                "name": "scene.entity.delete@1",
                                "arguments": {"id": root_id},
                            }
                        ]
                    }
                ),
                encoding="utf-8",
            )
            run_command(
                [
                    "batch",
                    str(project_dir),
                    "--file",
                    str(delete_file),
                    "--undo",
                    "1",
                ],
                "batch delete subtree undo",
            )
            inspected = run_command(
                ["scene", "inspect", str(project_dir)], "scene inspect after subtree undo"
            )
            if len(inspected["data"]["entities"]) != 2:
                return fail("subtree undo did not restore the hierarchy")

            dry_batch_file = Path(temporary) / "dry-batch.json"
            dry_batch_file.write_text(
                json.dumps(
                    {
                        "commands": [
                            {
                                "name": "scene.entity.create@1",
                                "arguments": {"name": "DryBatch"},
                            }
                        ]
                    }
                ),
                encoding="utf-8",
            )
            run_command(
                [
                    "batch",
                    str(project_dir),
                    "--file",
                    str(dry_batch_file),
                    "--dry-run",
                ],
                "batch dry-run",
            )
            inspected = run_command(
                ["scene", "inspect", str(project_dir)], "scene inspect after batch dry-run"
            )
            if len(inspected["data"]["entities"]) != 2:
                return fail("batch --dry-run changed the scene")

            missing = run_engine(
                engine,
                ["project", "validate", str(project_dir / "missing"), "--json"],
            )
            if missing.returncode == 0:
                return fail("project validate should fail for a missing project")
    except (RuntimeError, OSError) as error:
        return fail(str(error))

    print("CLI contract OK")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
