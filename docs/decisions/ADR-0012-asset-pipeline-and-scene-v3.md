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
`E_UNSUPPORTED`, the codebase's error for unsupported versions.

### Import boundary

The M3 importer accepts one self-contained binary glTF (`.glb`) with an
embedded BIN chunk and embedded PNG/JPEG images. It rejects, with
`E_VALIDATION` or `E_UNSUPPORTED` and a JSON Pointer, anything it cannot
represent faithfully:

- external buffers or images, glTF JSON containers, and more than one scene
- any `extensionsUsed` or `extensionsRequired` entry
- skins, cameras, punctual lights, morph targets and non-triangle primitives
- non-indexed primitives; export indexed meshes instead. fastgltf's
  `GenerateMeshIndices` is not used because fastgltf 0.9.1 generates indices
  during parsing, before the importer can validate the POSITION reference or
  its count.
- blended transparency, unsupported vertex attributes and non-float vertex data
- truncated or malformed containers, hierarchy cycles and out-of-range indices

Base64 data URIs embedded in the GLB JSON are accepted because they are still
self-contained; the project-relative source copy remains the single file that
is hashed for the cache key. Every buffer, buffer view and accessor is checked
against the bytes actually present before any accessor is read, so a truncated
BIN chunk fails with `E_VALIDATION` instead of reading past the allocation.
Nodes outside the one imported scene remain in the node table for stable-ID
mapping in M3.3 but are not scene roots. The importer does not call
`fastgltf::validate()`: in fastgltf 0.9.1 it dereferences animation, accessor and
buffer-view indices without bounds checks. The importer checks every reference
it consumes, including animation channel and sampler indices, before reading
any data.

Source size is limited to 512 MiB, each encoded image to 64 MiB, primitives to
4M vertices and 16M indices, and hierarchy depth to 1024 nodes. Animation
channels are counted and reported; their data is not imported. Node paths are
derived from the name hierarchy, with the source index as the reimport
fallback until M3.3 assigns stable IDs.

### Asset format and cache

The project asset layout is:

```text
assets/<assetId>/asset.json          # asset@1
assets/<assetId>/source.glb          # project-relative source copy
.engine/cache/<assetId>/<cacheKey16>/
    cooked.json                      # engine.asset.cooked@1
    meshes.bin
    textures.bin
```

- `asset.json` stores `assetId`, the original `source` path for reimport, a
  project-relative `sourceCopy`, source SHA-256, importer version, import
  settings, stable node/mesh/material/texture IDs, dependencies, cooked-cache
  key and metadata. The original `source` is stored relative to the project
  even when it lives outside it (for example `../models/cube.glb`); an absolute
  path is used only when no relative path can be expressed, such as a different
  Windows drive. `sourceCopy` is always the fallback. The original `source`
  is read outside the project sandbox by design; a sidecar can therefore point
  reimport at any readable GLB. Only `sourceCopy` and cooked-cache paths are
  written or read through `PathSandbox`.
- `.engine/cache/` is ignored by git. Cooked data is derived and regenerable.
  Cache directories use the first 16 hex characters of the cache key to stay
  below the Windows path limit; the full SHA-256 key is stored in the sidecar
  and the cooked manifest.
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
- `stb_image` from the pinned stb 2026-08-02 commit is approved for PNG/JPEG
  texture decoding in M3.
- The decoder defines `STBI_ONLY_PNG` and `STBI_ONLY_JPEG`.
- Before decoding, the header is inspected. Width above 8192, height above
  8192 or a decoded RGBA allocation above 256 MiB fails with `E_VALIDATION`
  and the measured/limit values. No allocation happens before the check.
- Truncated, corrupt and oversized inputs are covered by tests under
  ASan/UBSan.

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
- The image decoder is limited to PNG/JPEG and has hard dimension/memory
  checks before allocation.
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

## M3.4 tangent-generation correction (owner approved 2026-10-30)

The former custom UV-derivative accumulation/orthogonalization code was not
MikkTSpace and did not guarantee Blender tangent-space compatibility. Use the
official implementation pinned to commit
`3e895b49d05ea07e4c2133156cfa94369e19e409` (zlib), stored unmodified under
`third_party/mikktspace`, with configure-time SHA-256 checks.

Cooking calls `genTangSpaceDefault` for normal-mapped primitives without source
tangents. Its results are per corner: rebuild indices and split vertices at
tangent/sign discontinuities instead of averaging into the input index list.
Retain the existing vertex limit after splitting. Preserve exported tangents.
The importer version becomes `gltf-import@2`; old caches require reimport.
The sidecar and cooked formats remain version 1.

CPU regression tests compare generated tangents to Blender-exported tangents
for an original Blender model with a baked normal map, plus mirrored UVs,
vertex splitting and degenerate UVs. The matching PBR golden image test is an
M3.5 requirement, once normal-map rendering exists; it must run on Linux and
Windows lavapipe and compare the export without tangents to both a fixed
golden image and the export with Blender tangents. It is not implemented by
the CPU comparison.

The checked-in upstream sources remain unmodified. The generated build copy
contains two documented correctness fixes: modulo-32 random-seed rotation and
a finite, zero-extent-safe vertex-grid cell calculation. These were found by
UBSan and the permanent GLB fuzzer; the adapter and collapsed-position regression
run with the sanitizer checks enabled. See the third-party README for details.
MikkTSpace's returned sign is inverted when stored in glTF tangent.w, matching
the glTF UV convention and Blender's exported tangent signs.

## M3.5 scene-root preservation

`metadata.sceneRoots` stores the imported scene's root UUIDs in the existing
free-form metadata object. Cooked loading restores those roots instead of losing
scene membership or treating unused parentless nodes as part of the scene.
`gltf-import@3` requires recooking older derived caches. No sidecar/cooked format
version changes; original sources and stable IDs remain intact. See ADR-0014.
