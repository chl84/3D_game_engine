# Status report — 2026-10-30 — M3.6

**Delivered:** Owner-requested sRGB emissive correction and removal of the
inactive LSan rule, followed by asset instance placement and project asset
resolution for window/offscreen rendering. M3.4/M3.5 and the pinned MikkTSpace
implementation/marked UB-corrected build copy are owner-approved. No merge/tag
or M3.7 work is included.

| Function | Status | Evidence |
|---|---|---|
| Emissive sRGB cooking/cache invalidation | Automatically tested | asset CLI requires sRGB sidecar/cooked metadata with emissive alone and shared baseColor/emissive; importer@4 replaces @3 derived cache; mixed emissive/data use rejected |
| Corrected color PNG and linear negative control | Automatically tested on local lavapipe | Fixed chart emissive117; linear control156; omitted cooking classification and linear GPU-input mutations both fail |
| Shared asset.place@1 handler and CLI | Automatically tested locally | Asset existence/cache validation before scene mutation, name/position defaults, finite positions, unique and seed-deterministic entity IDs, double position precision |
| Placement undo/redo/transaction rollback | Automatically tested locally | CPU command tests; public batch undo and failed batch preserve the saved scene |
| Delete/undo of placed instance | Automatically tested locally | Snapshot restores AssetInstance; resulting rendered image equals pre-delete image |
| One reference entity per instance, v3 round-trip | Automatically tested locally | Existing v1/v2 migration and v3 component tests; public acceptance saves only camera plus two instances, without expanding GLB nodes |
| Read-only project resolution/resource sharing | Automatically tested locally | Fresh-process screenshot loads each UUID once (duplicate upload would fail); two image halves contain visible geometry; reload is pixel-identical |
| Changed-source reimport | Automatically tested locally | Node translation changes image; asset UUID and scene bytes stay unchanged; fresh reload matches changed output |
| Missing cache and repair | Automatically tested locally | Render fails with assetId without any project write; explicit reimport restores identical output |
| Windowed CLI with project assets | Automatically tested on local Linux lavapipe | Three hidden-window frames with two instances; no scene writes |
| Local Clang and ASan/UBSan | Automatically tested | Full 22/22 results and commands in M3_6_LOCAL_EVIDENCE.txt; sanitizers use retention only, no suppression file |
| Physical Iris Xe | Automatically tested locally | Complete public placement acceptance, including three hidden-window frames, passes with Intel ICD |
| Windows MSVC and lavapipe | Not verified for M3.6 yet | Previous M3.5 Windows success is not substituted for this change; new exact-SHA CI required |
| Independent reviewer | Pending | M3.6 review requested after local verification; no approval claimed |

[Permanent local evidence](M3_6_LOCAL_EVIDENCE.txt).

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

**Not verified / test limitations:** New Windows CI, Windows swapchain
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

**Decisions requiring approval:** None newly proposed. Independent review and
exact-SHA CI remain acceptance gates; merge/tag still require the owner.

**Next step:** Finish independent review and all six CI jobs, address findings,
and report the exact source/results to the owner. Do not start M3.7 here.
