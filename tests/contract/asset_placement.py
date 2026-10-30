#!/usr/bin/env python3
"""Import/place/render/save/load/reimport through the public CLI on both platforms."""
from __future__ import annotations

import hashlib
import json
import shutil
import struct
import subprocess
import sys
import tempfile
import zlib
from pathlib import Path

import jsonschema

ROOT = Path(__file__).resolve().parents[2]
SCENE = "scenes/main.scene.json"
ENVELOPE = json.loads((ROOT / "schemas/command-result@1.schema.json").read_text())


def snapshot(root):
    return {p.relative_to(root).as_posix(): hashlib.sha256(p.read_bytes()).hexdigest()
            for p in root.rglob("*") if p.is_file()}


def png_pixels(path):
    """Decode stb's non-interlaced RGBA8 output with only the Python standard library."""
    raw = path.read_bytes()
    assert raw[:8] == b"\x89PNG\r\n\x1a\n"
    offset, compressed = 8, bytearray()
    while offset < len(raw):
        size, kind = struct.unpack_from(">I4s", raw, offset)
        data = raw[offset + 8:offset + 8 + size]
        if kind == b"IHDR":
            width, height, depth, color, method, filtering, interlace = struct.unpack(">IIBBBBB", data)
            assert (depth, color, method, filtering, interlace) == (8, 6, 0, 0, 0)
        elif kind == b"IDAT":
            compressed.extend(data)
        offset += size + 12
    data = zlib.decompress(compressed)
    stride = width * 4
    pixels, previous = bytearray(), bytearray(stride)
    for y in range(height):
        start = y * (stride + 1)
        filtering = data[start]
        row = bytearray(data[start + 1:start + 1 + stride])
        for x in range(stride):
            a = row[x - 4] if x >= 4 else 0
            b = previous[x]
            c = previous[x - 4] if x >= 4 else 0
            if filtering == 1:
                predictor = a
            elif filtering == 2:
                predictor = b
            elif filtering == 3:
                predictor = (a + b) // 2
            elif filtering == 4:
                p = a + b - c
                distances = [abs(p - a), abs(p - b), abs(p - c)]
                predictor = (a, b, c)[distances.index(min(distances))]
            else:
                assert filtering == 0
                predictor = 0
            row[x] = (row[x] + predictor) & 255
        pixels.extend(row)
        previous = row
    return width, height, pixels


