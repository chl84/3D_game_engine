# Try M3 with your Blender model

Run from the repository root with the M3 branch built. Use a separate test
project; your Blender source and export remain outside the engine project.
Commands below are for Bash and use Python 3 to read returned UUIDs.

## 1. Export from Blender

- Start with a static mesh near the origin, roughly 1–3 metres across. Apply
  object scale, provide UVs and normals, and connect textures to Principled BSDF.
- For a normal map, use a Non-Color image through a Normal Map node into Normal.
  Bake procedural materials to image textures before export.
- Select the model and export **glTF 2.0 → GLB**, with **Selected Objects**,
  UVs, normals and materials. Turn off animation, skinning, morph targets,
  Draco and mesh compression. Use embedded PNG/JPEG images and Opaque or Mask
  materials; transparent Blend materials are unsupported in M3.
- Leave **Tangents** off for the first test, to exercise engine generation.
  You can also export with Tangents enabled to compare normal-map shading.
  Skeletal animation, point lights and shadows are outside M3.

## 2. Create a project, camera and light

Set `MODEL` to your exported file. Choose a new `PROJECT` directory each time.

```bash
ENGINE="$PWD/build/linux-release/tools/engine/engine"
MODEL=/absolute/path/to/my-model.glb
PROJECT=/tmp/engine-m3-manual
id_from_json() { python3 -c 'import json,sys; print(json.load(sys.stdin)["data"]["result"]["id"])'; }

"$ENGINE" project create "$PROJECT" --name M3Manual --json
CAMERA=$("$ENGINE" entity create "$PROJECT" --name Camera --json | id_from_json)
"$ENGINE" component add "$PROJECT" --id "$CAMERA" --component engine.scene.Camera --json
"$ENGINE" component set "$PROJECT" --id "$CAMERA" --component engine.scene.Transform \
  --value '{"position":[0,1,5],"rotation":[0,0,0,1],"scale":[1,1,1]}' --json
SUN=$("$ENGINE" entity create "$PROJECT" --name Sun --json | id_from_json)
"$ENGINE" component add "$PROJECT" --id "$SUN" --component engine.scene.DirectionalLight --json
```

## 3. Import, place and run

```bash
ASSET=$("$ENGINE" asset import "$PROJECT" "$MODEL" --name MyModel --json \
  | python3 -c 'import json,sys; print(json.load(sys.stdin)["data"]["assetId"])')
"$ENGINE" asset inspect "$PROJECT" "$ASSET" --json
"$ENGINE" asset validate "$PROJECT" "$ASSET" --json
"$ENGINE" asset place "$PROJECT" scenes/main.scene.json \
  --asset "$ASSET" --position 0 0 0 --json
"$ENGINE" run "$PROJECT" scenes/main.scene.json
```

Use WASD, Q/E, Shift and right-mouse look; F1 toggles diagnostics, Escape closes.
Check scale/orientation, visible textures and lighting, and normal-map seams.
The camera starts at (0,1,5), looking along -Z; move back for a larger model.
Placement saves the scene automatically. Camera movement during `run` is temporary.

## 4. Change the model and reimport

Close `engine run`. Change the mesh or an image in Blender, and export over the
**same `MODEL` file**. Keep object names/hierarchy stable for stable node IDs.

```bash
"$ENGINE" asset reimport "$PROJECT" "$ASSET" --dry-run --json
"$ENGINE" asset reimport "$PROJECT" "$ASSET" --json
"$ENGINE" asset validate "$PROJECT" "$ASSET" --json
"$ENGINE" run "$PROJECT" scenes/main.scene.json
```

The asset UUID and saved placement must remain unchanged; the changed model
must appear after restart. A changed source should report `cacheHit: false`.
Reimport reads the original export while it exists; otherwise it uses the
project's source copy. M3 does not automatically reload a running window.
Optional before/after captures (run after closing the window):

```bash
"$ENGINE" screenshot "$PROJECT" scenes/main.scene.json --out screenshots/model.png \
  --width 1280 --height 720 --json
```

Report the Blender version, export settings, asset UUID, observed differences,
and any returned error JSON before approving the merge. Missing-tangent input
has a 1,000,000 dense-weld-pair budget per asset, not a triangle-count cap;
groups of at most 64 indexed corners are exempt. On a budget rejection,
export Blender tangents or simplify overlapping/high-valence geometry.
