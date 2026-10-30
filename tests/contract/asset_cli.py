#!/usr/bin/env python3
"""Exercise the public asset CLI, error contract, dry-run and sandbox."""
from __future__ import annotations
import hashlib
import json
import shutil
import struct
import subprocess
import sys
import tempfile
from pathlib import Path
import jsonschema

ROOT = Path(__file__).resolve().parents[2]
FIXTURES = ROOT / "tests/assets/m3"
ENVELOPE = json.loads((ROOT / "schemas/command-result@1.schema.json").read_text())
DATA = json.loads((ROOT / "schemas/asset-result-data@1.schema.json").read_text())


def snapshot(root):
    return {p.relative_to(root).as_posix(): hashlib.sha256(p.read_bytes()).hexdigest()
            if p.is_file() else None for p in root.rglob("*")}


def main():
    engine = str(Path(sys.argv[1]).resolve())

    def run(*args, code=0, error=None):
        result = subprocess.run([engine, *map(str, args), "--json"], capture_output=True, text=True)
        assert result.returncode == code, (args, result.returncode, result.stdout, result.stderr)
        doc = json.loads(result.stdout)
        jsonschema.Draft7Validator(ENVELOPE).validate(doc)
        if error:
            assert doc["error"]["code"] == error, doc
        if code == 0 and args[0] == "asset":
            jsonschema.Draft7Validator(DATA).validate(doc["data"])
        return doc

    schemas = run("schema")["data"]["commands"]
    for name in ("import", "inspect", "validate", "reimport", "list"):
        assert "asset." + name + "@1" in schemas

    with tempfile.TemporaryDirectory(prefix="engine-asset-cli-") as tmp:
        tmp = Path(tmp)
        project = tmp / "project"
        run("project", "create", project, "--name", "Assets")
        source = tmp / "source.glb"
        shutil.copyfile(FIXTURES / "blender_cube.glb", source)
        before = snapshot(project)
        preview = run("asset", "import", project, source, "--name", "Cube", "--dry-run")
        assert preview["data"]["dryRun"] is True
        assert snapshot(project) == before
        assert run("asset", "list", project)["data"]["assets"] == []
        imported = run("asset", "import", project, source, "--name", "Cube")["data"]
        asset_id = imported["assetId"]
        assert imported["dryRun"] is False
        assert not Path(imported["sourceCopy"]).is_absolute()
        assert not Path(imported["cachePath"]).is_absolute()
        inspected = run("asset", "inspect", project, asset_id)["data"]
        assert inspected["name"] == "Cube"
        assert inspected["meshes"] and inspected["materials"] and inspected["textures"]
        assert inspected["cache"]["state"] == "ready"
        assert inspected["meshes"][0]["primitives"][0]["bounds"]["min"]
        assert run("asset", "validate", project, asset_id)["data"]["valid"] is True
        assert len(run("asset", "list", project)["data"]["assets"]) == 1
        before = snapshot(project)
        run("asset", "reimport", project, asset_id, "--dry-run")
        assert snapshot(project) == before
        assert run("asset", "reimport", project, asset_id)["data"]["cacheHit"] is True

        lock = project / ".engine/lock"
        lock.mkdir()
        run("asset", "reimport", project, asset_id, code=5, error="E_CONFLICT")
        lock.rmdir()

        # Emissive and baseColor may share an sRGB texture. The same texture
        # cannot also serve linear data. Exercise cooking, not just GPU mocks.
        raw_color = source.read_bytes()
        color_length = struct.unpack_from("<I", raw_color, 12)[0]
        color_doc = json.loads(raw_color[20:20 + color_length])
        color_doc["materials"][0]["emissiveTexture"] = {"index": 0}
        color_doc["materials"][0]["emissiveFactor"] = [1, 1, 1]
        emissive_source = tmp / "emissive.glb"

        def write_color_source():
            encoded = json.dumps(color_doc, separators=(",", ":")).encode()
            encoded += b" " * (-len(encoded) % 4)
            payload = struct.pack("<II", len(encoded), 0x4E4F534A) + encoded + raw_color[20 + color_length:]
            emissive_source.write_bytes(struct.pack("<III", 0x46546C67, 2, len(payload) + 12) + payload)

        write_color_source()
        emissive = run("asset", "import", project, emissive_source)["data"]
        emissive_info = run("asset", "inspect", project, emissive["assetId"])["data"]
        assert emissive_info["textures"][0]["colorSpace"] == "srgb"
        emissive_cooked = json.loads((project / emissive["cachePath"] / "cooked.json").read_text())
        assert emissive_cooked["textures"][0]["colorSpace"] == "srgb"
        color_doc["materials"][0]["occlusionTexture"] = {"index": 0}
        write_color_source()
        before = snapshot(project)
        run("asset", "import", project, emissive_source, code=3, error="E_VALIDATION")
        assert snapshot(project) == before

        # Simulate the previous emissive-linear cache; reimport must repair it.
        sidecar_path = project / "assets" / asset_id / "asset.json"
        legacy = json.loads(sidecar_path.read_text())
        old_key = hashlib.sha256((legacy["sourceHash"] + "|gltf-import@3|{}").encode()).hexdigest()
        old_path = ".engine/cache/" + asset_id + "/" + old_key[:16]
        shutil.move(str(project / legacy["cooked"]["path"]), str(project / old_path))
        legacy["importerVersion"] = "gltf-import@3"
        legacy["cooked"] = {"key": old_key, "path": old_path}
        sidecar_path.write_text(json.dumps(legacy))
        cooked_path = project / old_path / "cooked.json"
        cooked = json.loads(cooked_path.read_text())
        cooked["importerVersion"] = "gltf-import@3"
        cooked["cacheKey"] = old_key
        cooked_path.write_text(json.dumps(cooked))
        run("asset", "validate", project, asset_id, code=3, error="E_VALIDATION")
        rebuilt = run("asset", "reimport", project, asset_id)["data"]
        assert rebuilt["assetId"] == asset_id and rebuilt["cacheHit"] is False
        assert rebuilt["cacheKey"] == imported["cacheKey"]

        # Edit valid GLB JSON without changing the buffer: source change must
        # invalidate cache but preserve the asset/internal node references.
        raw = source.read_bytes()
        length = struct.unpack_from("<I", raw, 12)[0]
        gltf = json.loads(raw[20:20 + length])
        unsupported = tmp / "unsupported.glb"
        extension_doc = dict(gltf, extensionsUsed=["KHR_draco_mesh_compression"])
        extension_json = json.dumps(extension_doc, separators=(",", ":")).encode()
        extension_json += b" " * (-len(extension_json) % 4)
        extension_payload = struct.pack("<II", len(extension_json), 0x4E4F534A) + extension_json + raw[20 + length:]
        unsupported.write_bytes(struct.pack("<III", 0x46546C67, 2, len(extension_payload) + 12) + extension_payload)
        before = snapshot(project)
        run("asset", "import", project, unsupported, code=7, error="E_UNSUPPORTED")
        assert snapshot(project) == before
        gltf["nodes"][0]["translation"] = [0.125, 0.0, 0.0]
        encoded = json.dumps(gltf, separators=(",", ":")).encode()
        encoded += b" " * (-len(encoded) % 4)
        payload = struct.pack("<II", len(encoded), 0x4E4F534A) + encoded + raw[20 + length:]
        source.write_bytes(struct.pack("<III", 0x46546C67, 2, len(payload) + 12) + payload)
        updated = run("asset", "reimport", project, asset_id)["data"]
        assert updated["assetId"] == asset_id and updated["cacheHit"] is False
        assert updated["cacheKey"] != imported["cacheKey"]
        assert run("asset", "inspect", project, asset_id)["data"]["nodes"] == inspected["nodes"]
        source.unlink()
        run("asset", "reimport", project, asset_id)  # Uses portable source copy.

        run("asset", "inspect", project, "invalid", code=3, error="E_VALIDATION")
        run("asset", "inspect", project, "00000000-0000-4000-8000-000000000001", code=4, error="E_NOT_FOUND")
        malformed = tmp / "bad.glb"
        malformed.write_bytes(b"broken GLB")
        before = snapshot(project)
        run("asset", "import", project, malformed, code=3, error="E_VALIDATION")
        assert snapshot(project) == before
        run("asset", "import", project, tmp / "missing.glb", code=4, error="E_NOT_FOUND")

        # Missing cache is inspectable, but validate must fail; reimport rebuilds.
        shutil.rmtree(project / updated["cachePath"])
        unavailable = run("asset", "inspect", project, asset_id)
        assert unavailable["data"]["cache"]["state"] == "unavailable" and unavailable["warnings"]
        run("asset", "validate", project, asset_id, code=4, error="E_NOT_FOUND")
        run("asset", "reimport", project, asset_id)

        sidecar = project / "assets" / asset_id / "asset.json"
        original = json.loads(sidecar.read_text())
        for field in ("sourceCopy", "cooked.path"):
            for path in ("../../outside", str(tmp / "outside")):
                changed = json.loads(json.dumps(original))
                if field == "sourceCopy": changed[field] = path
                else: changed["cooked"]["path"] = path
                sidecar.write_text(json.dumps(changed))
                for operation in ("inspect", "validate", "reimport"):
                    run("asset", operation, project, asset_id, code=3, error="E_VALIDATION")
                assert not (tmp / "outside").exists()
        sidecar.write_text(json.dumps(original))
        original["assetId"] = "00000000-0000-4000-8000-000000000001"
        sidecar.write_text(json.dumps(original))
        run("asset", "inspect", project, asset_id, code=3, error="E_VALIDATION")

        animated = run("asset", "import", project, FIXTURES / "blender_animated.glb")["data"]
        assert animated["animationChannelCount"] > 0
        assert run("asset", "inspect", project, animated["assetId"])["warnings"]

        # Directory symlinks are checked where supported (no skip of the suite).
        linked = tmp / "linked"
        run("project", "create", linked)
        outside = tmp / "external-assets"
        outside.mkdir()
        try:
            (linked / "assets").rmdir()
            (linked / "assets").symlink_to(outside, target_is_directory=True)
        except OSError:
            print("NOTE: directory symlink creation unavailable on this platform")
        else:
            run("asset", "import", linked, FIXTURES / "blender_cube.glb", code=3, error="E_VALIDATION")
            assert not list(outside.iterdir())
    print("Asset CLI contract passed")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