def halves(image):
    width, height, pixels = image
    background = pixels[:4]
    counts = [0, 0]
    for y in range(height):
        for x in range(width):
            offset = (y * width + x) * 4
            if pixels[offset:offset + 4] != background:
                counts[x >= width // 2] += 1
    return counts


def main():
    engine = str(Path(sys.argv[1]).resolve())

    def run(*args, code=0, error=None):
        result = subprocess.run([engine, *map(str, args), "--json"], capture_output=True,
                                text=True, timeout=45)
        assert result.returncode == code, (args, result.returncode, result.stdout, result.stderr)
        document = json.loads(result.stdout)
        jsonschema.Draft7Validator(ENVELOPE).validate(document)
        if error:
            assert document["error"]["code"] == error, document
        return document["data"] if code == 0 else document

    assert "asset.place@1" in run("schema")["commands"]
    with tempfile.TemporaryDirectory(prefix="engine-asset-placement-") as temporary:
        tmp = Path(temporary)
        project = tmp / "project"
        run("project", "create", project)
        camera = run("entity", "create", project, "--name", "Camera")["result"]["id"]
        run("component", "add", project, "--id", camera, "--component", "engine.scene.Camera")
        run("component", "set", project, "--id", camera, "--component", "engine.scene.Transform",
            "--value", json.dumps({"position": [0, 0, 7]}))
        source = tmp / "cube.glb"
        shutil.copyfile(ROOT / "tests/assets/m3/blender_cube.glb", source)
        imported = run("asset", "import", project, source)
        asset_id = imported["assetId"]
        place = ("asset", "place", project, SCENE, "--asset", asset_id)
        before = snapshot(project)
        preview = run(*place, "--position", -1.5, 0, 0, "--dry-run")
        assert preview["dryRun"] is True and snapshot(project) == before
        run("asset", "place", project, SCENE, "--asset", "invalid", code=3, error="E_VALIDATION")
        run("asset", "place", project, "../escape.json", "--asset", asset_id,
            code=3, error="E_VALIDATION")
        run("asset", "place", project, SCENE, "--asset", "00000000-0000-4000-8000-000000000001",
            code=4, error="E_NOT_FOUND")
        assert snapshot(project) == before
        lock = project / ".engine/lock"
        lock.mkdir()
        run(*place, code=5, error="E_CONFLICT")
        lock.rmdir()

        left = run(*place, "--name", "Left", "--position", -1.5, 0, 0)["result"]
        scene_path = project / SCENE

        def shot(name):
            before_scene = scene_path.read_bytes()
            run("screenshot", project, SCENE, "--out", name, "--width", 128, "--height", 96)
            assert scene_path.read_bytes() == before_scene, "render mutated scene"
            return png_pixels(project / name)

        single = shot("single.png")
        assert halves(single)[0] > 50 and halves(single)[1] == 0, halves(single)
        right = run(*place, "--name", "Right", "--position", 1.5, 0, 0)["result"]
        assert right["id"] != left["id"]
        two = shot("two.png")
        assert all(count > 50 for count in halves(two)), halves(two)
        assert two == shot("reload.png"), "fresh process/load changed rendering"
        document = json.loads(scene_path.read_text())
        assert document["formatVersion"] == 3
        assert len(document["entities"]) == 3, "asset hierarchy expanded into scene entities"
        instances = [e for e in document["entities"] if "engine.scene.AssetInstance" in e["components"]]
        assert len(instances) == 2
        assert all(e["components"]["engine.scene.AssetInstance"]["assetId"] == asset_id for e in instances)

        # Entity deletion undo must restore its asset reference and transform.
        deletion = tmp / "delete.json"
        deletion.write_text(json.dumps({"commands": [
            {"name": "scene.entity.delete@1", "arguments": {"id": left["id"]}}]}))
        run("batch", project, SCENE, "--file", deletion, "--undo", 1)
        assert shot("delete-undo.png") == two

        # Undo a placement in a batch; failure after a placement must roll back.
        batch = tmp / "batch.json"
        batch.write_text(json.dumps({"commands": [{"name": "asset.place@1", "arguments": {"assetId": asset_id}}]}))
        run("batch", project, SCENE, "--file", batch, "--undo", 1)
        assert len(json.loads(scene_path.read_text())["entities"]) == 3
        saved_scene = scene_path.read_bytes()
        batch.write_text(json.dumps({"commands": [
            {"name": "asset.place@1", "arguments": {"assetId": asset_id}},
            {"name": "asset.place@1", "arguments": {"assetId": "invalid"}}]}))
        run("batch", project, SCENE, "--file", batch, code=3, error="E_VALIDATION")
        assert scene_path.read_bytes() == saved_scene

        # Change source geometry through a node transform, then recook without editing the scene.
        raw = source.read_bytes()
        size = struct.unpack_from("<I", raw, 12)[0]
        gltf = json.loads(raw[20:20 + size])
        gltf["nodes"][0]["translation"] = [0, 0.5, 0]
        encoded = json.dumps(gltf, separators=(",", ":")).encode()
        encoded += b" " * (-len(encoded) % 4)
        payload = struct.pack("<II", len(encoded), 0x4E4F534A) + encoded + raw[20 + size:]
        source.write_bytes(struct.pack("<III", 0x46546C67, 2, len(payload) + 12) + payload)
        updated = run("asset", "reimport", project, asset_id)
        assert updated["assetId"] == asset_id and updated["cacheKey"] != imported["cacheKey"]
        assert scene_path.read_bytes() == saved_scene
        changed = shot("reimport.png")
        assert changed != two and all(count > 50 for count in halves(changed))
        assert changed == shot("reimport-reload.png")

        # Missing cache must fail read-only, with the asset ID, then recover after reimport.
        shutil.rmtree(project / updated["cachePath"])
        before = snapshot(project)
        missing = run("screenshot", project, SCENE, "--out", "missing.png", "--width", 128,
                      "--height", 96, code=4, error="E_NOT_FOUND")
        assert missing["error"]["details"]["assetId"] == asset_id
        assert snapshot(project) == before
        run("asset", "reimport", project, asset_id)
        assert shot("recovered.png") == changed
        if "--window" in sys.argv:
            run("run", project, SCENE, "--frames", 3, "--hidden", "--width", 128, "--height", 96)
            assert scene_path.read_bytes() == saved_scene
        print("M3.6 import/place/two instances/render/save/load/reimport, undo and read-only resolution passed")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
