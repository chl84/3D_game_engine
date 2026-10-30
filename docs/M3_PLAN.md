# M3 Plan — Asset pipeline and live control

**Status:** Approved by the owner on 2026-10-30 with the changes below. M3.1 is
reviewer-approved and CI-green. M3.2 (GLB import core) is reviewer-approved
(round 4): self-contained GLB import, checked-in Blender fixtures,
double-precision world composition, and explicit rejection of unsupported
features.

M2 is released as `v0.3.0`. M3 starts from `main` at `bb8514c`.

## Goal

Import a Blender-exported GLB without the editor, inspect and validate it,
place it in a scene, render it with basic PBR and one directional light, save
and load the scene again, and expose the same commands through a local live
API. Reimporting a changed source must preserve asset identity and must not
break scene references.

Acceptance from `VISION.md` M3:

- A script can import a model exported from Blender, inspect it, place it in a
  scene, render, save and load the scene again.
- After the model is changed and reimported, no references are broken.
- A golden image test of the scene passes.

Additional M2 decisions carried into M3:

- Basic PBR and one directional light in M3, so golden images verify glTF
  materials. Shadows and point lights move to M5 with the reference scene.
- Descriptor indexing is a minimum requirement (ADR-0011). There is no classic
  descriptor fallback.
- `ilammy/msvc-dev-cmd` is replaced in M3 before GitHub removes the old Node
  runtime.
- Windows offscreen graphics is verified with pinned lavapipe; Windows
  presentation remains not verified.

## Dependencies requiring approval

| Dependency | Version | License | Purpose | Status |
|---|---|---|---|---|
| fastgltf | 0.9.1 | MIT | glTF 2.0 / GLB parsing, accessors, materials and node hierarchy | **Approved 2026-10-30; add in M3.1** |
| simdjson (transitive) | 5.0.5 | `(Apache-2.0 OR MIT) AND BSL-1.0 AND BSD-3-Clause` | fastgltf's JSON parser | **Approved 2026-10-30** |
| stb_image | 2026-08-02 | MIT OR Unlicense | PNG/JPEG texture decoding | **Approved 2026-10-30 with PNG/JPEG-only macros, pre-decode limits and sanitizer tests** |

No other runtime dependency is proposed. The live API uses the existing
nlohmann/json and platform sockets. Compressed-texture codecs (KTX2/Basis),
IBL tools and animation runtimes are not added in M3.

The image decoder is `stb_image` from the already pinned **stb 2026-08-02**
package (`MIT OR Unlicense`, exact vcpkg commit). It is already in the build for
golden-image reading; M3 would use it only to decode PNG/JPEG textures during
asset cooking. The owner approved this M3 use on 2026-10-30 with these requirements:
`STBI_ONLY_PNG`, `STBI_ONLY_JPEG`; maximum width/height 8192; maximum decoded
RGBA allocation 256 MiB; `E_VALIDATION` before allocation when a limit is
exceeded; and tests for truncated, corrupt and oversized images under
ASan/UBSan. No new package is required.

`fastgltf` and `simdjson` are added to `vcpkg.json` and `docs/DEPENDENCIES.md`
in M3.1 after the 2026-10-30 approval. They are pinned through the existing
vcpkg baseline and exact overrides. The transitive `simdjson` license is
permissive and compatible with MIT.

## Proposed decisions requiring owner approval

### D1. Scene format v3 and asset instances

**Recommendation:** add an optional `engine.scene.AssetInstance` component and
bump the scene format to **3**.

```json
"engine.scene.AssetInstance": {
  "assetId": "5f0c2a2e-7d4b-4c1e-9a2f-1f3d4c5b6a70"
}
```

- A scene stores one `AssetInstance` entity with a `Transform`, a name and an
  `assetId`, not a copy of every node in the GLB.
- The asset hierarchy is resolved read-only at render time. Reimporting a
  changed source updates the asset content while the scene continues to
  reference the same `assetId`.
