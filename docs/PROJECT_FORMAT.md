# Project and Scene Format

**Status:** Implemented in M1.5 and extended with `engine.scene.Motion` in
M1.7 and `engine.scene.Mesh` plus scene format version 2 in M2.4. The project
and scene readers/writers, canonical JSON, atomic saves, project locks,
round-trip tests and `scene validate` exist. Generated JSON Schemas are
completed in M1.8.

## Principles

- Plain text, UTF-8, LF line endings and no BOM.
- Canonical JSON: sorted object keys, stable array order, two-space
  indentation and a final newline.
- `formatVersion` is an integer and controls migration.
- Every entity, asset and prefab has a UUID.
- Derived data is not stored: world matrices, cache paths and runtime handles
  are not part of the file.
- Unknown fields are rejected, except `metadata`, which is an open object for
  tools and AI.
- All paths are relative to the project root and use `/`.
- Saving is atomic; an interrupted write must never leave a half-written file.
- Writing requires an exclusive lock; reading may happen without a lock.

## Coordinate system and units

| Element | Choice |
|---|---|
| Handedness | Right-handed |
| Up | +Y |
| Forward | −Z |
| Length | Metres |
| Position | Double-precision metres in files and in `Transform` |
| Rotation | Radians in the API; quaternion `(x, y, z, w)` in files, float |
| Scale | Unitless, float |
| Projection | Vulkan clip space applies a Y flip in the projection matrix, not in the data |

This matches glTF, which simplifies asset import.

## `project.json`

```json
{
  "formatVersion": 1,
  "projectId": "3f2a7c1e-9b4d-4e6a-8c1f-2d5b7a9e0c31",
  "name": "Hello World",
  "engineVersion": "0.2.1",
  "scenes": [
    "scenes/main.scene.json"
  ],
  "assetRoot": "assets",
  "scriptRoot": "scripts",
  "settings": {
    "defaultScene": "scenes/main.scene.json",
    "tickRate": 60,
    "window": {
      "width": 1920,
      "height": 1080,
      "mode": "windowed"
    }
  },
  "metadata": {}
}
```

- `scenes` is sorted and contains relative paths.
- `engineVersion` is informative; `formatVersion` is authoritative.
- `tickRate` is 1–1000; the default is 60.
- `window.mode` is `windowed`, `borderless` or `fullscreen`.

## `scenes/main.scene.json`

```json
{
  "formatVersion": 2,
  "sceneId": "7c1e9b4d-4e6a-4c1f-8d2b-5a9e0c313f2a",
  "name": "main",
  "revision": 42,
  "entities": [
    {
      "id": "a1b2c3d4-4e5f-4a6b-8c7d-9e0f1a2b3c4d",
      "name": "Root",
      "parentId": null,
      "components": {
        "engine.scene.Transform": {
          "position": [0.0, 0.0, 0.0],
          "rotation": [0.0, 0.0, 0.0, 1.0],
          "scale": [1.0, 1.0, 1.0]
        }
      },
      "metadata": {}
    },
    {
      "id": "b2c3d4e5-4f6a-4b7c-8d9e-0f1a2b3c4d5e",
      "name": "Player",
      "parentId": "a1b2c3d4-4e5f-4a6b-8c7d-9e0f1a2b3c4d",
      "components": {
        "engine.scene.Transform": {
          "position": [0.0, 1.7, 0.0],
          "rotation": [0.0, 0.0, 0.0, 1.0],
          "scale": [1.0, 1.0, 1.0]
        },
        "engine.scene.Mesh": {
          "meshId": "engine.builtin.cube"
        }
      },
      "metadata": {}
    },
    {
      "id": "c3d4e5f6-4a7b-4c8d-8e9f-1a2b3c4d5e6f",
      "name": "Main Camera",
      "parentId": "a1b2c3d4-4e5f-4a6b-8c7d-9e0f1a2b3c4d",
      "components": {
        "engine.scene.Camera": {
          "projection": "perspective",
          "fovYRadians": 1.0471975511965976,
          "near": 0.1,
          "far": 1000.0
        },
        "engine.scene.Transform": {
          "position": [0.0, 1.7, 0.0],
          "rotation": [0.0, 0.0, 0.0, 1.0],
          "scale": [1.0, 1.0, 1.0]
        }
      },
      "metadata": {}
    }
  ],
  "metadata": {}
}
```

