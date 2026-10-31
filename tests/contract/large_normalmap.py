#!/usr/bin/env python3
"""Import a realistic high-poly Blender normal-mapped asset without tangents.

Every repetition uses a fresh project/cache. Wall time covers the CLI process,
GLB decode, tangent admission/generation, texture decode, cooking and disk writes.
There is deliberately no hardware-dependent performance assertion.
"""
from __future__ import annotations

import argparse
import hashlib
import json
import math
import statistics
import struct
import subprocess
import tempfile
import time
from pathlib import Path

import jsonschema

ROOT = Path(__file__).resolve().parents[2]
SOURCE = ROOT / "tests/assets/m3/large/blender_large_normal_no_tangents.glb"


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("engine", type=Path)
    parser.add_argument("--repeat", type=int, default=1)
    args = parser.parse_args()
    assert args.repeat > 0
    engine = str(args.engine.resolve())
    raw = SOURCE.read_bytes()
    length = struct.unpack_from("<I", raw, 12)[0]
    gltf = json.loads(raw[20:20 + length])
    triangles = 0
    for mesh in gltf["meshes"]:
        for primitive in mesh["primitives"]:
            assert primitive.get("mode", 4) == 4
            assert "TANGENT" not in primitive["attributes"], "must exercise tangent generation"
            assert {"NORMAL", "TEXCOORD_0", "POSITION"} <= primitive["attributes"].keys()
            assert "normalTexture" in gltf["materials"][primitive["material"]]
            triangles += gltf["accessors"][primitive["indices"]]["count"] // 3
    assert triangles == 262144 and triangles >= 200000
    assert all("bufferView" in image and image["mimeType"] == "image/png"
               for image in gltf["images"])
    schema = json.loads((ROOT / "schemas/command-result@1.schema.json").read_text())

    def cli(*arguments):
        result = subprocess.run([engine, *map(str, arguments), "--json"],
                                text=True, capture_output=True, timeout=120)
        assert result.returncode == 0, (arguments, result.stdout, result.stderr)
        document = json.loads(result.stdout)
        jsonschema.Draft7Validator(schema).validate(document)
        assert document["status"] == "ok", document
        return document["data"]

    times = []
    with tempfile.TemporaryDirectory(prefix="engine-large-normalmap-") as temporary:
        for repetition in range(args.repeat):
            project = Path(temporary) / str(repetition)
            cli("project", "create", project)
            start = time.perf_counter()
            imported = cli("asset", "import", project, SOURCE)
            elapsed = time.perf_counter() - start
            assert imported["cacheHit"] is False
            cache = project / imported["cachePath"]
            cooked = json.loads((cache / "cooked.json").read_text())
            mesh_bytes = (cache / "meshes.bin").read_bytes()
            count = 0
            materials = {m["id"]: m for m in cooked["materials"]}
            for mesh in cooked["meshes"]:
                for primitive in mesh["primitives"]:
                    assert "normal" in materials[primitive["materialId"]]["pbr"]["textures"]
                    count += primitive["indexCount"] // 3
                    # cooked@1 uses twelve little-endian floats per vertex:
                    # position, normal, tangent.xyzw, UV. Check every tangent.
                    for vertex in range(primitive["vertexCount"]):
                        tangent = struct.unpack_from(
                            "<4f", mesh_bytes, primitive["vertexOffset"] + vertex * 48 + 24)
                        assert all(math.isfinite(x) for x in tangent)
                        assert abs(sum(x * x for x in tangent[:3]) - 1) < 0.001
                        assert tangent[3] in (-1, 1)
            assert count == triangles, "cooking dropped geometry"
            assert cli("asset", "validate", project, imported["assetId"])["valid"] is True
            times.append(elapsed)
    print(json.dumps({"triangles": triangles, "sourceTangents": False,
                      "sha256": hashlib.sha256(raw).hexdigest(),
                      "uncachedImportSeconds": times,
                      "medianSeconds": statistics.median(times)}, indent=2))


if __name__ == "__main__":
    main()