- Nodes inside an asset receive stable `nodeId` values that survive reimport.
  They are stored in `asset.json` and reported by `asset inspect`; overrides
  keyed by `nodeId` are not implemented in M3.
- `project.json` stays at `formatVersion` 1. Scene files move from 2 to 3.
  Version 2 loads and is upgraded on save. Version 1 continues through the
  existing migration. Version 4 and higher fail with `E_UNSUPPORTED_FORMAT`.
- The existing `engine.scene.Mesh` component remains for built-in primitives
  and direct mesh references.
- Scene format 3 also adds `engine.scene.DirectionalLight` and reserves
  `engine.scene.PointLight` now. M3 renders the directional light; M5 adds
  point lights and shadows without another scene-format version. A scene that
  contains point lights loads in M3 but the M3 renderer ignores them.

**Alternative considered:** expand every GLB node into scene entities at
placement. This avoids a scene-version bump but makes reimport dependent on
stable per-primitive mesh IDs and can leave a scene with stale references when
the source hierarchy changes. It is not recommended.

### D2. Asset sidecar and cooked format

**Recommendation:** introduce `asset@1` and `engine.asset.cooked@1`, both
versioned and owner-approved before use.

Proposed layout:

```text
assets/<assetId>/asset.json          # asset@1 sidecar
assets/<assetId>/source.glb          # project-relative source copy
.engine/cache/assets/<assetId>/<cacheKey>/
    cooked.json                      # engine.asset.cooked@1 manifest
    meshes.bin                       # little-endian mesh buffers
    textures.bin                     # RGBA8 texture data and metadata
```

- `asset.json` fields: `formatVersion`, `assetId`, `name`, `source` (the
  original source path for reimport), `sourceCopy` (project-relative source
  copy), `sourceHash` (SHA-256), `importerVersion`, `settings`, stable
  `nodeId`/mesh/material/texture IDs, dependency hashes, cooked key and
  `metadata`. The original source path is stored for reimport; the project-
  relative copy is the fallback when the original path is gone.
- `.engine/cache/` is added to `.gitignore`; cooked data is never committed.
- The first import generates and stores the asset UUID. Reimport reads the
  sidecar and preserves it. Tests must prove that an asset ID and a scene
  `AssetInstance` reference survive a source change.
- Cooked data is derived and regenerable. The cache key is the SHA-256 of the
  source bytes, importer version and import settings. Writes are atomic and
  sandboxed under the project root.
- Mesh buffers store Float32 positions/normals/tangents/UV0 and Uint32
  indices, with per-primitive material IDs and bounds. World transforms are
  composed in double precision.
- Node IDs are stable strings/UUIDs assigned on first import and matched by
  node path/index on reimport. They survive unchanged nodes and are available
  for later per-node overrides.
- Textures are decoded from PNG/JPEG with `stb_image` from the pinned stb
  package, once the explicit M3 texture-decoding approval is given, and cooked
  to RGBA8 with explicit sRGB/linear metadata. Mip generation happens
  at GPU upload. KTX2, WebP and other compressed formats fail with
  `E_UNSUPPORTED`.
- If a normal map has no tangents, tangents are generated during cooking.
  Otherwise the source tangents are preserved.
- Animations are **not imported** in M3. `asset inspect` only reports whether
  animation channels are present in the source; no animation data is cooked or
  played. Animation import/playback moves to M4.

**Alternative considered:** decode source textures on every load and skip a
cooked cache. This is simpler but does not satisfy dependency tracking,
deterministic cooking or the M3 cache requirement.

### D3. PBR material model and the M3 light

**Recommendation:** implement glTF metallic-roughness PBR with an
`engine.scene.DirectionalLight` component in scene format 3. Point lights and
shadows remain M5 features.

- Material inputs: base-color factor/texture, metallic/roughness
  factor/texture, normal texture, occlusion texture and emissive
  factor/texture.
- Alpha: opaque and mask are supported in M3. Blended transparency is
  deferred because it needs sorting and is not required for the M3 golden
  scene.
