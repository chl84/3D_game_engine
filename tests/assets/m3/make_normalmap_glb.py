#!/usr/bin/env python3
"""Build a minimal normal-mapped triangle GLB without TANGENT attributes."""

from __future__ import annotations

import json
import struct
import zlib
from pathlib import Path

HERE = Path(__file__).resolve().parent


def flat_normal_png() -> bytes:
    width = height = 2
    rows = b"".join(b"\x00" + bytes([128, 128, 255, 255]) * width for _ in range(height))

    def chunk(kind: bytes, data: bytes) -> bytes:
        return (struct.pack(">I", len(data)) + kind + data +
                struct.pack(">I", zlib.crc32(kind + data) & 0xFFFFFFFF))

    header = struct.pack(">IIBBBBB", width, height, 8, 6, 0, 0, 0)
    return (b"\x89PNG\r\n\x1a\n" + chunk(b"IHDR", header) +
            chunk(b"IDAT", zlib.compress(rows)) + chunk(b"IEND", b""))


def main() -> None:
    positions = [0.0, 0.0, 0.0, 1.0, 0.0, 0.0, 0.0, 1.0, 0.0]
    normals = [0.0, 0.0, 1.0] * 3
    uvs = [0.0, 0.0, 1.0, 0.0, 0.0, 1.0]
    indices = [0, 1, 2]
    png = flat_normal_png()
    payloads = [
        struct.pack("<9f", *positions),
        struct.pack("<9f", *normals),
        struct.pack("<6f", *uvs),
        struct.pack("<3H", *indices),
        png,
    ]
    binary = bytearray()
    views = []
    for payload in payloads:
        while len(binary) % 4:
            binary.append(0)
        offset = len(binary)
        binary.extend(payload)
        views.append({"buffer": 0, "byteOffset": offset, "byteLength": len(payload)})
    while len(binary) % 4:
        binary.append(0)
    document = {
        "asset": {"version": "2.0", "generator": "engine M3 tangent fixture"},
        "scene": 0,
        "scenes": [{"name": "Scene", "nodes": [0]}],
        "nodes": [{"name": "Triangle", "mesh": 0}],
        "meshes": [{
            "name": "TriangleMesh",
            "primitives": [{
                "attributes": {"POSITION": 0, "NORMAL": 1, "TEXCOORD_0": 2},
                "indices": 3,
                "material": 0,
            }],
        }],
        "materials": [{
            "name": "NormalMapped",
            "pbrMetallicRoughness": {"baseColorFactor": [1.0, 1.0, 1.0, 1.0]},
            "normalTexture": {"index": 0},
        }],
        "textures": [{"source": 0, "sampler": 0}],
        "samplers": [{"magFilter": 9729, "minFilter": 9987}],
        "images": [{"bufferView": 4, "mimeType": "image/png"}],
        "accessors": [
            {"bufferView": 0, "componentType": 5126, "count": 3, "type": "VEC3",
             "min": [0.0, 0.0, 0.0], "max": [1.0, 1.0, 0.0]},
            {"bufferView": 1, "componentType": 5126, "count": 3, "type": "VEC3"},
            {"bufferView": 2, "componentType": 5126, "count": 3, "type": "VEC2"},
            {"bufferView": 3, "componentType": 5123, "count": 3, "type": "SCALAR"},
        ],
        "bufferViews": views,
        "buffers": [{"byteLength": len(binary)}],
    }
    encoded = json.dumps(document, separators=(",", ":")).encode()
    while len(encoded) % 4:
        encoded += b" "
    binary = bytes(binary)
    total = 12 + 8 + len(encoded) + 8 + len(binary)
    glb = (struct.pack("<4sII", b"glTF", 2, total) +
           struct.pack("<II", len(encoded), 0x4E4F534A) + encoded +
           struct.pack("<II", len(binary), 0x004E4942) + binary)
    (HERE / "normalmap.glb").write_bytes(glb)


if __name__ == "__main__":
    main()
