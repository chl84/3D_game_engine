#!/usr/bin/env python3
"""Verify the checked-in mirrored-UV fixture without Blender or extra packages.

Run: python3 tests/assets/m3/blender/verify_mirrored_normalmap.py
Checks the delivered repository files, not a generator's temporary outputs.
"""
from pathlib import Path
import collections
import json
import struct

out = Path(__file__).resolve().parents[1]
normal_png = (out / 'blender_mirrored_normal.png').read_bytes()
for filename in ('blender_mirrored_normalmap.blend', 'blender_mirrored_normal.png',
                 'blender_mirrored_no_tangents.glb', 'blender_mirrored_tangents.glb'):
    p = out / filename
    data = p.read_bytes()
    for marker in (b'/home/', Path.home().as_posix().encode(), b'@users.noreply'):
        assert marker not in data, (p.name, marker)
    print(p.name, len(data), 'bytes; no personal-path markers')
    if p.suffix != '.glb':
        continue
    assert struct.unpack_from('<III', data) == (0x46546C67, 2, len(data))
    size = struct.unpack_from('<I', data, 12)[0]
    gltf = json.loads(data[20:20 + size])
    blob = data[28 + size:]
    assert not gltf.get('animations')
    assert not gltf.get('cameras')
    assert not gltf.get('extensionsUsed')
    assert len(gltf['meshes']) == 2
    assert all('bufferView' in image and 'uri' not in image for image in gltf['images'])
    assert len(gltf['images']) == 1
    view = gltf['bufferViews'][gltf['images'][0]['bufferView']]
    offset = view.get('byteOffset', 0)
    assert blob[offset:offset + view['byteLength']] == normal_png
    normal_primitives = 0

    def accessor(n):
        a = gltf['accessors'][n]
        v = gltf['bufferViews'][a['bufferView']]
        count = {'SCALAR': 1, 'VEC2': 2, 'VEC3': 3, 'VEC4': 4}[a['type']]
        scalar = {5126: 'f', 5125: 'I', 5123: 'H'}[a['componentType']]
        fmt = '<' + scalar * count
        stride = v.get('byteStride', struct.calcsize(fmt))
        offset = v.get('byteOffset', 0) + a.get('byteOffset', 0)
        return [struct.unpack_from(fmt, blob, offset + i * stride) for i in range(a['count'])]

    for mesh in gltf['meshes']:
        prim = mesh['primitives'][0]
        material = gltf['materials'][prim['material']]
        if 'normalTexture' not in material:
            continue
        normal_primitives += 1
        assert prim.get('mode', 4) == 4
        attrs = prim['attributes']
        coords = accessor(attrs['TEXCOORD_0'])
        indices = [i[0] for i in accessor(prim['indices'])]
        signs = collections.Counter()
        island_u = {'positive': [], 'negative': []}
        for i in range(0, len(indices), 3):
            a, b, c = [coords[k] for k in indices[i:i + 3]]
            area = (b[0] - a[0]) * (c[1] - a[1]) - (b[1] - a[1]) * (c[0] - a[0])
            assert abs(area) > 1e-8
            orientation = 'positive' if area > 0 else 'negative'
            signs[orientation] += 1
            island_u[orientation].extend(v[0] for v in (a, b, c))
        assert signs == {'positive': 512, 'negative': 512}, signs
        assert (max(island_u['positive']) < min(island_u['negative']) or
                max(island_u['negative']) < min(island_u['positive']))
        assert ('TANGENT' in attrs) == ('no_tangents' not in p.stem)
        if 'TANGENT' in attrs:
            assert {round(t[3]) for t in accessor(attrs['TANGENT'])} == {-1, 1}
        print('GLB normal-mapped primitive:', len(indices) // 3,
              'triangles, UV orientations', dict(signs),
              'TANGENT present:', 'TANGENT' in attrs)
    assert normal_primitives == 1
print('Mirrored GLB fixture verification passed')
