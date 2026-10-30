# Status report — 2026-10-30 — M3.4

**Delivered:** Asset CLI and approved official MikkTSpace cooking. This report
is retrospective for code `7cfad7a0f76d3abbba16e9851c8b7ceb1d273eb4`, not a claim
that the later PBR/golden work already existed at that commit. M3.4 was approved
by the independent reviewer in round 3. No M3 merge or tag is authorized.

| Function | Status | Evidence |
|---|---|---|
| `asset import/inspect/validate/reimport/list` through shared command handlers | Automatically tested | `engine_asset_cli` contract tests, including JSON/schema/error results, in the final CI below |
| Dry-run without project changes; locks and transaction rejection | Automatically tested | Import/reimport snapshot comparison; conflicting project lock returns E_CONFLICT; imports reject scene transactions before disk writes |
| Project sandbox and error atomicity | Automatically tested | Relative/absolute sourceCopy/cooked escapes and symlinked assets rejected; invalid source/GLB/extension leaves no import artifacts |
| Cache invalidation and stable IDs | Automatically tested | MikkTSpace importer `gltf-import@2`; old @1 cache rejected and reimported; stable asset/node IDs retained |
| Official MikkTSpace and tangent seams | Automatically tested | `genTangSpaceDefault`; generated per-corner data split by original index and complete tangent; non-finite/vertex-budget rejection; authored tangents preserved |
| License and exact source identity | Automatically tested | CMake checks the two SHA-256 values in the pinned MikkTSpace README and checks all three build-copy correction matches |
| CLI/API documentation | Automatically tested | Generated API reference, documentation-link and sensitive-data checks in final CI |
| Linux Clang/GCC14 | Compiled and automatically tested | 18/18 CTest, no skips in final CI |
| Linux ASan/UBSan | Automatically tested in reference CI | 18/18 in final CI, with no sanitizer suppression |
| Windows MSVC: compilation/headless/offscreen | Compiled and automatically tested | 13/13 including M2 offscreen golden/screenshot on installed lavapipe; this was not yet an M3 normal-map/PBR golden |
| Fuzz and Quality | Automatically tested | Both jobs success in final CI; permanent GLB/image fuzz targets use ASan and UBSan halt-on-error |
| Clean public clone | Automatically tested | Exact `7cfad7a`: 18/18, M1 acceptance and clean-clone acceptance passed; independently checked in review |

Final CI: [41041374885](https://github.com/chl84/3D_game_engine/actions/runs/41041374885),
exact `7cfad7a`. All six jobs succeeded: Linux Clang, GCC14, ASan/UBSan, Windows,
Fuzz, Quality. The earlier MSVC JSON/string_view ambiguity was fixed before this
run; failed earlier evidence is not substituted for the successful SHA.

## Tangent algorithm and provenance

This uses the official [mmikk/MikkTSpace](https://github.com/mmikk/MikkTSpace)
commit `3e895b49d05ea07e4c2133156cfa94369e19e409`, zlib, approved by the owner.
It is not a proprietary tangent algorithm or fallback. The C++ adapter supplies
positions/normals/UV0 to `genTangSpaceDefault` and converts the returned sign to
the importer's glTF/Blender convention. Vertex splitting preserves discontinuous
tangents/handedness rather than averaging them through the old index list.

The two upstream files remain byte-identical in the repository. The generated
build copy has three documented UB corrections: two safe 32-bit rotations and
one degenerate-grid guard. It is marked as altered, licensed and hash-pinned.
See [vendor provenance](../../third_party/mikktspace/README.md) and
[adapter](../../engine/asset/src/tangents.cpp).

The paired Blender exports first provided CPU tangent evidence. The later
owner-requested mirrored coupon and all PBR/golden evidence belong to M3.5;
see [its report](M3_5_STATUS_REPORT.md). Cache interpretation later advances to
`gltf-import@3` for selected-scene root preservation, without changing v1 schemas.

**Not verified / test limitations:** Windows presentation and interactive
physical Windows GPU behavior. M3 PBR normal-map and explicit color-space PNG
goldens were not run at M3.4; they are supplied and reported under M3.5.

**Known defects:** The full local sanitizer run at M3.4 was 12/18 with six
native graphics failures. Their 112-byte source is now proved to be Mesa RTASM
arena metadata, not engine allocations; see [diagnosis](M3_SANITIZER_DIAGNOSIS.md).
The supported local retention policy passes the current complete suite with
all sanitizer checks active. Reference CI was already unsuppressed and green.
No unresolved engine-owned leak has been identified by this investigation.

**Deviations from plan:** The normal-map rendering golden was delivered in
M3.5 rather than claimed from CPU tangent comparisons in M3.4. The original
vendored algorithm remains official MikkTSpace; only its generated build copy
contains the documented sanitizer corrections. Performance/reference-scene
measurements and optional nightly cached-corpus fuzzing remain later work.

**Decisions requiring approval:** None newly proposed. MikkTSpace was explicitly
approved. Merge, tag and milestone closure still require the owner.

**Next step:** Assess M3.5 with its separate report. M3.6 has not started and
will not start as part of this reporting/diagnosis follow-up.