- Entities are sorted ascending by `id`.
- Component keys are sorted lexicographically.
- `parentId` is `null` for root entities.
- `revision` increases on every mutation and is used for optimistic
  concurrency.
- `engine.scene.Camera` is a normal component. It does not affect M1
  simulation, but makes the format ready for M2.
- `engine.scene.Motion` has a `linearVelocity` Vec3 and is read by the
  fixed-tick simulation. Simulation mutates runtime transforms without saving
  the scene; `engine simulate` returns a state dump instead.
- `engine.scene.Transform.position` is stored with double precision. Rotation
  and scale are float. World composition and camera-relative conversion happen
  in double until the renderer creates its float matrices.
- `engine.scene.Mesh` stores only a `meshId` string. The value is an asset UUID
  (resolved by the asset pipeline from M3) or a fixed built-in ID:
  `engine.builtin.cube` or `engine.builtin.plane`. Mesh vertices and indices
  are never stored in the scene file.

## Canonical form

- Object keys are sorted lexicographically.
- `entities` are sorted by `id`.
- `scenes` are sorted.
- Arrays representing vectors and quaternions have a fixed order.
- Floating-point values use the shortest round-trip representation; `NaN` and
  `Infinity` are rejected.
- Integers are written without decimals; floating-point values use `.0` when
  needed to preserve the type.
- Dates are written as UTC ISO 8601 with milliseconds.
- Files end with one `\n`.
- The round-trip test saves, loads and saves again and compares byte for byte.

## Versioning and migration

- `project.json` uses `formatVersion` 1. Scene files use `formatVersion` 2;
  the two formats are versioned independently.
- Scene format 1 loads and is upgraded to 2 when saved. Scene format 1 may not
  contain `engine.scene.Mesh`.
- Scene format 2 adds the optional `engine.scene.Mesh` component.
- On load:
  - equal to current: validate and use
  - lower: run the additive migration `vN → vN+1 → …`
  - higher: `E_UNSUPPORTED_FORMAT`
- Migrations are pure JSON→JSON functions with their own tests.
- Unknown fields are rejected; `metadata` is preserved.
- New format versions require an ADR and owner approval. Scene format 2 is
  recorded in ADR-0009.
- No automatic "best effort" migration that can lose data.

## Paths, security and locking

- `ProjectRoot` is canonical.
- All paths must stay under the root; absolute paths, `..`, drive letters and
  UNC paths are rejected.
- Symlinks that point outside the root are rejected.
- Case-insensitive collisions are tested on Windows.
- `.engine/lock` is an exclusive lock with
  `{pid, host, sessionId, startedAt}`.
- Orphaned locks are detected by PID/session and can be cleared with
  `engine project unlock --force`.
- The live server holds the lock while it runs.
- A scene may be read without a lock because saving is atomic.

## Atomic saves

1. Write to a temporary file in the same directory.
2. `fsync` the file.
3. Replace the file atomically:
   - POSIX: `rename`
   - Windows: `ReplaceFileW` or `MoveFileExW`
4. `fsync` the directory on POSIX.
5. On failure: delete the temporary file; the original is untouched.

This is implemented in `platform` and tested on both platforms.

## Assets, prefabs and metadata

- Assets get a UUID and a sidecar with import settings, source hash and a
  cooked-cache key (M3).
- Cooked data lives under `.engine/cache/` and can be regenerated.
- Prefabs are reserved in the format; instantiation comes later.
- `metadata` is an open JSON object on the project, scene, entity and
  component. It is used by tools and AI and is preserved without being
  interpreted.
- No absolute asset paths.

## Schemas and validation

- `schemas/project.schema.json` and `schemas/scene.schema.json` are generated
  from reflection and hand-written envelopes.
- `engine scene validate --json` validates both the schema and semantics:
  duplicate UUIDs, orphaned parents, cycles and unknown component types.
- Semantic errors include a JSON Pointer to the location in the file.
- Schemas are versioned with an `$id` that contains the format version.

## M1 acceptance

M1 must demonstrate that a CLI-only script can create a project, build a
scene with an entity hierarchy, change components, undo, save and load again.
Saving again must produce byte-identical files, and all CLI responses must
validate against their schemas.
