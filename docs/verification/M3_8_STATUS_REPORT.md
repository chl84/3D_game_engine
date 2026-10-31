# Status report — 2026-10-31 — M3.8

**Final release closure:** All six jobs in
[CI41184429834](https://github.com/chl84/3D_game_engine/actions/runs/41184429834)
passed on release head 031bc43. Owner-authorized standard merge completed in
PR5 as 88ccc85; annotated v0.4.0 is published and its remote target verified.
[Release evidence](M3_FINAL_RELEASE_EVIDENCE.txt). Earlier source-specific
acceptance records below are retained; no M3 release gate remains open.
M4 is planning-only until the owner approves its proposal.

**Outcome:** Technical acceptance is complete for the verified implementation.
The owner reported the manual Blender test passed and asked work to continue.
The owner subsequently authorized merge and tagging. Normal PR checks must
pass on the v0.4.0 release candidate before those actions.

**Delivered:** M3 hardening and complete CLI/live Blender acceptance, retained
fuzz regressions, bounded asset-wide tangent-generation admission, a realistic
262,144-triangle normal-mapped import regression, manual Blender guide,
authenticated durable live control re-review, corrected PNG handling and
extended fuzz/corpus evidence. All required review findings are resolved.

**Reviewed implementation:** `5d01a36d83b590392349b5925d6ce38cd7b3a7c1`.
GPT-6 Astra high independently approved the entire M3.8 delta and current
M3.7 corrections/journal without blocking or required findings.
**Verified publication baseline:** `9c812f5063b3f4a291777a43e071022d56865f54`.
This baseline and evidence commits through 684f101 have no implementation delta
from the reviewed source. Release preparation additionally aligns CMake/the
engine package version and the exact version smoke test to 0.4.0; dependency
versions and engine behavior are unchanged. Final normal CI must cover that
version-only delta. [PR5](https://github.com/chl84/3D_game_engine/pull/5)
shows the latest documentation publication checks; the source-specific evidence
below records what was actually run, not an assumed pass for another commit.

| Function or gate | Status | Evidence |
|---|---|---|
| Independent review and M3.7 re-review | Independently verified | [Astra high review](M3_8_ASTRA_REVIEW.md): no required findings; fresh Clang 28/28; deadline and durable intent/completion ordering confirmed |
| Full Linux GCC14 and Clang18 reference suites | Automatically tested | [CI41177949157](https://github.com/chl84/3D_game_engine/actions/runs/41177949157), 28/28 in each job; [excerpts](M3_8_REFERENCE_CI_EVIDENCE.txt) |
| Full Linux ASan/UBSan | Automatically tested | Reference 27/27; local 27/27 with leak checks and documented lavapipe retention; [local evidence](M3_8_ASTRA_LOCAL_EVIDENCE.txt) |
| Full-build TSan scope | Automatically tested | All targets built, documented 13-test scope passed; [local evidence](M3_8_ASTRA_LOCAL_EVIDENCE.txt) |
| Windows MSVC and offscreen lavapipe | Automatically tested | Reference 21/21, including large import, live API, journal/deadline and PBR/color-space goldens |
| Reference format/tidy18 and short fuzz | Automatically tested | All six reference jobs completed/success; short GLB/image/RPC fuzz completed 31 seconds each |
| Exact-source public clean clone | Automatically tested | Candidate 9c812f5 rebuilt; 28/28, zero CTest skips, M1 and complete M3 public CLI/live acceptance; [evidence](M3_8_PUBLIC_CLONE_EVIDENCE.txt) |
| Actual device/render execution | Automatically tested | Supplemental clone Catch2 XML: 13 cases, 105 assertions, zero internal skips; doctor confirms lavapipe and descriptor indexing |
| Blender golden, color spaces and reimport | Automatically tested | Linux/Windows goldens pass; Blender tangent image difference 0; import/place/save/load/reimport preserves references; public acceptance passes |
| Tangent work admission | Owner-approved and automatically tested | Inclusive 1,000,000 dense-pair asset budget; [ADR-0015](../decisions/ADR-0015-tangent-work-budget.md), [budget evidence](M3_8_TANGENT_BUDGET_EVIDENCE.txt) |
| Controlled rejection/error guidance | Automatically tested | Import/reimport/dry-run and live tests require E_VALIDATION, work/limit/primitive details and Blender tangent-export hint; no project/cache mutation on rejection |
| Large normal-mapped asset | Automatically tested and measured | Original Blender ring, 262,144 triangles, embedded baked normal PNG, no source tangents; retained triangle count and valid generated tangents; [evidence](M3_8_LARGE_MODEL_EVIDENCE.txt) |
| PNG empty-IDAT regression | Automatically tested | Exact old fuzz input fails baseline, corrected replay passes; valid empty-chunk PNGs retain identical pixels; ASan/UBSan image units 36 assertions / 3 cases; [evidence](M3_8_EMPTY_IDAT_EVIDENCE.txt) |
| Fresh local extended fuzz | Automatically tested | 600 seconds per target, exit 0: GLB 65,407, image 168,215, RPC 1,274,502; [evidence](M3_8_FRESH_EXTENDED_FUZZ_EVIDENCE.txt) |
| GitHub extended fuzz and corpus save | Automatically tested | [Run41177977464](https://github.com/chl84/3D_game_engine/actions/runs/41177977464) success; 600 seconds each; GLB 96,906, image 286,644, RPC 2,484,027; [evidence](M3_8_GITHUB_LONG_FUZZ_FIRST.txt) |
| GitHub corpus restoration and use | Verified from actual runner logs | Second run hit the first run's exact key, restored it and started GLB with 676 corpus files; [evidence](M3_8_GITHUB_CORPUS_REUSE_EVIDENCE.txt) |
| Owner manual Blender flow | Manually verified, owner-reported | Owner message on 2026-10-31: manual Blender test passed; [guide](../../samples/m3-manual/README.md). No model/settings/hardware details were supplied or inferred |

## Performance measurement

The large model's three optimized imports with fresh engine caches took
0.876245, 0.854202 and 0.852245 seconds; median **0.854202 seconds**. The
i7-1265U development machine was on AC with a fixed `balanced` profile and
currently has 30 GiB RAM. The original target specifies 16 GB. The OS page cache
was warm. Timing includes CLI launch, parsing, admission/generation, texture
decode, cooking, writes and response-schema validation; GPU rendering is not
included. [Full conditions](../PERFORMANCE.md#m38-high-poly-import-admission-measurement-2026-10-31).
This is an import measurement, not reference-world frame-rate or memory-budget
compliance, and one representative model does not prove admission of all topology.

## Cache-run disposition and scope correction

The first successful GitHub long run saved its corpus. The second run restored
and used that exact corpus, then was cancelled at the owner's request. Its
final conclusion is `cancelled`; it is not represented as a passing second long
run. The approved M3 plan describes nightly/cached long fuzzing as an optional
suggestion. Requiring a second complete long run was an unnecessary extra gate
introduced by the parent agent; it is removed from the closure checklist.
Required short fuzz, one complete long run and actual save/restore evidence all
remain intact. No test or sanitizer guard was weakened to obtain a pass.

## Not verified / limitations

- Physical Windows presentation remains unverified; approved Windows M3
  acceptance uses offscreen lavapipe and passes.
- CTest JUnit cannot expose every internal Catch2 skip. Supplemental direct
  render reports demonstrate zero skips for the verified clone; broader
  reporting remains an optional future improvement.
- Full reference-world 60 FPS and memory-budget compliance belongs to M5/M8.
- Durable journal synchronization tests do not establish storage-controller
  power-loss durability or cross-file atomicity with scene/asset writes.
- Existing M3 scope exclusions remain: skeletal playback, shadows, point-light
  rendering, IBL, compressed textures and editor authoring UI.

**Known defects:** No unresolved required implementation/review finding for the
verified M3.8 candidate. Limitations above are disclosed rather than counted
as completed capabilities.
**Plan deviations:** The optional cache experiment was temporarily treated as
a mandatory second full run; the report corrects that extra gate. The PNG fuzz
failure was fixed with a permanent regression before Astra approval. Dependency
versions, source/cooked formats and sanitizer limits remain unchanged.
**Authorization:** The owner explicitly approved merge and tagging after the
manual test pass; use standard branch protection and tag v0.4.0 on the actual
main merge commit. M4 implementation still requires plan approval.
**Next step:** Publish this report/evidence, verify the normal publication CI,
then perform the authorized merge/tag. Only after merge and tag, draft the M4 plan
with ADR proposals for Lua bindings, deterministic Jolt settings and game-module
linking, and wait for owner approval before implementation.
