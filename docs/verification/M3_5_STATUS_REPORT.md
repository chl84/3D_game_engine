# Status report — 2026-10-30 — M3.5

**Delivered:** Shared bindless PBR resources for window/offscreen rendering,
authored DirectionalLight, baked Blender normal-map golden, analytical GPU
probes, safe retirement/frame error recovery, and documentation. Independent
Claude Opus 5.5 high round 3 approved code `eceb5f9`. The owner's reporting
follow-up adds a dedicated color-space PNG golden and resolves the local
112-byte report with source evidence and an opt-in native-driver policy.
M3.6 has not started. No merge/tag or milestone closure is included.

| Function | Status | Evidence |
|---|---|---|
| Shared mandatory bindless PBR path | Automatically tested | 4096 sampled images and 4096 materials, actual last-slot rendering, rejected overflow; pool accounts for 4099 descriptors including two samplers and SSBO |
| Mesh/image upload, mips and material inputs | Automatically tested | GGX/Smith/Schlick metallic-roughness, baseColor, normal, AO, emissive, OPAQUE/MASK; numeric readback, GPU mip and full-table probes |
| DirectionalLight/tone output | Automatically tested | One authored enabled light, error for multiple enabled lights, ambient 0.03, Reinhard plus one sRGB transfer |
| Tangents without exported TANGENT | Automatically tested | Original local Blender mirrored coupon cooked by official MikkTSpace; strict image match against fixed golden and Blender-exported tangents |
| Bake/geometry provenance | Automatically tested and inspected in Blender | Packed editable .blend and paired self-contained GLBs; curved top, 1024 triangles, 512 per UV orientation, actual mirrored non-overlapping islands and seam duplication |
| Independent tangent sensitivity | Automatically tested | Removing normal map and reversing tangent signs change the image; analytical reflected-normal red 137 becomes 75 when determinant handedness is removed |
| Transform precision/degeneracy | Automatically tested | Identical 16 km image, affine hierarchy composition, tiny/collapsed node controls; scale 1e-14 probe becomes 48 instead of 137 without normalization on FTZ hardware |
| Resource retirement and frame recovery | Automatically tested | Two submitted frames retain retired descriptor/buffer slots until idle collection; render resumes after missing upload/camera, duplicate light and retired scene asset errors; Required validation zero errors |
| Cache/scene-root correction | Automatically tested | Selected roots preserved via stable IDs in existing metadata; importer@3 invalidates old derived cache; matching-key corrupt buffers rebuilt; v1 source formats unchanged |
| Original M3.5 Linux and Windows acceptance | Compiled and automatically tested | Final original CI below: Clang/GCC14/ASan 20/20; WindowsMSVC 14/14 inclPBRgolden on lavapipe; all six jobs success |
| Original exact clean clone | Automatically tested | `eceb5f9`: 20/20, M1 acceptance and clean-clone acceptance passed; independently reproduced by reviewer with GCC |
| Physical Iris Xe | Automatically tested | Strict Blender reference difference 0, normal probes137, wrap/filter/underflow controls and window recovery/lifetime pass; independently repeated in review |
| Dedicated color-space PNG golden (follow-up) | Automatically tested on Linux and Windows lavapipe | `engine_pbr_color_golden` compares six Vulkan-rendered patches to `m3_pbr_colorspace_192x128.png` and checks independent pixel values; included in Linux/Windows CTest |
| Full follow-up local Clang and exact clean clone | Automatically tested | 21/21 without skips plus CLI acceptance, commands/output in M3_ACCEPTANCE_EVIDENCE.txt |
| Local Mesa ASan/UBSan policy | Automatically tested | 21/21 using `linux-asan-lavapipe`, detect_leaks=1 and UBSan halt-on-error; ordinary preset and reference CI stay unsuppressed |
| Native leak ownership/negative control | Manually verified | Manual cc/probe commands in M3_SANITIZER_DIAGNOSIS.md; exact 112-byte reproduction and fatal 73-byte control captured in M3_SANITIZER_EVIDENCE.txt |

## CI and source boundaries

