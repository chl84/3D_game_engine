#!/usr/bin/env python3
"""Verify that point lights produce warnings in validate, run and screenshot."""

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


def has_point_light_warning(document: dict[str, Any]) -> bool:
    warnings = document.get("warnings", [])
    return any("PointLight" in warning and "M5" in warning for warning in warnings)


def main() -> int:
    if len(sys.argv) != 2:
        return fail("usage: point_light_warning.py <engine>")
    engine = sys.argv[1]

    with tempfile.TemporaryDirectory(prefix="engine-point-light-") as temporary:
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

        light_result = run_engine(
            engine, ["entity", "create", str(project), "--name", "Light", "--json"]
        )
        light_document = parse_result(light_result, "light entity create")
        if light_result.returncode != 0:
            print(light_result.stdout, light_result.stderr)
            return fail("light entity create failed")
        light = light_document["data"]["result"]["id"]
        added = run_engine(
            engine,
            [
                "component",
                "add",
                str(project),
                "--id",
                light,
                "--component",
                "engine.scene.PointLight",
                "--json",
            ],
        )
        if added.returncode != 0:
            print(added.stdout, added.stderr)
            return fail("point light component add failed")
        parse_result(added, "point light component add")

        validated = run_engine(engine, ["project", "validate", str(project), "--json"])
        validated_document = parse_result(validated, "project validate")
        if validated.returncode != 0 or not has_point_light_warning(validated_document):
            return fail("project validate did not warn about PointLight")

        scene_validated = run_engine(
            engine, ["scene", "validate", str(project), "scenes/main.scene.json", "--json"]
        )
        scene_validated_document = parse_result(scene_validated, "scene validate")
        if scene_validated.returncode != 0 or not has_point_light_warning(scene_validated_document):
            return fail("scene validate did not warn about PointLight")

        if sys.platform.startswith("linux") and not (
            os.environ.get("DISPLAY") or os.environ.get("WAYLAND_DISPLAY")
        ):
            print("SKIPPED: no display server is available for run and screenshot")
            return SKIP_EXIT_CODE

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
        run_document = parse_result(run_result, "run")
        if run_result.returncode != 0:
            stage = run_document.get("error", {}).get("details", {}).get("stage", "")
            if stage in {"loader", "instance", "physical-device", "validation", "swapchain"}:
                print(f"SKIPPED: no usable Vulkan window path ({stage})")
                return SKIP_EXIT_CODE
            return fail("engine run failed")
        if not has_point_light_warning(run_document):
            return fail("engine run did not warn about PointLight")

        shot_result = run_engine(
            engine,
            [
                "screenshot",
                str(project),
                "scenes/main.scene.json",
                "--width",
                "64",
                "--height",
                "64",
                "--out",
                "point-light.png",
                "--json",
            ],
        )
        shot_document = parse_result(shot_result, "screenshot")
        if shot_result.returncode != 0:
            stage = shot_document.get("error", {}).get("details", {}).get("stage", "")
            if stage in {"loader", "instance", "physical-device", "validation", "swapchain"}:
                print(f"SKIPPED: no usable Vulkan device ({stage})")
                return SKIP_EXIT_CODE
            return fail("engine screenshot failed")
        if not has_point_light_warning(shot_document):
            return fail("engine screenshot did not warn about PointLight")

    print("point light warning CLI OK")
    return 0


if __name__ == "__main__":
    sys.exit(main())
