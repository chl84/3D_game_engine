# ADR-0012: Asset pipeline, scene format v3 and PBR lighting

- **Status:** Accepted
- **Date:** 2026-10-30

## Context

M3 imports Blender-exported GLB assets, cooks them into a cache, places them in
scenes and renders basic PBR. The owner approved the M3 plan with these
constraints:

- Scene format v3 adds `engine.scene.AssetInstance` and an authored
  `engine.scene.DirectionalLight`; M5 must not need another format version for
  point lights and shadows.
- Nodes inside an asset keep stable IDs across reimport so they can be used for
  later overrides. Overrides are not implemented in M3.
- `asset.json` stores the original source path for reimport; `.engine/cache/` is
  ignored by git.
- PBR uses sRGB for base-color textures and linear data for the remaining
  texture inputs. A golden test verifies the color pipeline.
- Animations are not imported in M3; `asset inspect` only reports that they
  exist.

## Decision

### Scene format v3

The scene format version becomes **3**. `project.json` stays at version 1.

New optional components:

- `engine.scene.AssetInstance` with `assetId`. The asset hierarchy is resolved
  read-only at render time; a scene stores one instance entity rather than a
  copy of every GLB node.
- `engine.scene.DirectionalLight` with `direction`, linear `color`, `intensity`
  and `enabled`.
- `engine.scene.PointLight` with linear `color`, `intensity`, `range` and
  `enabled`. It is reserved in v3 now so M5 adds point lights and shadows
  without a new scene version. The M3 renderer ignores point lights.

Version 1 and 2 scenes load and upgrade to v3 on save. Components introduced in
v3 are rejected when read from a v1 or v2 file. Version 4 and higher fail with
`E_UNSUPPORTED_FORMAT`.

### Asset format and cache

The project asset layout is:

```text
assets/<assetId>/asset.json          # asset@1
assets/<assetId>/source.glb          # project-relative source copy
.engine/cache/assets/<assetId>/<cacheKey>/
    cooked.json                      # engine.asset.cooked@1
    meshes.bin
    textures.bin
```

- `asset.json` stores `assetId`, the original `source` path for reimport, a
  project-relative `sourceCopy`, source SHA-256, importer version, import
  settings, stable node/mesh/material/texture IDs, dependencies, cooked-cache
  key and metadata.
- `.engine/cache/` is ignored by git. Cooked data is derived and regenerable.
- The cache key is the SHA-256 of source bytes, importer version and import
  settings. Writes are atomic and sandboxed.
- Node IDs are stable across reimport for nodes that still exist. Overrides are
  reserved for a later milestone.
- Mesh data is cooked as Float32 attributes and Uint32 indices. Textures are
  cooked as RGBA8 with explicit sRGB/linear metadata. Mip generation happens at
  GPU upload.

### PBR and lighting

- glTF metallic-roughness: base-color, metallic/roughness, normal, occlusion
  and emissive inputs.
- Base-color textures use sRGB. All other texture inputs, including emissive,
  normal, metallic-roughness and occlusion, use linear values, as approved by
  the owner.
- Opaque and alpha-mask materials are supported. Blended transparency is
  deferred because it requires sorting.
- One authored `DirectionalLight` plus a small fixed ambient term. No IBL or
  environment maps in M3. Shadows and point lights are M5.
- A golden image test uses controlled texture colors to fail visibly if an
  sRGB/linear conversion is wrong.

### Animations and image decoding

- Animation channels are detected and reported by `asset inspect`; no animation
  data is cooked or played in M3. Animation import/playback moves to M4.
- `stb_image` from the pinned stb 2026-08-02 commit is proposed for PNG/JPEG
  texture decoding. The owner must explicitly approve that M3 use before any
  texture-decode code is added. No other image decoder is proposed.

### Test models

Checked-in GLB fixtures must be own Blender models or CC0 models with a
`PROVENANCE.md` recording source, author, license and retrieval date. No
unlicensed model is committed.

## Consequences

- Scenes reference assets by stable UUID and are not rewritten when an asset is
  reimported.
- M5 can render point lights and shadows in scene v3 without a v4 migration.
- Asset identity and node identity have separate stability tests.
- The original source path is stored for reimport, while the project-relative
  copy keeps a clean clone self-contained when the original is unavailable.
- The M3 renderer ignores point lights until M5.
- The image decoder remains a separate approval gate even though `stb_image` is
  already in the build for golden-image reading.
- Scenes written by M3 cannot be read by the M2 release; this is explicit and
  versioned.

## Alternatives considered

- **Expand GLB nodes into scene entities at placement:** avoids a scene-version
  bump, but reimport can leave stale per-primitive references. Rejected.
- **Only DirectionalLight in v3, add PointLight in M5:** would require a new
  scene version under the project's versioning rule. Rejected.
- **Fixed engine light:** cannot be authored per scene and would need a format
  change later. Rejected by the owner.
- **Cook textures on every load:** no dependency tracking or deterministic
  cache. Rejected.
- **Import animations in M3:** expands the asset format and runtime before M4
  playback exists. Deferred.
