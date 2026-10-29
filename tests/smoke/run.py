#!/usr/bin/env python3
"""Smoke-test the windowed `engine run` command contract."""

from __future__ import annotations

import json
import os
import subprocess
import sys
import tempfile
from pathlib import Path
from typing import Any

import jsonschema

ROOT = Path(__file__).resolve().parents[2]
SKIP_EXIT_CODE = 77


def fail(message: str) -> int:
    print(f"error: {message}", file=sys.stderr)
    return 1


def run_engine(engine: str, arguments: list[str]) -> subprocess.CompletedProcess[str]:
    return subprocess.run([engine, *arguments], check=False, capture_output=True, text=True)


def parse_result(result: subprocess.CompletedProcess[str], label: str) -> dict[str, Any]:
    try:
        document = json.loads(result.stdout)
    except json.JSONDecodeError as error:
        raise RuntimeError(f"{label} output is not valid JSON: {error}") from error
    with (ROOT / "schemas" / "command-result@1.schema.json").open(encoding="utf-8") as schema_file:
        schema = json.load(schema_file)
    jsonschema.validate(document, schema)
    return document


def main() -> int:
    if len(sys.argv) != 2:
        return fail("usage: run.py <engine>")
    engine = sys.argv[1]

    if sys.platform.startswith("linux") and not (
        os.environ.get("DISPLAY") or os.environ.get("WAYLAND_DISPLAY")
    ):
        print("SKIPPED: no display server is available")
        return SKIP_EXIT_CODE

    with tempfile.TemporaryDirectory(prefix="engine-run-") as temporary:
        project = Path(temporary) / "demo"
        created = run_engine(engine, ["project", "create", str(project), "--name", "Demo", "--json"])
        if created.returncode != 0:
            print(created.stdout, created.stderr)
            return fail("project create failed")
        parse_result(created, "project create")

        camera_result = run_engine(
            engine, ["entity", "create", str(project), "--name", "Camera", "--json"]
        )
        camera_document = parse_result(camera_result, "camera entity create")
        if camera_result.returncode != 0:
            print(camera_result.stdout, camera_result.stderr)
            return fail("camera entity create failed")
        camera = camera_document["data"]["result"]["id"]

        for command in (
            ["component", "add", str(project), "--id", camera, "--component", "engine.scene.Camera"],
            [
                "component",
                "set",
                str(project),
                "--id",
                camera,
                "--component",
                "engine.scene.Transform",
                "--value",
                '{"position":[0.0,0.0,5.0],"rotation":[0.0,0.0,0.0,1.0],"scale":[1.0,1.0,1.0]}',
            ],
        ):
            result = run_engine(engine, [*command, "--json"])
            if result.returncode != 0:
                print(result.stdout, result.stderr)
                return fail(f"{command[1]} failed")
            parse_result(result, command[1])

        cube_result = run_engine(
            engine, ["entity", "create", str(project), "--name", "Cube", "--json"]
        )
        cube_document = parse_result(cube_result, "cube entity create")
        if cube_result.returncode != 0:
            print(cube_result.stdout, cube_result.stderr)
            return fail("cube entity create failed")
        cube = cube_document["data"]["result"]["id"]
        mesh_added = run_engine(
            engine,
            ["component", "add", str(project), "--id", cube, "--component", "engine.scene.Mesh", "--json"],
        )
        if mesh_added.returncode != 0:
            print(mesh_added.stdout, mesh_added.stderr)
            return fail("mesh component add failed")
        parse_result(mesh_added, "mesh component add")

        run_result = run_engine(
            engine,
            [
                "run",
                str(project),
                "scenes/main.scene.json",
                "--frames",
                "2",
                "--width",
                "320",
                "--height",
                "240",
                "--hidden",
                "--json",
            ],
        )
        document = parse_result(run_result, "run")
        if run_result.returncode != 0:
            stage = document.get("error", {}).get("details", {}).get("stage", "")
            if stage in {"loader", "instance", "physical-device", "validation", "swapchain"}:
                print(f"SKIPPED: no usable Vulkan window path ({stage})")
                return SKIP_EXIT_CODE
            print(run_result.stdout, run_result.stderr)
            return fail("engine run failed")
        if document.get("status") != "ok":
            return fail("engine run did not report status ok")
        data = document.get("data", {})
        if data.get("frames") != 2:
            return fail("engine run did not render the requested frames")
        if (data.get("width"), data.get("height")) != (320, 240):
            return fail("engine run reported the wrong window size")

    print("run CLI OK")
    return 0


if __name__ == "__main__":
    sys.exit(main())
