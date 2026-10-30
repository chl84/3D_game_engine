# Status report — 2026-10-30 — M3.6

**Delivered:** Owner-requested sRGB emissive correction and removal of the
inactive LSan rule, followed by asset instance placement and project asset
resolution for window/offscreen rendering. M3.4/M3.5 and the pinned MikkTSpace
implementation/marked UB-corrected build copy are owner-approved. No merge/tag
or M3.7 work is included.

| Function | Status | Evidence |
|---|---|---|
| Emissive sRGB cooking/cache invalidation | Automatically tested | asset CLI requires sRGB sidecar/cooked metadata with emissive alone and shared baseColor/emissive; importer@4 replaces @3 derived cache; mixed emissive/data use rejected |
| Corrected color PNG and linear negative control | Automatically tested on Linux/Windows lavapipe | Fixed chart emissive 117; linear control 156; omitted cooking classification and linear GPU-input mutations both fail |
| Shared asset.place@1 handler and CLI | Automatically tested | Asset existence/cache validation before scene mutation, name/position defaults, finite positions, unique and seed-deterministic entity IDs, double position precision |
| Placement undo/redo/transaction rollback | Automatically tested | CPU command tests; public batch undo and failed batch preserve the saved scene |
| Delete/undo of placed instance | Automatically tested | Snapshot restores AssetInstance; resulting rendered image equals pre-delete image |
| One reference entity per instance, v3 round-trip | Automatically tested | Existing v1/v2 migration and v3 component tests; public acceptance saves only camera plus two instances, without expanding GLB nodes |
| Read-only project resolution/resource sharing | Automatically tested | Fresh-process screenshot loads each UUID once (duplicate upload would fail); two image halves contain visible geometry; reload is pixel-identical |
| Changed-source reimport | Automatically tested | Node translation changes image; asset UUID and scene bytes stay unchanged; fresh reload matches changed output |
| Missing cache and repair | Automatically tested | Render fails with assetId without any project write; explicit reimport restores identical output |
| Windowed CLI with project assets | Automatically tested on local Linux lavapipe | Three hidden-window frames with two instances; no scene writes |
| Local Clang and ASan/UBSan | Automatically tested | Full 22/22 results and commands in M3_6_LOCAL_EVIDENCE.txt; sanitizers use retention only, no suppression file |
| Physical Iris Xe | Automatically tested | Complete public placement acceptance, including three hidden-window frames, passes with Intel ICD |
| Windows MSVC and lavapipe | Compiled and automatically tested on `ef6775a` | 16/16 including uppercase/mixed-case placement and corrected emissive PNG; CI 41076353145 Windows job success |
| Independent reviewer | Manually verified | Claude Opus 5.5 high GODKJENT on `4ce8cac`; one low UUID identity finding corrected locally; the small correction is separately verified by regressions and final CI |

[Permanent local evidence](M3_6_LOCAL_EVIDENCE.txt),
[independent review record](M3_6_REVIEW_SUMMARY.txt),
[CI excerpts](M3_6_CI_EVIDENCE.txt).

The correction is commit850a9e1; the additional emissive-only regression is part
of M3.6. ADR-0012/0014 now specify sRGB baseColor/emissive and linear normal/MR/AO.
Material factors remain linear. The fixed Blender normal-map golden is unchanged.
No existing test tolerance or skip rule is weakened. The removed inactive rule
never produced the local sanitizer pass; retention was already responsible.

The CLI composition root uploads only scene-referenced assets. PbrRenderer
continues to traverse their cooked hierarchy read-only and share mesh/material/
texture resources. Runtime rendering/import algorithms are otherwise unchanged.
Placement uses the scene's UUID generator so seeded command replay remains
reproducible; redo restores the same instance UUID. The asset module uses its
already-approved scene dependency; no module boundary or new library is added.

**Not verified / test limitations:** Windows swapchain
presentation, reference-scene FPS/whole-process RAM, live synchronization and
streaming. The acceptance test requires a Vulkan device/lavapipe and a Linux
display (CI uses Xvfb); it has no device-unavailable skip. Retention does not
test native driver unload/reload or eliminate driver-global LSan roots.

**Known defects:** Mesa RTASM unload metadata leak is diagnosed in the existing
sanitizer report. No new engine-owned leak is identified. Ordinary local
unload behavior is not claimed clean. Reference Ubuntu CI remains unsuppressed.

**Deviations from plan:** AssetInstance and scene-format3 migration existed
from M3.1; M3.6 reuses them. The new work is the undoable placement command and
project resolution/acceptance. No breaking schema/API or new dependency.

**Decisions requiring approval:** None newly proposed. Technical review approved
M3.6; post-correction exact-SHA CI is complete/success. Owner assessment of
M3.6, merge/tag and starting M3.7 remain with the owner.

**Next step:** Owner assessment of the consolidated M3.6 report. Review findings
are handled and all required verification passed. Do not start M3.7 without
owner authorization.

## Independent review and small follow-up

The reviewer approved `4ce8cac` with no blocking findings and independently
reproduced Linux 22/22, local ASan/UBSan22/22 without a suppression file, all eight
negative mutations and a fatal77-byte injected application leak. Initial
CI 41073848866 completed/success with all six jobs (Linux 22/22, Windows 16/16).
See [committed CI excerpts](M3_6_CI_EVIDENCE.txt).

L1 is corrected: project upload deduplicates parsed UUIDs in canonical form,
and PBR uses the same normalized identity for scene lookups. Uppercase-only
and mixed-case instances now render identically without rewriting the scene;
invalid UUIDs return validation errors with assetId. The public acceptance
checks both cases and restores the original canonical references afterwards.
I1 is documented (empty CLI name uses the default); I2 historical suppression
prose is changed to past tense while captured command evidence is retained;
I3 required no change because both emissive-only and shared-use tests discriminate.

The owner reporting rule is persisted in the agent guide and STATUS. No
intermediate A.5 report is sent while final verification is pending.

Final correction source: `ef6775a9bb49e1330cac11d6f4bb2931a0cb2cb4`. Its clean
public clone passed 22/22 plus M1 acceptance and clean-clone acceptance. The
uppercase/mixed-case public scenario also passed with the explicit Intel ICD
and three hidden-window frames. CI 41076353145 verifies this exact source;
it completed/success with all six jobs. LinuxClang/GCC14/unsuppressedASan each
passed 22/22; WindowsMSVC passed 16/16, including placement and both goldens.
Both lavapipe platforms reported emissive 117 and Blender image difference 0.
Quality clang-format18/clang-tidy18 with warnings-as-errors and Fuzz passed.
No test tolerance, skip rule, sanitizer check or dependency was weakened to
obtain these results. This final source differs from the independently approved
4ce8cac by the documented small UUID identity fix and reporting documentation.

Final CI: [41076353145](https://github.com/chl84/3D_game_engine/actions/runs/41076353145).