- An `engine.scene.DirectionalLight` component is added in scene format 3 now,
  with direction, linear color, intensity and enabled fields. The M3 golden
  scene uses one directional light and a small documented ambient term. No
  IBL/environment map in M3. `engine.scene.PointLight` is reserved in v3 now
  but is ignored by the M3 renderer; M5 adds point lights and shadows without
  a new scene-format version.
- Linear lighting, sRGB texture handling and a documented deterministic tone
  mapping. Golden images use the same authored directional light and tone map
  on Linux and Windows lavapipe.

The owner chose authored lighting now. The golden image test must verify the
color pipeline: base-color textures are decoded as sRGB, while normal,
metallic-roughness, occlusion and other data textures stay linear. The test
uses controlled texture values so a swapped color space fails visibly.

### D4. Live API transport and protocol

**Recommendation:** JSON-RPC 2.0 over a loopback-only TCP socket.

- Bind to `127.0.0.1` on an ephemeral port.
- Write a session file at `.engine/live.json` containing the port, token,
  protocol version and project root. It is sandboxed, written atomically and
  readable only by the owner: POSIX mode `0600`, and a Windows DACL restricted
  to the current user. The server refuses to start if it cannot enforce this.
- Require the token on every request. No remote interfaces, no `0.0.0.0`.
- Methods:
  - `command`: the existing command envelope and command bus.
  - `inspect`: project/scene/asset inspection.
  - `screenshot`: render and return a PNG. The proposed default is base64 in
    the JSON-RPC result, with an optional sandboxed output path.
  - `ping` and `shutdown`.
- Maximum message size, request timeout, structured errors and the same
  `command-result@1` payloads as the CLI.
- No new dependency: nlohmann/json and the existing platform socket/atomic
  file code are used.

**Alternative considered:** Unix domain sockets on Linux and named pipes on
Windows. This is more platform-specific code and does not improve the local
security model for M3.

### D5. CI tooling: replace `ilammy/msvc-dev-cmd`

Replace `ilammy/msvc-dev-cmd@v1` with a native PowerShell step that locates
Visual Studio with `vswhere`, invokes `vcvarsall.bat x64`, and exports the
environment to `GITHUB_ENV`/`GITHUB_PATH`. This is CI tooling only, with no
runtime dependency. It is done in M3 before GitHub removes the old Node
runtime.

## Substeps

### M3.1 Decisions, schemas and dependency gate

- Write the asset/scene ADR (D1/D2/D3) and the live API ADR (D4).
- Add the versioned asset and cooked schemas, scene v3 schema and migration
  tests.
- Add `fastgltf`/`simdjson` to vcpkg only after owner approval.
- Replace `ilammy/msvc-dev-cmd` with the native `vcvars` step and verify the
  Windows job.
- Exit: owner-approved ADRs and schemas; dependency pinned; CI builds with the
  new dependency; no asset feature code before approval.

### M3.2 GLB import core

**Status:** Implemented — reviewer pending.

- Parse GLB with fastgltf, including buffers, accessors, nodes, meshes,
  primitives, hierarchy, materials, textures and samplers. Detect animation
  channels and report that they exist, but do not import their data.
- Validate required attributes and reject unsupported extensions/features
  with stable error codes and JSON Pointer details.
- Convert glTF node transforms into engine double-precision world composition;
  keep rotation/scale float at the asset boundary.
- Exit: unit tests for checked-in GLB fixtures with documented provenance and
  license (own Blender model or CC0 with a `PROVENANCE.md`), malformed files,
  missing buffers, unsupported compression and hierarchy composition. Animation
  channels are reported by `inspect`, not imported.

### M3.3 Cooked asset format, cache and reimport

- Write `asset@1` sidecars and `engine.asset.cooked@1` cache entries.
- Implement source hashing, dependency tracking, cache invalidation, atomic
  writes and project sandboxing.
- Preserve the asset UUID on reimport and preserve stable node IDs for nodes
  that still exist. Overrides are not built in M3.
