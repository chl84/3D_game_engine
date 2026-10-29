#!/usr/bin/env python3
"""CLI-only M1 acceptance: project, scene, commands, undo, save/load, simulation."""

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


def main() -> int:
    if len(sys.argv) != 2:
        return fail("usage: acceptance_m1.py <engine-binary>")

    engine = sys.argv[1]
    root = Path(__file__).resolve().parents[1]
    schema = json.loads(
        (root / "schemas" / "command-result@1.schema.json").read_text(encoding="utf-8")
    )

    def run(arguments: list[str], label: str) -> dict[str, Any]:
        result = subprocess.run(
            [engine, *arguments, "--json"], check=False, capture_output=True, text=True
        )
        if result.returncode != 0:
            raise RuntimeError(
                f"{label} exited with {result.returncode}: {result.stderr.strip()}"
            )
        try:
            document = json.loads(result.stdout)
        except json.JSONDecodeError as error:
            raise RuntimeError(f"{label} is not valid JSON: {error}") from error
        jsonschema.Draft7Validator(schema).validate(document)
        if document.get("status") != "ok":
            raise RuntimeError(f"{label} did not report status ok")
        return document

    try:
        with tempfile.TemporaryDirectory(prefix="engine-m1-acceptance-") as temporary:
            project = Path(temporary) / "game"
            run(["project", "create", str(project), "--name", "Acceptance"], "project create")

            root_created = run(
                ["entity", "create", str(project), "--name", "Root"], "entity create root"
            )
            root_id = root_created["data"]["result"]["id"]
            child_created = run(
                [
                    "entity",
                    "create",
                    str(project),
                    "--name",
                    "Player",
                    "--parent",
                    root_id,
                ],
                "entity create child",
            )
            child_id = child_created["data"]["result"]["id"]

            run(
                [
                    "component",
                    "add",
                    str(project),
                    "--id",
                    child_id,
                    "--component",
                    "engine.scene.Camera",
                ],
                "component add camera",
            )
            run(
                [
                    "component",
                    "set",
                    str(project),
                    "--id",
                    child_id,
                    "--component",
                    "engine.scene.Transform",
                    "--value",
                    json.dumps(
                        {
                            "position": [0.0, 1.7, 0.0],
                            "rotation": [0.0, 0.0, 0.0, 1.0],
                            "scale": [1.0, 1.0, 1.0],
                        }
                    ),
                ],
                "component set transform",
            )
            run(
                [
                    "component",
                    "add",
                    str(project),
                    "--id",
                    child_id,
                    "--component",
                    "engine.scene.Motion",
                ],
                "component add motion",
            )
            run(
                [
                    "component",
                    "set",
                    str(project),
                    "--id",
                    child_id,
                    "--component",
                    "engine.scene.Motion",
                    "--value",
                    json.dumps({"linearVelocity": [1.0, 0.0, 0.0]}),
                ],
                "component set motion",
            )

            inspected = run(["scene", "inspect", str(project)], "scene inspect")
            if len(inspected["data"]["entities"]) != 2:
                return fail("expected two entities after setup")

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
            batch = run(
                [
                    "batch",
                    str(project),
                    "--file",
                    str(batch_file),
                    "--undo",
                    "1",
                ],
                "batch undo",
            )
            if batch["data"].get("undoCount") != 1:
                return fail("batch did not undo one command")
            inspected = run(["scene", "inspect", str(project)], "scene inspect after undo")
            if len(inspected["data"]["entities"]) != 2:
                return fail("batch undo did not restore the entity count")

            scene_file = project / "scenes" / "main.scene.json"
            before = scene_file.read_bytes()
            empty_batch = Path(temporary) / "empty-batch.json"
            empty_batch.write_text('{"commands": []}', encoding="utf-8")
            run(
                ["batch", str(project), "--file", str(empty_batch)],
                "batch round-trip",
            )
            after = scene_file.read_bytes()
            if before != after:
                return fail("save/load/save did not produce byte-identical scene files")

            first = run(["simulate", str(project), "--ticks", "60"], "simulate first")
            second = run(["simulate", str(project), "--ticks", "60"], "simulate second")
            if first["data"] != second["data"]:
                return fail("two simulation runs produced different state dumps")

            run(["project", "validate", str(project)], "project validate")
            run(["scene", "validate", str(project)], "scene validate")
    except (RuntimeError, OSError, jsonschema.ValidationError) as error:
        return fail(str(error))

    print("M1 acceptance passed")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
