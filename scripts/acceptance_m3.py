#!/usr/bin/env python3
"""Public CLI/live M3 acceptance, using the existing mirrored Blender golden."""
from __future__ import annotations

import base64
import json
import math
import queue
import shutil
import subprocess
import sys
import tempfile
import threading
from pathlib import Path

import jsonschema

ROOT = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(ROOT / "tests/contract"))
from asset_placement import png_pixels  # noqa: E402
from live_api import alter_glb  # noqa: E402

SCENE = "scenes/main.scene.json"


def camera_rotation():
    """Quaternion looking from (3.7,4.6,4.5) at the origin with Y up.

    The same authored camera is used by the lower-level PBR test. Constructing
    it here keeps this test entirely on the public command interface.
    """
    def normalize(v):
        length = math.sqrt(sum(x * x for x in v))
        return [x / length for x in v]

    back = normalize([3.7, 4.6, 4.5])
    right = normalize([back[2], 0, -back[0]])
    up = [back[1] * right[2] - back[2] * right[1],
          back[2] * right[0] - back[0] * right[2],
          back[0] * right[1] - back[1] * right[0]]
    scale = math.sqrt(1 + right[0] + up[1] + back[2]) * 2
    return [(up[2] - back[1]) / scale, (back[0] - right[2]) / scale,
            (right[1] - up[0]) / scale, scale / 4]


