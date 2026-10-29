# ADR-0009: Scene format v2 and mesh references

- **Status:** Accepted
- **Date:** 2026-10-29

## Context

M2.4 needs a `Mesh` component. The existing scene format is `formatVersion`
1 and rejects unknown components. Adding `engine.scene.Mesh` without a version
bump would make new scenes fail in older readers and would blur the meaning of
the version field. The owner requires an additive, versioned change that M3
does not have to break, and the mesh component must reference a mesh by asset
ID or a fixed built-in primitive ID rather than storing mesh data in the scene
file.

## Decision

- The scene format version becomes **2**. `project.json` stays at
  `formatVersion` 1; the scene format is versioned independently.
- A scene with `formatVersion` 1 still loads. Saving it writes version 2. A
  version-1 scene may not contain `engine.scene.Mesh`.
- `formatVersion` 2 adds the optional component `engine.scene.Mesh` with one
  string field, `meshId`. Mesh data never appears in a scene file.
- `meshId` is either an asset UUID (resolved by the asset pipeline in M3) or
  one of the fixed built-in IDs `engine.builtin.cube` and
  `engine.builtin.plane`.
- M2.4 resolves the two built-ins in the renderer. Unknown IDs fail with
  `E_UNSUPPORTED` and the ID in the error details; M3 adds asset resolution
  without changing the scene format.
- Versions below 1 or above 2 fail with `E_UNSUPPORTED`. The v1 → v2 path is a
  pure additive migration; no entity or component data changes.

The owner approved the M2.4 requirement, including this additive, versioned
format change, on 2026-10-29.

## Consequences

- Scenes written by M2 cannot be read by the M1 release, because the format
  version is higher. This is explicit and detected, not a silent parse failure.
- M2 and later can read both v1 and v2 scenes; saving a v1 scene upgrades it.
- The renderer and future asset pipeline share one component contract; no mesh
  vertex data is duplicated in scene files.
- M3 adds asset-ID resolution behind the same `meshId` field.
- Local `Transform` positions are stored as double. Rotation and scale remain
  float. A root entity placed directly at 8 km or 16 km therefore keeps
  sub-millimetre position precision, and world composition and camera-relative
  conversion stay in double until the final float model matrix. M5's
  large-world work can add chunk-relative transforms on top without changing
  the v2 field types.

## Alternatives considered

- **Add the component without a version bump:** rejected because version-1
  readers reject unknown components and the change would not be versioned.
- **Store mesh vertices and indices in the scene:** rejected because it bloats
  files, duplicates asset data and violates the derived-data principle.
- **Put the mesh reference in `metadata`:** rejected because metadata is an
  open tool bag, not an engine component with reflection and schema support.
- **Version only the component name (`engine.scene.Mesh@1`):** rejected because
  the scene format already has a version field and old readers reject unknown
  component names regardless of a suffix.