Original M3.5 acceptance:
[41057830415](https://github.com/chl84/3D_game_engine/actions/runs/41057830415),
exact head `eceb5f9e7dd63c2e2aef7a5c54b8cab239d99f18`, completed/success.
Linux Clang, GCC14, ASan/UBSan, Windows MSVC, Fuzz, Quality all succeeded.
ASan had detect_leaks=1:abort_on_error=1 and UBSan halt_on_error=1, no leak or
runtime-error lines, no unexplained skips. Windows PBR reported Blender image
difference 0 and both analytical normal values 137. Quality ran clang-tidy18
with warnings as errors over every C++ source.

The follow-up changes tests/configuration/diagnostics/reports, not the approved
runtime rendering/import implementation. Its final SHA, CI and reviewer results
are recorded in the final evidence update below; original CI is not claimed
as proof of a newly added PNG test.

## Exactly how missing tangents are tested

Official MikkTSpace is called during cooking, with vertex splits at corner
breaks; authored tangents are preserved. The original Blender file packs a
non-flat baked normal texture. One export omits TANGENT and the paired export
contains Blender's tangents with both handedness signs.

- CPU: every normal-mapped corner, including w, compared with tolerance 2e-4;
  both signs required. The current asset suite has 18 cases / 22,693 assertions.
- GPU: no-TANGENT model versus Blender reference and fixed 256x192 golden with
  max byte error 8, fewer than 0.1% bytes differing by more than 3. Measured
  reference difference 0. Normal-map removal must also fail that strict matcher.
- Negative mutations: missing sign, missing normal map, S/T swap, wrong UV wrap,
  missing scale normalization and frame-fence misordering are detected. No
  golden was re-recorded to accommodate these defects.

## Explicit color-space golden

[Fixed PNG](../../tests/fixtures/golden/m3_pbr_colorspace_192x128.png), 192x128,
row-major six 64x64 GPU renders (one shared PBR implementation):

| Patch | Input | Test reference center red (±2) |
|---|---|---|
| 0 | 128 sRGB baseColor | 19 |
| 1 | Same 128 interpreted linearly, negative format control | 33 |
| 2 | Linear metallic-roughness, blue128/green200 | 33 band center; analytic rounded value and PNG readback 32 |
| 3 | Linear AO 128 | 33 |
| 4 | Linear emissive 128 and black base | 156 |
| 5 | Linear normal (128,204,230), authored directional light | 137 |

Unrounded analytical values are approximately 18.8 / 32.5 / 32.4 / 32.5 /
156.4 / 137.3. The PNG center bytes are 19 / 33 / 32 / 33 / 156 / 137.
Patch 2 retains the legacy integer band center 33 with tolerance ±2; 33 is
not presented as its rounded analytical value. No tolerance is widened.
Every patch must satisfy its independent reference band even when recording.
The complete PNG comparison uses the same strict tolerance as the Blender
image test. The existing separate sRGB mip probe verifies linear-space mip
filtering. Emissive-linear is the owner's approved M3 policy (ADR-0012).
Seven mutations verify discrimination: changing the format of any patch fails
its independent reference check; corrupting one loaded reference pixel fails the fixed
PNG comparison. Correct test artifacts were restored.

The owner's emissive-linear policy differs from glTF 2.0, which specifies sRGB
for emissiveTexture. The golden locks the approved policy: linear gives 156;
sRGB would give 117. No policy change is made here.

This dedicated PNG was added in this follow-up: before it, these color inputs
were covered by numeric GPU readback in `engine_pbr_golden`, not by a separate
fixed color-chart PNG. The distinction corrects the earlier short report.

```sh
VK_ICD_FILENAMES=/usr/share/vulkan/icd.d/lvp_icd.json \
  ctest --preset ci-linux-clang -R engine_pbr_color_golden -V
VK_ICD_FILENAMES=/usr/share/vulkan/icd.d/lvp_icd.json \
  ctest --preset linux-asan-lavapipe --output-on-failure
```

## Leak diagnosis and policy

[Full diagnosis, normalized stacks and source proof](M3_SANITIZER_DIAGNOSIS.md),
[captured native stack/control](M3_SANITIZER_EVIDENCE.txt),
the historical narrow LSan rule (removed at the owner's request).
The two allocations are Mesa's RTASM arena metadata, not engine-owned objects.
The manually run standalone program releases all requested executable code
and still reproduces exactly 56+56 bytes. The loader had unloaded the driver before LSan
symbolization; address/base correlation recovers the originating module and
source functions. Exact DWARF was unavailable; recovered names are explicitly
identified as instruction/source correlation, not invented debug symbols.

The supported loader-retention switch preserves process-lifetime globals.
The file only names rtasm_exec_malloc; unknown modules/calloc/engine code are
not excluded. This rule is inactive and unverified on the available stripped
binaries; a matching external debug file would be needed to make it usable. On this stripped build retention alone makes the arena
reachable, so zero suppression patterns are consumed. An intentional 73-byte
application leak remains fatal with that policy. Reviewer also injected 91
bytes into engine_pbr_test itself; LSan reported them and exited 134. This
control does not establish that every possible allocation referenced only
through a retained driver/layer global is freed: such globals are LSan roots.
No broad exception, disabled
leak check or normal-map/validation bypass is used.

**Not verified / test limitations:** Windows window/swapchain presentation;
reference-scene FPS/whole-process memory measurements (M5/M8); asynchronous
streaming; shadows/IBL/point-light rendering. Retained-library local tests do
not test driver unload/reload; the separate diagnostic covers that behavior.
CLI placement/project auto-resolution remains M3.6 and is not claimed here.

**Known defects:** Mesa 26.2.5 leaves its RTASM global arena metadata on module
unload. Source and ownership are resolved and bounded; ordinary local LSan
still exposes it without the opt-in policy. There is no identified engine-owned
leak from this report. Reference Ubuntu runs remain unmodified/unsuppressed.
The original MSVC portability, fence-hang, weak comparison/probe and tidy18
findings were corrected before final acceptance; none remain open.

**Deviations from plan:** The initially missing explicit color-space PNG is
supplied in this follow-up rather than retrospectively claimed. Native budget
reservations are conservative estimates, not full process/performance proof.
No new dependency or breaking project/scene/asset schema was introduced.

**Decisions requiring approval:** None outstanding for these corrections;
the owner explicitly requested external-leak diagnosis and a documented policy.
Merge, tag and M3 milestone closure still require the owner.

**Next step:** Owner assessment of both full reports. M3.6 remains stopped and
has not been started.

## Follow-up final evidence

Source `59c2e979a12484e2c9124f38ad0165ca2b9be0c3`,
[CI41065543569](https://github.com/chl84/3D_game_engine/actions/runs/41065543569),
attempt 2 completed/success. All six jobs succeeded. Linux Clang, GCC14 and
unsuppressed ASan/UBSan passed 21/21; Windows MSVC passed 15/15. The fixed
color-space PNG test explicitly passed on both lavapipe platforms with center
bytes 19/33/32/33/156/137. Fuzz and Quality also passed.

Attempt 1 Windows failed before compilation on a Chocolatey HTTP 504 during
pinned SDK installation. Only that job was rerun, with no source, SDK version,
skip or tolerance change. The successful retry supplies the missing Windows
compilation/headless/offscreen evidence.

Permanent evidence:

- [CI excerpts, source/attempt identity and sanitizer flags](M3_CI_EVIDENCE.txt).
- [Local21/21, exact clean-clone acceptance, Iris Xe and asset/hash output](M3_ACCEPTANCE_EVIDENCE.txt).
- [Local-policy21/21, native112-byte stack and73-byte negative control](M3_SANITIZER_EVIDENCE.txt).
- [Independent reviewer91-byte injection into engine_pbr_test, exit134](M3_REVIEW_LEAK_CONTROL.txt).

The independent technical audit confirmed the native source attribution,
local-policy21/21, upstream MikkTSpace hashes, Blender TANGENT absence/presence,
color mathematics, PNG-alone discrimination and production-code mutations.
Mandatory reporting corrections are applied: manual native diagnostics are
marked Manually verified, and acceptance evidence is committed rather than
pointing to temporary logs. The inactive rule, driver-global reachability
limit and diagnostic exit codes are explicit. No rendering/import runtime code
changed during these reporting corrections. M3.6 remains stopped.

## Owner approval and subsequent correction — 2026-10-30

The owner approved M3.4 and M3.5 and authorized M3.6. The evidence above describes
`59c2e97` and its former emissive-linear policy. The subsequent owner correction
requires sRGB emissiveTexture: ADR-0012 is updated, importer@4 invalidates old
caches, the regenerated color golden expects 117 for emissive, and a linear
negative control expects 156. The inactive suppression rule is removed;
linux-asan-lavapipe retains the loader policy without any suppression file.
Current correction and M3.6 verification are recorded in STATUS.md.