def main():
    if len(sys.argv) != 2:
        print("usage: acceptance_m3.py <engine-binary>", file=sys.stderr)
        return 2
    engine = str(Path(sys.argv[1]).resolve())
    envelope = json.loads((ROOT / "schemas/command-result@1.schema.json").read_text())
    record_schema = json.loads((ROOT / "schemas/live-command-record@1.schema.json").read_text())

    def cli(*args):
        result = subprocess.run([engine, *map(str, args), "--json"], capture_output=True,
                                text=True, timeout=60)
        assert result.returncode == 0, (args, result.returncode, result.stdout, result.stderr)
        document = json.loads(result.stdout)
        jsonschema.Draft7Validator(envelope).validate(document)
        assert document["status"] == "ok", document
        return document["data"]

    with tempfile.TemporaryDirectory(prefix="engine-m3-acceptance-") as temporary:
        project = Path(temporary) / "project"
        cli("project", "create", project)
        model = project / "model.glb"
        shutil.copyfile(ROOT / "tests/assets/m3/blender_mirrored_no_tangents.glb", model)
        imported = cli("asset", "import", project, model)
        asset_id = imported["assetId"]
        cli("asset", "inspect", project, asset_id)
        cli("asset", "validate", project, asset_id)
        sidecar = project / "assets" / asset_id / "asset.json"
        before = json.loads(sidecar.read_text())
        camera = cli("entity", "create", project, "--name", "Camera")["result"]["id"]
        cli("component", "add", project, "--id", camera, "--component", "engine.scene.Camera")
        cli("component", "set", project, "--id", camera, "--component", "engine.scene.Camera",
            "--value", json.dumps({"fovYRadians": 0.65}))
        cli("component", "set", project, "--id", camera, "--component", "engine.scene.Transform",
            "--value", json.dumps({"position": [3.7, 4.6, 4.5], "rotation": camera_rotation()}))
        cli("asset", "place", project, SCENE, "--asset", asset_id, "--name", "Model")
        sun = cli("entity", "create", project, "--name", "Sun")["result"]["id"]
        cli("component", "add", project, "--id", sun, "--component", "engine.scene.DirectionalLight")
        cli("component", "set", project, "--id", sun, "--component", "engine.scene.DirectionalLight",
            "--value", json.dumps({"direction": [-0.4, -1, -0.3], "intensity": 3}))

        def offline_shot(name):
            cli("screenshot", project, SCENE, "--out", name, "--width", 256, "--height", 192,
                "--frames", 2)
            return png_pixels(project / name)

        actual = offline_shot("offline.png")
        golden = png_pixels(ROOT / "tests/fixtures/golden/m3_pbr_256.png")
        assert actual[:2] == golden[:2] == (256, 192)
        deltas = [abs(a - b) for a, b in zip(actual[2], golden[2], strict=True)]
        changed = sum(delta > 3 for delta in deltas)
        assert max(deltas) <= 8 and changed < len(deltas) // 1000, (max(deltas), changed)
        print(f"M3 public CLI Blender golden passed: max={max(deltas)}, changed={changed}")
        saved = (project / SCENE).read_bytes()

        # Live calls go through the shipped client and the exact CommandBus used
        # by the offline commands above. No test-only command implementation.
        stderr_path = Path(temporary) / "live.stderr"
        with stderr_path.open("w+") as stderr:
            process = subprocess.Popen([engine, "live", "serve", str(project), SCENE, "--json"],
                                       stdout=subprocess.PIPE, stderr=stderr, text=True)
            try:
                # Bound startup even when invoked directly outside CTest. The
                # daemon reader is released by process cleanup on every failure.
                lines = queue.Queue()
                reader = threading.Thread(target=lambda: lines.put(process.stdout.readline()),
                                          daemon=True)
                reader.start()
                ready = json.loads(lines.get(timeout=15))
                reader.join(timeout=1)
                jsonschema.Draft7Validator(envelope).validate(ready)
                assert ready["status"] == "ok", ready

                def rpc(method, params=None):
                    return cli("live", "call", project, "--method", method,
                               "--params", json.dumps(params or {}))

                def command(name, arguments=None):
                    return rpc("command", {"name": name, "arguments": arguments or {}})

                def live_shot(name):
                    shot = rpc("screenshot", {"out": name, "width": 256, "height": 192, "frames": 2})
                    assert base64.b64decode(shot["pngBase64"], validate=True) == (project / name).read_bytes()
                    return png_pixels(project / name)

                assert live_shot("live.png") == actual
                command("scene.save@1", {"path": SCENE})
                command("scene.load@1", {"path": SCENE})
                assert (project / SCENE).read_bytes() == saved
                assert live_shot("loaded.png") == actual
                alter_glb(model)
                updated = command("asset.reimport@1", {"assetId": asset_id})
                assert updated["assetId"] == asset_id and updated["cacheKey"] != imported["cacheKey"]
                after = json.loads(sidecar.read_text())
                for field in ("nodes", "meshes", "materials", "textures"):
                    assert [item["id"] for item in before[field]] == [item["id"] for item in after[field]]
                assert (project / SCENE).read_bytes() == saved
                changed_image = live_shot("changed.png")
                assert changed_image != actual, "reimport did not change the rendered geometry"
                command("scene.load@1", {"path": SCENE})
                assert live_shot("changed-loaded.png") == changed_image
                entries = rpc("inspect", {"target": "log", "limit": 64})["entries"]
                assert any(e["name"] == "asset.reimport@1" for e in entries)
                journal = project / ready["data"]["commandLogFile"]
                rpc("shutdown")
                assert process.wait(timeout=10) == 0
            finally:
                if process.poll() is None:
                    process.kill()
                    process.wait(timeout=10)
        assert not stderr_path.read_text(), "live server wrote diagnostics"
        records = [json.loads(line) for line in journal.read_text().splitlines()]
        for record in records:
            jsonschema.Draft7Validator(record_schema).validate(record)
        completed = [r for r in records if r["event"] == "completed"]
        assert [r["name"] for r in completed] == [e["name"] for e in entries]
        assert len(records) == len(completed) * 2
        cli("asset", "validate", project, asset_id)
        assert offline_shot("final-offline.png") == changed_image
        assert (project / SCENE).read_bytes() == saved
        print("M3 import/inspect/validate/place/PBR/save/load/live/reimport/stable IDs/JSONL acceptance passed")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