- Exit: import → cook → load round-trip is byte-stable; changing the source
  changes the cache key; reimport preserves the asset ID and stable node IDs;
  a scene `AssetInstance` reference remains valid; `.engine/cache/` is ignored
  by git.

### M3.4 Asset CLI

Add:

```text
engine asset import <project> <source> [--name NAME] [--json]
engine asset inspect <project> <assetId> [--json]
engine asset validate <project> <assetId> [--json]
engine asset reimport <project> <assetId> [--json]
engine asset list <project> [--json]
```

- All commands use `command-result@1`, the project sandbox and stable error
  codes.
- `inspect` reports meshes, primitives, materials, textures, bounds,
  dependencies and cooked-cache state.
- Exit: CLI contract tests and schemas; no absolute paths or writes outside the
  project.

### M3.5 PBR renderer, bindless assets and the M3 light

- Implement ADR-0011's one bindless descriptor set: texture array and material
  record storage buffer, 4096-image/4096-material budget, no fallback.
- Upload cooked meshes and textures; generate mips; handle sRGB/linear formats.
- Add the metallic-roughness shader with normal mapping, occlusion and
  emissive, plus the `DirectionalLight` component and documented tone map.
- Exit: validation-clean PBR scene on lavapipe Linux and Windows; descriptor
  budgets checked; golden image of a textured GLB passes with the authored
  directional light, and a color-space test proves sRGB baseColor versus linear
  data textures.

### M3.6 Asset instances and scene placement

- Add `engine.scene.AssetInstance` and scene format 3 with v1/v2 migrations.
- Add `engine asset place <project> <scene> --asset <assetId> [--name NAME]
  [--position X Y Z] [--json]`.
- Resolve asset hierarchies read-only at render time, with each instance
  sharing the same cooked asset and materials.
- Exit: import → place → render → save → load; reimport after changing the
  source preserves the asset ID and the scene still loads; two instances of the
  same asset render correctly.

### M3.7 Live API

- Implement the loopback JSON-RPC server and the CLI client/launcher.
- Expose command, inspect, screenshot, ping and shutdown.
- Add authentication, message-size limits, timeouts, structured errors and
  protocol-version checks.
- Exit: a Python script uses only the live API to import/list/inspect/place a
  GLB, save/load the scene, render and take a screenshot; all responses
  validate against the published schemas.

### M3.8 Hardening and M3 acceptance

- Run the Blender GLB acceptance script with checked-in models whose provenance
  and license are documented, and the reimport scenario.
- Run Linux GCC/Clang, ASan/UBSan, Windows MSVC with offscreen lavapipe, and
  the clean-clone acceptance.
- Add golden images for the PBR scene on Linux and Windows lavapipe.
- Update ADRs, `STATUS.md`, `TESTING.md`, `BUILD.md`, `DEPENDENCIES.md`,
  `PROJECT_FORMAT.md`, `API_REFERENCE.md` and `VISION.md` status.
- Reviewer approval, then owner approval for merge and the next tag.

## Acceptance criteria

M3 is complete only when all of these hold:

1. A script imports a Blender-exported GLB, inspects/validates it, places it in
   a scene, renders it, saves and loads the scene, all without the editor.
2. The rendered scene uses basic PBR materials and the documented one-
   directional-light model.
3. Changing the source and running `asset reimport` preserves the asset UUID;
   the existing scene `AssetInstance` still loads and references the updated
   asset with no broken IDs.
4. A golden image test of the PBR scene passes on Linux lavapipe and on the
   verified Windows offscreen lavapipe path, including a color-space check for
   sRGB baseColor and linear data textures.
5. The live API executes the same commands as the CLI, plus inspection and
   screenshot, over the approved local transport; all responses validate
   against the schemas.
6. Asset commands reject malformed or unsupported GLBs with stable error codes,
   JSON Pointer details and no writes outside the project.
7. The clean-clone script passes; Windows offscreen tests pass; ASan/UBSan and
   the documented TSan scope pass.
