#!/usr/bin/env python3
"""Smoke-test the screenshot CLI, PNG output and command-result envelope."""

from __future__ import annotations

import json
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
        return fail("usage: screenshot.py <engine>")
    engine = sys.argv[1]

    with tempfile.TemporaryDirectory(prefix="engine-screenshot-") as temporary:
        project = Path(temporary) / "demo"
        created = run_engine(engine, ["project", "create", str(project), "--name", "Demo", "--json"])
        if created.returncode != 0:
            print(created.stdout, created.stderr)
            return fail("project create failed")
        parse_result(created, "project create")

        camera_result = run_engine(
            engine, ["entity", "create", str(project), "--name", "Camera", "--json"]
        )
        if camera_result.returncode != 0:
            print(camera_result.stdout, camera_result.stderr)
            return fail("camera entity create failed")
        camera_document = parse_result(camera_result, "camera entity create")
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
                return fail(f"{' '.join(command[:2])} failed")
            parse_result(result, command[1])

        mesh_result = run_engine(
            engine, ["entity", "create", str(project), "--name", "Cube", "--json"]
        )
        if mesh_result.returncode != 0:
            print(mesh_result.stdout, mesh_result.stderr)
            return fail("mesh entity create failed")
        mesh_document = parse_result(mesh_result, "mesh entity create")
        mesh = mesh_document["data"]["result"]["id"]
        mesh_added = run_engine(
            engine,
            ["component", "add", str(project), "--id", mesh, "--component", "engine.scene.Mesh", "--json"],
        )
        if mesh_added.returncode != 0:
            print(mesh_added.stdout, mesh_added.stderr)
            return fail("mesh component add failed")
        parse_result(mesh_added, "mesh component add")

        out_relative = "screenshots/shot.png"
        out = project / out_relative
        shot = run_engine(
            engine,
            [
                "screenshot",
                str(project),
                "scenes/main.scene.json",
                "--frames",
                "2",
                "--width",
                "64",
                "--height",
                "64",
                "--out",
                out_relative,
                "--json",
            ],
        )
        document = parse_result(shot, "screenshot")
        if shot.returncode != 0:
            stage = document.get("error", {}).get("details", {}).get("stage", "")
            if stage in {"loader", "instance", "physical-device"}:
                print(f"SKIPPED: no usable Vulkan device ({stage})")
                return SKIP_EXIT_CODE
            print(shot.stdout, shot.stderr)
            return fail("screenshot failed")
        if document.get("status") != "ok":
            return fail("screenshot did not report status ok")
        if not out.exists():
            return fail("screenshot did not create the PNG")
        data = out.read_bytes()
        if data[:8] != b"\x89PNG\r\n\x1a\n":
            return fail("screenshot output is not a PNG")
        width = int.from_bytes(data[16:20], "big")
        height = int.from_bytes(data[20:24], "big")
        if (width, height) != (64, 64):
            return fail(f"screenshot dimensions are {width}x{height}, expected 64x64")
        if document["data"].get("frames") != 2:
            return fail("screenshot result has the wrong frame count")

        escaped = run_engine(
            engine,
            [
                "screenshot",
                str(project),
                "scenes/main.scene.json",
                "--out",
                "../escape.png",
                "--json",
            ],
        )
        escaped_document = parse_result(escaped, "escaped screenshot")
        if escaped.returncode == 0 or escaped_document.get("status") != "error":
            return fail("screenshot accepted a path outside the project")
        escaped_error = escaped_document.get("error", {})
        if escaped_error.get("code") != "E_VALIDATION":
            return fail("escaped screenshot did not return E_VALIDATION")
        if escaped_error.get("details", {}).get("path") != "../escape.png":
            return fail("escaped screenshot did not report the rejected path")
        if (project.parent / "escape.png").exists():
            return fail("screenshot wrote outside the project")

        oversized = run_engine(
            engine,
            [
                "screenshot",
                str(project),
                "scenes/main.scene.json",
                "--width",
                "8193",
                "--out",
                "screenshots/too-wide.png",
                "--json",
            ],
        )
        oversized_document = parse_result(oversized, "oversized screenshot")
        if oversized.returncode == 0 or oversized_document.get("status") != "error":
            return fail("screenshot accepted dimensions above the limit")
        oversized_error = oversized_document.get("error", {})
        if oversized_error.get("code") != "E_VALIDATION":
            return fail("oversized screenshot did not return E_VALIDATION")
        if oversized_error.get("details", {}).get("maxDimension") != 8192:
            return fail("oversized screenshot did not report maxDimension")

        too_tall = run_engine(
            engine,
            [
                "screenshot",
                str(project),
                "scenes/main.scene.json",
                "--height",
                "8193",
                "--out",
                "screenshots/too-tall.png",
                "--json",
            ],
        )
        too_tall_document = parse_result(too_tall, "too-tall screenshot")
        if too_tall.returncode == 0 or too_tall_document.get("status") != "error":
            return fail("screenshot accepted a height above the limit")
        too_tall_error = too_tall_document.get("error", {})
        if too_tall_error.get("code") != "E_VALIDATION":
            return fail("too-tall screenshot did not return E_VALIDATION")
        if too_tall_error.get("details", {}).get("maxDimension") != 8192:
            return fail("too-tall screenshot did not report maxDimension")

        too_many_frames = run_engine(
            engine,
            [
                "screenshot",
                str(project),
                "scenes/main.scene.json",
                "--frames",
                "1001",
                "--out",
                "screenshots/too-many-frames.png",
                "--json",
            ],
        )
        too_many_frames_document = parse_result(too_many_frames, "too-many-frames screenshot")
        if too_many_frames.returncode == 0 or too_many_frames_document.get("status") != "error":
            return fail("screenshot accepted a frame count above the limit")
        too_many_frames_error = too_many_frames_document.get("error", {})
        if too_many_frames_error.get("code") != "E_VALIDATION":
            return fail("too-many-frames screenshot did not return E_VALIDATION")
        if too_many_frames_error.get("details", {}).get("maxFrames") != 1000:
            return fail("too-many-frames screenshot did not report maxFrames")

    print("screenshot CLI OK")
    return 0


if __name__ == "__main__":
    sys.exit(main())