8. No new dependency or format is used before its owner approval, and the
   dependency/license/format docs are updated.
9. `ilammy/msvc-dev-cmd` is replaced by the native `vcvars` CI step.

## Testing

| Test | Purpose |
|---|---|
| GLB import unit tests | Accessors, buffers, node hierarchy, transforms, materials, textures, malformed/unsupported input |
| Cooked round-trip | Import → cook → load → cook is deterministic; cache keys and atomic writes |
| Reimport ID test | Source change preserves asset ID and scene references |
| Asset CLI contract | JSON envelopes, schemas, error codes, sandbox |
| PBR golden image | Textured GLB with the authored directional light on Linux and Windows lavapipe |
| Asset instance integration | Place, render, save, load, two instances, reimport |
| Live API integration | Python client using only JSON-RPC for import/place/save/load/screenshot |
| Windows offscreen | MSVC build and lavapipe offscreen tests |
| Clean clone | Build and full acceptance on a fresh clone |
| Sanitizers | ASan/UBSan on CPU-side importer/cooker/live API; TSan where meaningful |

## Risks

- **glTF variability:** real Blender exports may use unsupported extensions,
  compressed textures, skinning or animation. These must fail clearly, not
  silently misrender; animation channels are reported but not imported in M3.
- **Texture color spaces and tangents:** incorrect sRGB or tangent handling
  breaks PBR golden images. The cooked format records both explicitly, and the
  golden test includes a color-space probe.
- **Asset identity:** reimport must preserve the asset UUID and stable internal
  IDs. The scene's `AssetInstance` design avoids copying hierarchy into scenes.
- **Bindless limits:** ADR-0011 requires descriptor indexing; textures and
  material records must stay within the 4096 budgets and be validated on
  lavapipe and Iris Xe.
- **fastgltf/simdjson integration:** new dependency on Linux and Windows;
  licensing and transitive versions must be approved and pinned.
- **Live API security:** loopback-only binding, token authentication, message
  limits and sandboxing are mandatory.
- **Golden determinism:** authored directional light, fixed tone map, explicit
  texture color spaces and controlled mip generation.
- **Scope:** skeletal animation playback, compressed textures, IBL, shadows and
  point lights are out of scope for M3.

## Out of scope

- Skeletal animation playback (M4)
- Shadow maps, point lights and the reference scene (M5)
- IBL/environment maps and compressed texture formats (later milestone)
- Editor, gizmos and material authoring UI (M6)
- Network-exposed live API or remote clients

## Approval requested

1. Approve the M3 scope, substeps and acceptance criteria.
2. **Approved:** `fastgltf` 0.9.1 and transitive `simdjson` 5.0.5 are added in
   M3.1.
   **Approved:** `stb_image` from the pinned stb 2026-08-02 commit for M3
   PNG/JPEG texture decoding, with PNG/JPEG-only macros, pre-decode limits and
   ASan/UBSan tests for malformed images.
3. **Approved:** scene format **v3** with `engine.scene.AssetInstance` and
   `engine.scene.DirectionalLight`; `engine.scene.PointLight` is reserved in
   v3 so M5 needs no new format version.
4. **Approved:** the `asset@1` sidecar and `engine.asset.cooked@1` formats,
   including original source path, stable node IDs, asset UUID and cache
   layout. `.engine/cache/` is committed to `.gitignore`.
5. **Approved:** the PBR material model, `engine.scene.DirectionalLight` and
   opaque/mask alpha scope, with a golden color-space test.
6. **Approved:** loopback TCP JSON-RPC with token authentication, owner-only
   token-file permissions on Linux and Windows, and the proposed methods.
7. **Approved:** no compressed textures, IBL, shadows, point-light rendering or
   skeletal playback in M3; animations are reported but not imported.
8. **Approved:** replace `ilammy/msvc-dev-cmd` with native `vcvars` in M3.1.

M3.1 starts with the approved decisions. The `stb_image` texture-decoding use
is approved and documented. New ADRs record the approved decisions before the
corresponding code is written.
