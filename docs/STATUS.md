# Status

**Last updated:** 2026-10-30
**Current milestone:** M3 — asset pipeline and live control
**Current substep:** M3.8 — authorized by the owner on 2026-10-30; hardening and final acceptance in progress.

M3.7 is owner-approved on 2026-10-30.
Verified source d0797e1b361f4f4379b69d33ebc523ab88e93035. Claude Opus5.5 high
independently approved all required corrections and durable JSONL with no
blocking/required findings. Final CI41108912088 completed/success in all six
jobs: GCC/Clang26/26, ASan25/25, Windows19/19, Fuzz and Quality success.
[Consolidated A.5 report](verification/M3_7_STATUS_REPORT.md).
Reviewer selection remains exclusively the owner's decision; Claude Opus is
current reviewer. M3.8 is authorized; merge and tag require separate owner approval.

M3.6 was owner-approved on 2026-10-30 after reviewer approval and final
CI41076353145 (all6jobs; Linux22/22, Windows16/16). M3.4/M3.5 are approved.
The owner requires shared CLI handlers/bus, 127.0.0.1 only, per-request token,
bounded framing/connections/timeouts, sandboxed client paths, exclusive
project locking, parser fuzzing and explicit negative/security/log tests.
The current independent reviewer is Claude Opus5.5 high. Historical Astra
review attribution is retained; no substitution is permitted without the owner.
M3.8 is in progress. No M3 merge/tag is authorized. Reports follow the owner timing rule.

## Substeps

| Substeps | Result |
|---|---|
| M0.0 | Vulkan development packages installed by the owner; vcpkg bootstrapped at the pinned commit; git identity configured; `vulkaninfo` and lavapipe verified |
| M0.1 | Repository initialized; MIT license; CMake skeleton with presets; `engine --version --json`; `engine_version_json` smoke test; ADR-0001–0003; initial documentation skeleton |
| M0.2 | vcpkg manifest with nlohmann/json, spdlog, CLI11, `tl::expected` and Catch2; warning flags with `-Werror` in CI presets; clang-format and clang-tidy configuration; ASan/UBSan and TSan presets; Catch2 unit and dependency smoke tests; Windows MSVC + vcpkg CI job moved from M0.5 and green |
| M0.3 | Core foundations: `Result<T>`/`Error` with stable codes, UUID v4 and seeded `UuidGenerator`, `PathSandbox`, JSON `Config`, `Log` with console and JSONL sinks, `log-record@1` schema, and 20 core test cases at M0.3 (21 after M0.4); Linux and Windows CI green |
| M0.4 | CLI11 command dispatch, `engine --version --json`, `engine doctor --json` with volk-based Vulkan detection, `version@1` and `doctor@1` schemas, Python contract tests with `jsonschema`, timestamp boundary tests, and Catch2 `SKIP` with a CI test summary that shows skipped cases; Linux and Windows CI green |
| M0.5 | clang-format and clang-tidy in CI (including project headers), an ASan/UBSan Linux job, pinned CMake 4.4.5 and Ninja 1.13.2, lavapipe installation and Vulkan detection tests in Linux CI, failure artifacts, and branch protection on `main` with the five CI jobs as required checks; all five CI jobs green |
| M0.6 | Complete English `VISION.md` translation B.3–B.13; expanded `ARCHITECTURE.md`, `PROJECT_FORMAT.md`, `AI_AGENT_GUIDE.md` and `API_REFERENCE.md`; ADR-0004–0006; `THIRD_PARTY_NOTICES.md`; and the `engine_docs_links` test. Reviewer verdict: GODKJENT |
| M0.7 | Clean-clone acceptance script, ASan/UBSan, TSan, format/tidy, Windows headless CI, sensitive-data scan, tag `v0.1.0` and the M0 acceptance report. Approved by the owner on 2026-10-29 |

## M1 substeps

| Substep | Scope | Status |
|---|---|---|
| M1.1 | Reflection registry and JSON Schema generation | Done |
| M1.2 | ECS, UUID and transform hierarchy | Done |
| M1.3 | Command bus with validation and command log | Done |
| M1.4 | Transactions and undo/redo | Done |
| M1.5 | Project and scene format | Done |
| M1.6 | Offline CLI | Done |
| M1.7 | Fixed tick and determinism | Done |
| M1.8 | API reference, AI guide and acceptance script | Done |
| M1.9 | Hardening and M1 acceptance | Done |
| M1.10 | `--dry-run`, generated `API_REFERENCE.md`, and fixes from the Codex review (symlink-safe temp files, scene-create overwrite, subtree undo, missing scene IDs, revision handling) | Done — released as `v0.2.1` |

## M2 substeps

| Substep | Scope | Status |
|---|---|---|
| M2.1 | Render module, SDL3 window, Vulkan 1.3 device, queues, Synchronization2, Dynamic Rendering, VMA, validation messenger, headless mode and smoke tests | Done — reviewer GODKJENT |
| M2.2 | Swapchain, frame loop, Dynamic Rendering clear, present, resize/minimize handling and integration test | Done — reviewer GODKJENT |
| M2.3 | glslc build step, graphics pipeline, vertex buffer, camera and first triangle | Done — reviewer GODKJENT |
| M2.4 | Scene format v2 mesh references, SceneView, double world transforms, camera-relative rendering, depth buffer and frustum culling | Done — owner approved with double-position change; re-review approved |
| M2.5 | Offscreen rendering, `engine screenshot`, PNG, golden images and Windows ICD follow-up | Done — reviewer GODKJENT; owner approved 2026-10-29. Windows ICD and pinned Linux `glslc` moved to M2.8 |
| M2.6 | ImGui debug overlay, screenshot sandbox/limits, full CTest under Xvfb | Done — reviewer GODKJENT; owner approved 2026-10-29 |
| M2.7 | Descriptor strategy and validation hardening | Done — reviewer GODKJENT; owner approved 2026-10-29 |
| M2.8 | Hardening and M2 acceptance | Approved by the owner 2026-10-29; reference measurement moved to M5/M8 and ImGui re-init to M8 |
| M2.9 | Windowed `engine run`, fly camera, overlay toggle and ImGui input routing; sample project | Done — reviewer GODKJENT; owner manual test passed; merged and tagged `v0.3.0` |

## M3 substeps

| Substep | Scope | Status |
|---|---|---|
| M3.1 | ADRs, scene format v3, asset schemas, fastgltf dependency, `ilammy` replacement and PointLight warnings | Reviewer GODKJENT |
| M3.2 | GLB import core; animation channels reported only | Reviewer GODKJENT (round 4) |
| M3.3 | Cooked asset cache, stable IDs, reimport, image decoding and fuzz targets | Reviewer GODKJENT (round 2) |
| M3.4 | Asset CLI and approved MikkTSpace cooking | Owner approved 2026-10-30 |
| M3.5 | PBR renderer, bindless assets and `DirectionalLight` | Owner approved 2026-10-30; emissive correction requested |
| M3.6 | `AssetInstance` placement and scene v3 save/load | Owner approved 2026-10-30 |
| M3.7 | Local JSON-RPC live API and durable JSONL | Owner-approved2026-10-30; Claude technical approval; all six final CI jobs green |
| M3.8 | M3 hardening and acceptance | Owner-authorized; implementation/local verification in progress; Claude review and reference CI pending |

## M3.5 fixture preparation (owner requested)

A locally authored Blender calibration coupon is saved under `tests/assets/m3`:
`blender_mirrored_normalmap.blend`, `blender_mirrored_no_tangents.glb`,
`blender_mirrored_tangents.glb` and `blender_mirrored_normal.png`.
The editable source packs its texture and retains the procedural bake recipe.
Its curved top has 1024 triangles and two non-overlapping UV islands with
opposite orientations (512 triangles each). Provenance and regeneration are
recorded in `tests/assets/m3/PROVENANCE.md`.

The fixture was generated and reopened with local Blender 5.2.2 LTS. Binary
checks found no home-directory/user markers; GLB inspection verifies actual
mirrored UVs, embedded images, and TANGENT absence/presence in the paired
exports. The new CPU regression compares regenerated tangents against Blender's
reference on the normal-mapped primitive, including both handedness signs.
The full asset suite passes ASan/UBSan: 16 cases / 22,677 assertions. The fixture
was reviewer-approved. Small follow-ups remove absolute packed-image metadata
and add `blender/verify_mirrored_normalmap.py`, which verifies the repository
files directly. The .blend reopens after relocation without an external PNG.
The fixture now also drives the M3.5 PBR golden and Blender-reference image tests.

## Repository state

- `main`: the M2 release `bb8514c`, merged from `m2/vulkan-renderer` on
  2026-10-30 and tagged `v0.3.0`.
- `m3/plan`: the approved M3 plan, not merged.
- `m3/asset-pipeline`: the M3 implementation branch.
- `m0/foundation`, `m1/headless-core`, `m1.10/maintenance`: merged milestone
  branches, tagged `v0.1.0`, `v0.2.0` and `v0.2.1`.
- The repository is **public**. The owner made that change; the agent did not
  change repository visibility. It was done to enable branch protection on the
  GitHub Free plan.
- Branch protection is enabled on `main` via
  `scripts/setup_branch_protection.sh`. Required checks are
  `Linux (ci-linux-gcc14)`, `Linux (ci-linux-clang)`, `Windows (MSVC)`,
  `Quality` and `Linux ASan/UBSan`; force pushes and deletions are disabled,
  and conversations must be resolved before merging.
- **Branch protection blocker:** PR #2 was blocked by
  `required_conversation_resolution`, because six Codex review threads were
  unresolved. `--admin` bypassed the rule and must not be used again.

## Owner reporting instruction — 2026-10-30

Do not send interim or substep status reports before independent review is
fully handled. Send one consolidated A.5 report after review findings have
been addressed and required re-review and CI checks have passed. During the
work, provide only brief progress updates, necessary questions or genuine
blockers. This owner instruction applies across sessions and supersedes any
interpretation of A.2/A.4 that would require a premature report. It is also
recorded in the versioned AI_AGENT_GUIDE.md and under A.5 in the local prompt
sources AGENT_RULES.md and MASTERPROMPT_v0.2.md.

## Owner approval required

- Merging to `main`, creating or pushing a tag, and using GitHub admin bypass
  (`--admin`) **always require the owner's explicit approval**.
- Reviewer approval covers a substep only; it never authorizes a merge, tag or
  milestone closure.
- When branch protection blocks a merge, stop and ask the owner; do not use
  `--admin`.

## M3.4 current verification

- Local Clang build with warnings as errors: 18/18 CTest passed on lavapipe,
  including the existing window/overlay/offscreen tests; no skipped tests.
- ASan/UBSan: core, asset and asset CLI tests passed (3/3). Full local suite
  passed 12/18; six graphics-related tests failed, including 112-byte
  LeakSanitizer reports without engine symbols. No sanitizer was disabled;
  Ubuntu CI on `7cfad7a` passed the complete sanitized suite (18/18).
- Permanent Clang fuzz targets with UBSan halt-on-error: GLB 20,055 runs and
  image 87,754 runs, 30-second budgets each, no final-run sanitizer findings.
  Initial fuzzing found a MikkTSpace NaN-to-int UB; the documented build-copy
  correction and collapsed-position regression were added before the final run.
- clang-format 18 passed; local clang-tidy 23 passed for modified C++ units.
- Sensitive scan passed for 203 tracked files; documentation links passed.
- Review round 1: RETT FØRST for MSVC JSON/string_view ambiguity; fixed in
  `7cfad7a`. Review round 2 found no remaining code defect and awaited Quality.
- CI [41041374885](https://github.com/chl84/3D_game_engine/actions/runs/41041374885)
  on `7cfad7a`: all six jobs passed. Linux GCC/Clang and ASan/UBSan ran 18/18;
  Windows MSVC ran 13/13 with lavapipe offscreen golden/screenshot. Quality and
  Fuzz passed. The original Windows build failure is retained in the review
  history; the new successful Windows run supplies the missing evidence.
- A clean public clone of `7cfad7a` built and passed 18/18 without skips,
  followed by `M1 acceptance passed` and `M1 clean-clone acceptance passed`.
- Final reviewer verdict: GODKJENT (round 3). The reviewer independently
  verified the exact CI SHA and logs. No merge or tag has been performed.
- Normal-mapped Blender GLBs with and without exported tangents are checked
  in and compared per corner by the CPU tests. The PBR image test remains an
  explicit M3.5 requirement; it has not run or been claimed complete.

## Historical verification evidence

The table below records earlier substeps; it is not a fresh M3.4 verification
claim. Current evidence is recorded with the M3.4 report and summary.

| Item | Status | Evidence |
|---|---|---|
| GCC 14 C++20 build | Automatically tested | `cmake --preset ci-linux-gcc14`, build, `ctest` → 18 registered; CI runs 18/18 with lavapipe and Xvfb. Locally `linux-debug` passes 17/18 and skips the golden test without lavapipe; `engine_core_tests` → 63 cases / 500 assertions |
| Clang C++20 build | Automatically tested | `cmake --preset ci-linux-clang`, build, `ctest` → 18 registered; locally 17/18 with the golden test skipped without lavapipe. CI runs 18/18 with lavapipe and Xvfb |
| ASan + UBSan | Automatically tested | `cmake --preset linux-asan`, build, `ctest` → 18 registered; locally 17/18 with the golden test skipped. The window, swapchain and overlay tests now execute. The CI `Linux ASan/UBSan` job also runs `doctor --require vulkan` under the sanitizers |
| TSan | Locally automatically tested (not in CI) | `cmake --preset linux-tsan`, build, `ctest -E "engine_window_smoke\|engine_swapchain_test\|engine_overlay_test\|engine_run_smoke"` → 11/11 CPU-side tests. The four GPU/window tests are excluded because the Khronos validation layer reports a lock-order inversion inside the uninstrumented layer |
| UUID parsing and seeded generation | Automatically tested | `uuid_test.cpp` → round-trip, invalid input, v4 bits, same seed/stream sequence |
| Path sandbox | Automatically tested | `path_sandbox_test.cpp` → `../`, absolute paths, symlink escapes, missing/non-directory roots |
| Config loading and merge | Automatically tested | `config_test.cpp` → nested values, type errors, missing keys, invalid JSON, recursive merge |
| JSONL logging | Automatically tested | `log_test.cpp` → filtered records, required fields, `fields` object, level names |
| log-record@1 schema | Automatically tested | `tests/contract/validate_cli.py` validates `tests/fixtures/log-record.jsonl` against `schemas/log-record@1.schema.json`; `log_test.cpp` checks the C++ output structurally |
| UTC timestamp formatting | Automatically tested | `std::chrono::year_month_day` and `hh_mm_ss`; tests cover epoch, leap day, month boundary and year boundary |
| Symlink test on Windows | Automatically tested | Run [40792296820](https://github.com/chl84/3D_game_engine/actions/runs/40792296820) Windows summary reports 21/21 test cases and 132 assertions with no skipped cases, so the symlink test ran. `SKIP` is used if symlink creation is unavailable, and `ctest -R engine_core_tests -V` in CI displays skipped cases |
| Dependency compile/link | Automatically tested | `engine_dependencies_smoke` includes and links nlohmann/json, spdlog, CLI11, `tl::expected`, Catch2, volk, SDL3, VMA, GLM and Dear ImGui |
| `engine --version --json` | Automatically tested | `tests/smoke/version_json.py` and `engine_cli_contract` validate the JSON and `version@1` schema |
| `engine doctor --json` | Automatically tested | `engine_cli_contract` validates against `doctor@1`; local run reports Intel Iris Xe, llvmpipe, loader 1.4.363 and `VK_LAYER_KHRONOS_validation` |
| `--require vulkan` | Automatically tested | CI `Vulkan doctor detection` captures the exit code explicitly and asserts 0; `Vulkan doctor missing ICD` asserts 8. The contract test also checks `--require unknown` → 2 |
| CLI contract test | Automatically tested | `engine_cli_contract` validates version and doctor output against published schemas and checks `--require unknown` exits 2 |
| clang-format | Automatically tested | `Quality` job runs clang-format 18 with `--dry-run --Werror`; local clang-format 23 is also clean |
| clang-tidy | Automatically tested | `Quality` job runs clang-tidy 18 with `--warnings-as-errors='*'`. `HeaderFilterRegex` is `.*/(engine|tools|tests)/.*`, so project headers are analysed; the corrected filter found `performance-enum-size` in `log.hpp`, fixed by making `LogLevel` a `std::uint8_t` enum |
| clang-tidy header coverage | Locally verified | Before the filter fix, `performance-enum-size` in `log.hpp` was suppressed; after the fix, `clang-tidy -p build/ci-linux-clang` reported it, and it is fixed in code. CI runs clang-tidy 18 with the corrected filter and exits 0; CI has not re-run the old header against the old filter |
| Pinned CMake and Ninja | Automatically tested in CI; locally verified | CI installs CMake 4.4.5 and Ninja 1.13.2 from official release archives and fails unless `cmake --version` and `ninja --version` match. A local clean build in `/tmp/opencode/cmake445-build` with the same binaries passed the suite at the time (4/4); the current suite has 5 tests |
| Windows `doctor` command | Automatically tested | `engine_cli_contract` runs `engine doctor --json` and validates `doctor@1` on Windows; when no Vulkan device exists it accepts exit 8 for `--require vulkan` instead of claiming Vulkan support |
| English VISION translation | Complete | `docs/VISION.md` contains B.1–B.13, including the B.2 wording from ADR-0002; no pending sections |
| Documentation files | Automatically tested | `engine_docs_links` checks local Markdown links and the B.10 file list; 19 Markdown files passed |
| Sensitive-data scan | Automatically tested | `engine_sensitive_scan` checks tracked files for private keys, token formats, credential assignments, absolute home paths, Windows user paths, email addresses and SSH public keys; 72 tracked files passed |
| Reflection registry | Automatically tested | `reflect_test.cpp` covers component registration, field metadata, JSON round-trip, partial updates, unknown fields, wrong types and generated draft-07 schemas |
| Scene ECS and hierarchy | Automatically tested | `scene_test.cpp` covers deterministic UUIDs, duplicate UUID rejection, parent/child links, world transforms, cycle rejection and recursive subtree destruction |
| Command bus | Automatically tested | `command_test.cpp` covers schema validation, successful and failed handlers, unknown commands, duplicate/malformed registration, sorted schemas, command-log entries, undo/redo, redo invalidation, transactions and rollback |
| Project and scene format | Automatically tested | `project_test.cpp` covers project create/load, canonical byte-identical round-trip, entity hierarchy and components, unknown fields, newer format versions, path traversal and exclusive locks |
| Offline CLI | Automatically tested | `validate_cli.py` creates a project, creates a parent/child hierarchy, sets Camera and Motion components, inspects and validates the scene, runs a batch with undo, and validates every response against `command-result@1` |
| Fixed-tick simulation | Automatically tested | `simulation_test.cpp` runs 60/120 ticks at 60 Hz, checks motion, unchanged entities and byte-identical dumps from two identical runs; `validate_cli.py` compares two CLI simulation dumps; 62 core cases / 474 assertions |
| Component schemas | Automatically tested | `engine schema --json` returns draft-07 schemas for `Transform`, `Camera` and `Motion`; `validate_cli.py` validates the envelope |
| M1 CLI acceptance | Automatically tested | `engine_acceptance_m1` runs project create, hierarchy, components, batch undo, byte-identical save/load/save and two identical simulations; 8/8 CTest |
| `--dry-run` | Automatically tested | `validate_cli.py` checks project create, scene create, entity create/delete/reparent, component set/add/remove and batch dry-runs; `project unlock --dry-run` remains manually inspected |
| Generated API reference | Automatically tested | `engine schema --json` exposes 7 components and 8 command schemas; `engine_api_reference` checks `docs/API_REFERENCE.md` against the generator |
| Codex review fixes | Automatically tested, redo manually verified | Symlink-safe exclusive temp files, scene-create overwrite rejection (existing and listed-but-missing), recursive subtree undo, missing sceneId and negative revision validation, and revision bump on component undo; redo is manually verified because the CLI has no redo command |
| M1.10 clean clone | Automatically tested | `scripts/acceptance_m1.sh` cloned public `v0.2.1`, built `linux-debug`, ran 8/8 CTest and `M1 acceptance passed` |
| M1.10 CI | Automatically tested | Main push [40848990449](https://github.com/chl84/3D_game_engine/actions/runs/40848990449) → Linux GCC 14, Linux Clang, Windows MSVC, Quality and Linux ASan/UBSan all green |
| M2.1 headless device | Automatically tested | `engine_render_tests` → 22 assertions in 4 test cases against the local GPU and against lavapipe forced with `VK_ICD_FILENAMES` |
| M2.1 window and surface | Automatically tested locally | `engine_window_smoke` created a hidden SDL3 window, a Vulkan surface and a present queue with zero validation errors; the test reports skipped in headless CI |
| M2.1 validation layers | Automatically tested | The required-mode test fails when `VK_LAYER_KHRONOS_validation` is missing and asserts that the debug messenger counted zero errors |
| M2.1 dependencies | Automatically tested | `engine_dependencies_smoke` includes and links SDL3, VMA and GLM; the vcpkg manifest pins 3.4.18, 3.5.0 and 1.0.3 |
| M2.1 CI | Automatically tested | Manual dispatch [40857973518](https://github.com/chl84/3D_game_engine/actions/runs/40857973518) → Linux GCC 14, Linux Clang and Linux ASan/UBSan 10/10 with render tests passed; Windows MSVC 9/9 with the headless render test skipped because the runner has no Vulkan loader; Quality green |
| M2.1 clean clone | Automatically tested | `scripts/acceptance_m1.sh` cloned `m2/vulkan-renderer`, built `linux-debug`, ran 10/10 CTest including both render tests and `M1 acceptance passed` |
| M2.2 swapchain and frame loop | Automatically tested locally | `engine_swapchain_test` presents frames, follows a hidden-window resize, follows the mapped window and reports zero validation errors |
| M2.2 minimize/restore | Automatically tested where the WM supports it | The test asserts that no frame is presented while SDL reports the window as minimized; a CI step under Xvfb + openbox runs it with `--require-minimize` |
| M2.2 frame synchronization | Automatically tested | The swapchain test presents 16 frames locally and 6 frames under Xvfb with per-frame image-available semaphores and per-swapchain-image render-finished semaphores; the validation layers report no semaphore-reuse errors |
| M2.2 CI | Automatically tested | Manual dispatch [40891052868](https://github.com/chl84/3D_game_engine/actions/runs/40891052868) → Linux GCC 14, Linux Clang and Linux ASan/UBSan 11/11 with the window and swapchain tests skipped headless; Windows MSVC 9/9 with the headless render test skipped; Quality green |
| M2.2 minimize under Xvfb | Automatically tested | The Linux CI jobs run `engine_swapchain_test --require-minimize` under Xvfb + openbox; both report `minimize: renderer reported no frames` and present afterward |
| M2.3 glslc build step | Automatically tested | CMake finds `glslc` and compiles `triangle.vert` and `triangle.frag` to SPIR-V; Linux uses `/usr/bin/glslc`, Windows uses `C:/VulkanSDK/1.4.363.0/Bin/glslc.exe`, and both build logs show `Compiling triangle.vert`, `Compiling triangle.frag` and `Embedding SPIR-V shaders` |
| M2.3 triangle pipeline | Automatically tested | `engine_swapchain_test` records the graphics pipeline and three-vertex draw with `Required` validation and zero errors; the Xvfb CI run presents six frames |
| M2.3 CI | Automatically tested | Manual dispatch [40900651135](https://github.com/chl84/3D_game_engine/actions/runs/40900651135) → Linux GCC 14, Linux Clang and Linux ASan/UBSan 11/11; Windows MSVC 9/9 with `engine_render_tests` passing the camera cases and skipping the device cases; Quality green |
| M2.3 clean clone | Automatically tested | `scripts/acceptance_m1.sh` cloned `m2/vulkan-renderer` at `32341c7`, built with glslc, ran 11/11 CTest including `engine_swapchain_test` and `M1 acceptance passed` |
| M2.3 camera | Automatically tested | `engine_render_tests` includes camera cases for Vulkan clip space and a zero aspect guard |
| M2.4 scene format v2 | Automatically tested | `project_test` loads a v1 scene, upgrades it to v2 on save, round-trips `engine.scene.Mesh`, and rejects Mesh in v1 and scene version 3 |
| M2.4 mesh references | Automatically tested | Scenes store only `meshId`; `engine.builtin.cube` and `engine.builtin.plane` resolve in the renderer without mesh data in the scene file |
| M2.4 double world transform | Automatically tested | `scene_test` stores root entities directly at 8 km and 16 km with 1 mm offsets, checks the double positions, and composes an 8 km hierarchy with a 1 mm child offset; `project_test` round-trips a 16 km position through the scene file |
| M2.4 camera-relative rendering | Automatically tested | `scene_transform_test` builds the 8 km model matrix from the double offset, checks finite values, and verifies the rotation-only camera view |
| M2.4 depth and culling | Automatically tested | `engine_swapchain_test` renders a 8 km scene with a depth buffer and checks that the far cube and ground are visible while an entity behind the camera is culled |
| M2.4 CI | Automatically tested | Manual dispatch [40920229557](https://github.com/chl84/3D_game_engine/actions/runs/40920229557) → Linux GCC 14, Linux Clang and Linux ASan/UBSan 11/11; Windows MSVC 9/9 with camera/transform cases passing and device cases skipped; Quality green |
| M2.4 clean clone | Automatically tested | `scripts/acceptance_m1.sh` cloned `m2/vulkan-renderer` at `d2a9e22`, built with glslc, ran 11/11 CTest and `M1 acceptance passed` |
| M2.5 offscreen render | Automatically tested | `engine_screenshot_golden` renders a 64×64 scene through the headless Vulkan device and compares it to `tests/fixtures/golden/m2_scene_64.png` on lavapipe |
| M2.5 screenshot CLI | Automatically tested | `tests/smoke/screenshot.py` creates a project, camera and cube, runs `engine screenshot --frames 2 --width 64 --height 64`, validates `command-result@1`, and checks the PNG signature and size |
| M2.5 stb dependency | Automatically tested | `stb` 2026-08-02 is pinned in vcpkg; `stb_image_write` is linked only into the CLI/test path and documented in `DEPENDENCIES.md` and `THIRD_PARTY_NOTICES.md` |
| M2.5 CI | Automatically tested | Manual dispatch [40925072019](https://github.com/chl84/3D_game_engine/actions/runs/40925072019) → Linux 13/13 including `engine_screenshot_golden` on lavapipe and `engine_screenshot_cli`; Windows 9/9 with graphics tests skipped; Quality green |
| M2.5 clean clone | Automatically tested | `scripts/acceptance_m1.sh` cloned `m2/vulkan-renderer` at `72d6a92`, built with glslc and stb, ran 13/13 CTest; the golden test skips on the local Intel device and runs on lavapipe CI |
| M2.5 Windows ICD evaluation | Decided and implemented in M2.8 | The Windows runner installs pinned Mesa lavapipe 26.2.6 (`mmozeiko/build-mesa`, SHA-256 verified) and registers its ICD. Windows offscreen golden and screenshot tests run and pass; window/swapchain presentation remains not verified |
| M2.5 Linux glslc evaluation | Decided and implemented in M2.8 | Linux CI caches the LunarG Vulkan SDK 1.4.363.0 and uses its `glslc` (shaderc v2026.4), matching the Windows SDK. The cache is keyed by SDK version; the extracted SDK is reused across Linux jobs |
| M2.6 ImGui dependency | Automatically tested | Dear ImGui 1.93.0-docking is pinned in vcpkg to the exact commit `b916186e66aa3c02b72c6ca17455516512ed8049` with the SHA512 of that commit's archive, and built through `cmake/vcpkg-overlay/imgui`. The overlay keeps SDL3 at `default-features: false` and builds the Vulkan backend against volk; it adds no dbus, systemd, libxcrypt or second Vulkan loader. ADR-0010 records the decision |
| M2.6 ImGui overlay | Automatically tested | `engine_overlay_test` renders four frames with a camera and two meshes, checks the entity list, CPU/GPU frame statistics including at least one GPU timestamp sample, non-empty draw data, unchanged scene revision and camera position, and zero validation errors. Local result: 480 vertices for 3 entities, CPU 1.76 ms, GPU 0.10 ms on Intel Iris Xe |
| M2.6 screenshot sandbox and limits | Automatically tested | `engine screenshot --out` resolves through `PathSandbox`; `../escape.png` is rejected and nothing is written outside the project. Width and height are capped at 8192 and frames at 1000; `--width 8193`, `--height 8193` and `--frames 1001` are rejected. `tests/smoke/screenshot.py` covers all four cases |
| M2.6 CI Xvfb regression | Fixed | The CTest step was never under Xvfb: `engine_window_smoke` was always skipped and `engine_swapchain_test` ran only in the separate `--require-minimize` step. M2.6 runs the whole CTest suite under Xvfb + openbox and keeps the strict minimize step. `VK_ICD_FILENAMES` is selected from `lvp_icd*.json` before the tests |
| M2.6 local suites | Automatically tested | `ci-linux-clang` and `linux-debug` → 14 registered, 13 passed and the golden test skipped without lavapipe; `linux-asan` → same; `linux-tsan` → 11/11 CPU-side tests. Golden image on lavapipe still matches with maximum channel difference 0 |
| M2.6 CI | Automatically tested | Final run [40941397208](https://github.com/chl84/3D_game_engine/actions/runs/40941397208) on `7496661` → all five jobs green. Linux GCC 14 and Clang 18: 14/14 CTest with `engine_window_smoke`, `engine_swapchain_test`, `engine_overlay_test` and `engine_screenshot_golden` passed on lavapipe under Xvfb; the strict minimize step printed `minimize: renderer reported no frames`. ASan/UBSan: 14/14. Windows MSVC: 9/9 with graphics tests not registered. Quality: clang-format and clang-tidy green |
| M2.6 clean clone | Automatically tested | `scripts/acceptance_m1.sh` cloned `m2/vulkan-renderer` at `7496661`, built with the exact-commit overlay port, ran 14 registered tests (13 passed, golden skipped on the local Intel device) and `M1 acceptance passed` |
| M2.6 ImGui exact pin | Verified | The overlay port uses `REF b916186e66aa3c02b72c6ca17455516512ed8049` (the commit behind the annotated `v1.93.0-docking` tag) and the SHA512 of that commit's archive; the non-docking branch is pinned to `c1aaac65e896a45f81f47a4d68f9959e0f86c859`. A moved tag or rewritten commit fails the vcpkg source check. CI [40941397208](https://github.com/chl84/3D_game_engine/actions/runs/40941397208) built the pinned ports on Linux and Windows |
| M2.7 descriptor strategy | Implemented | ADR-0011 is accepted: descriptor indexing (bindless) is a minimum requirement, there is no classic fallback, and BDA is deferred to M5/M8. The support table records Iris Xe and lavapipe |
| M2.7 device capability gate | Automatically tested | `Device::create` queries the required descriptor-indexing features and limits. A missing feature or limit returns `E_UNSUPPORTED`, stage `descriptor-indexing`, with `missingFeatures` and `missingLimits` (name, required, supported). `Device::descriptor_capabilities()` exposes the queried values |
| M2.7 descriptor failure test | Automatically tested | `engine_render_tests` (13 cases / 105 assertions) constructs capabilities with a missing feature and a low limit, asserts `E_UNSUPPORTED`, stage `descriptor-indexing`, `missingFeatures` and the machine-readable limit entry. There is no fallback test because there is no fallback path |
| M2.7 doctor | Automatically tested | `engine doctor` reports `vulkan.descriptorIndexing` and a per-device `descriptorIndexing` object with `errorCode: E_UNSUPPORTED` and missing lists. `--require vulkan` exits 8 when no device satisfies the requirement; an enumerated but unsupported device adds `descriptor-indexing` to `requirements.failed` and makes plain `doctor` exit 8, so the exit code and JSON agree. The doctor schema and contract test cover the new fields |
| M2.7 screenshot error codes | Automatically tested | `tests/smoke/screenshot.py` requires `E_VALIDATION` and the exact `path`, `maxDimension` and `maxFrames` details for `../escape.png`, `--width 8193`, `--height 8193` and `--frames 1001` |
| M2.7 budgets | Documented | Engine budget is 4096 sampled images + 2 samplers and 4096 material records. The 1 GiB texture budget is a sub-budget of the 4 GiB total GPU memory budget |
| M2.8 performance plan | Documented | `docs/PERFORMANCE.md` records the M2 reference budget, the current small-scene baseline and the M5/M8 measurement plan. M2 does not claim reference-scene budget compliance |
| M2.7 validation scenarios | Automatically tested | The swapchain test covers resize, minimize/restore and swapchain recreation with Required validation; the overlay test covers frame rendering with Required validation. Device loss is not simulated because the test harness has no practical way to force it |
| M2.7 CI | Automatically tested | Manual dispatch [40950970681](https://github.com/chl84/3D_game_engine/actions/runs/40950970681) on `dbc313a` → all five jobs green. Linux GCC 14 and Clang 18: 14/14 CTest; lavapipe LLVM 20.1.2 reports `descriptorIndexing: true` and `descriptor-indexing: pass`. ASan/UBSan: 14/14. Windows MSVC: 9/9; graphics tests not registered. Quality green |
| M2.7 clean clone | Automatically tested | `scripts/acceptance_m1.sh` cloned `m2/vulkan-renderer` at `dbc313a`, built the exact-commit dependencies, ran 14 registered tests (13 passed, golden skipped on the local Intel device) and `M1 acceptance passed` |
| M2.8 Windows offscreen ICD | Automatically tested | Windows CI installs `mmozeiko/build-mesa` lavapipe 26.2.6 x64 (SHA-256 verified), registers `lvp_icd.x86_64.json` in the Vulkan loader registry, and runs on `llvmpipe (LLVM 23.1.4)`. `engine_render_tests` is 13 cases / 105 assertions with device cases running; `engine_screenshot_golden` and `engine_screenshot_cli` pass; Windows CTest is 11/11 |
| M2.8 pinned Linux glslc | Automatically tested | Linux CI caches the LunarG Vulkan SDK 1.4.363.0 and uses its `glslc` (`shaderc v2026.4`); `command -v glslc` is asserted to be `$VULKAN_SDK/bin/glslc`. This matches the Windows SDK version |
| M2.8 doctor mixed-device test | Automatically tested | `engine_core_tests` adds mixed supported/unsupported device cases: one supported device passes, all-unsupported fails, and an empty device list remains diagnostic. Core tests are 62 cases / 474 assertions |
| M2.8 CI | Automatically tested | Final run [40959315657](https://github.com/chl84/3D_game_engine/actions/runs/40959315657) on `330e6b5` → all five jobs green. Linux GCC 14 / Clang 18 / ASan: 14/14; Windows MSVC: 11/11 with lavapipe offscreen tests; Quality green |
| M2.8 clean clone | Automatically tested | `scripts/acceptance_m1.sh` cloned `m2/vulkan-renderer` at `330e6b5`, built with pinned dependencies, ran 14 registered tests (13 passed, golden skipped on the local Intel device) and `M1 acceptance passed` |
| M2.9 windowed run | Automatically tested | `engine_run_smoke` creates a project with a camera and cube and runs `engine run --frames 2 --hidden` at 320×240; the command returns `command-result@1` with 2 frames |
| M2.9 presentation input | Implemented; manual test pending | `engine run` supports WASD + Q/E, Shift, right-mouse look, F1 overlay toggle and Escape close. The camera pose is passed to `Renderer::draw_frame` and never written to the scene (ADR-0002) |
| M2.9 overlay toggle and ImGui input | Automatically tested | `engine_overlay_test` toggles the overlay off and on and checks visibility/draw vertices; `Renderer::process_sdl_event` routes SDL events to ImGui. The owner verifies the interactive fly camera and toggle on Hyprland |
| M2.9 CI | Automatically tested | Final run [40963990696](https://github.com/chl84/3D_game_engine/actions/runs/40963990696) on `8bb3c82` → all five jobs green. Linux 15/15 including `engine_run_smoke`; Windows 11/11; ASan/UBSan 15/15; Quality green |
| M2.9 clean clone | Automatically tested | `scripts/acceptance_m1.sh` cloned `m2/vulkan-renderer` at `8bb3c82`, ran 15 registered tests (14 passed, golden skipped on the local Intel device) and `M1 acceptance passed` |
| M3.1 scene format v3 | Automatically tested | Scene format 3 adds `AssetInstance`, `DirectionalLight` and reserved `PointLight`; v1/v2 scenes upgrade to v3, v3 components are rejected in v2, and v4 is rejected. `engine_core_tests` is 63 cases / 500 assertions |
| M3.1 asset schemas | Documented | `schemas/asset@1.schema.json` and `schemas/asset-cooked@1.schema.json` define the approved asset sidecar and cooked manifest, including stable node IDs, original source path and source copy |
| M3.1 fastgltf dependency | Automatically tested | fastgltf 0.9.1 and simdjson 5.0.5 are pinned in vcpkg with overrides; `engine_dependencies_smoke` constructs a `fastgltf::Parser` and passes |
| M3.1 cache and source rules | Documented | `.engine/cache/` is explicitly ignored by git; `asset@1` stores the original reimport source and a project-relative source copy (ADR-0012) |
| M3.1 stb_image approval | Documented | PNG/JPEG only via `STBI_ONLY_PNG`/`STBI_ONLY_JPEG`, 8192×8192 and 256 MiB pre-decode limits, `E_VALIDATION`, and truncated/corrupt/oversized ASan/UBSan tests |
| M3.1 PointLight warnings | Automatically tested | `engine_point_light_warning` checks `project validate`, `engine run` and `engine screenshot` warnings; no silent ignore |
| M3.1 `ilammy` replacement | Automatically tested | The Windows job uses `vswhere` + `vcvarsall.bat x64`; `Verify MSVC` passes `cl.exe`, and the Windows matrix is green |
| M3.1 CI | Automatically tested | Manual dispatch [40975777615](https://github.com/chl84/3D_game_engine/actions/runs/40975777615) on `5d473e6` → all five jobs green. Linux 16/16 including `engine_point_light_warning`; Windows 11/11 with native MSVC setup, fastgltf and scene v3; ASan 16/16; Quality green |
| M3.1 clean clone | Automatically tested | `scripts/acceptance_m1.sh` on `5d473e6` → 16 registered tests (15 passed, golden skipped on local Intel) and `M1 acceptance passed` |
| M3.2 GLB import core | Automatically tested | `engine_asset_tests` imports the Blender 5.2.2 fixtures, checks hierarchy, stable paths, materials, embedded PNG, samplers and animation-channel counts, and composes world transforms in double precision |
| M3.2 malformed input | Automatically tested | Mutated GLBs prove `E_VALIDATION`/`E_UNSUPPORTED` with JSON pointers for truncated containers, invalid JSON, required/optional extensions, external buffers/images, multiple scenes, non-triangle primitives, blended materials, missing attributes, unsupported attributes, cycles and out-of-range indices |
| M3.2 fixture provenance | Documented | `tests/assets/m3/PROVENANCE.md` records the Blender 5.2.2 generator, MIT license and that no third-party model or texture is included |
| M3.2 CI | Automatically tested | Manual dispatch [41019502955](https://github.com/chl84/3D_game_engine/actions/runs/41019502955) on `a19d077` → all five jobs green. Linux 17/17 including `engine_asset_tests`; Windows 12/12; ASan 17/17; Quality green |
| M3.2 clean clone | Automatically tested | `scripts/acceptance_m1.sh` on `a19d077` → 17 registered tests (16 passed, golden skipped on local Intel) and `M1 acceptance passed` |
| M3.3 sidecar and cooked cache | Automatically tested | Import writes `assets/<id>/asset.json`, `source.glb` and `.engine/cache/<id>/<cacheKey16>/`; sidecar and cooked manifest validate against `asset@1` and `engine.asset.cooked@1` |
| M3.3 stable IDs and reimport | Automatically tested | Reimport preserves the asset UUID, node/mesh IDs and a scene `AssetInstance` reference; unchanged reimport is byte-stable and reuses the cache, changed source changes the cache key |
| M3.3 sidecar sandbox | Automatically tested | `sourceCopy` and `cooked.path` from the sidecar are resolved through `PathSandbox`; `../../escape` paths are rejected with `E_VALIDATION` |
| M3.3 image decoding | Automatically tested | PNG/JPEG decode to RGBA8 with 8192×8192 and 256 MiB pre-allocation limits; truncated, corrupt and oversized inputs fail with `E_VALIDATION` |
| M3.3 cooking tangents | Automatically tested | A normal-mapped triangle without TANGENT attributes gets cooked tangents with unit length and handedness |
| M3.3 fuzz targets | Automatically tested | `engine_gltf_fuzz` and `engine_image_fuzz` build with Clang libFuzzer + ASan/UBSan, run in the CI `Fuzz (Clang ASan)` job and use the checked-in fixtures as seed corpus |
| M3.3 CI | Automatically tested | Manual dispatch [41033868190](https://github.com/chl84/3D_game_engine/actions/runs/41033868190) on `9077f19` → all six jobs green, including Linux 17/17, Windows 12/12, ASan 17/17 and the 30-second fuzz job |
| M3.3 clean clone | Automatically tested | `scripts/acceptance_m1.sh` on `9077f19` → 17 registered tests (16 passed, golden skipped on local Intel) and `M1 acceptance passed` |
| M3.4 asset CLI | Automatically tested | `engine_asset_cli` covers `asset import/inspect/validate/reimport/list`, `--dry-run`, schema/error contract, cache state, source-copy fallback and sandbox escape rejection |
| M3.4 MikkTSpace tangents | Automatically tested | Cooking uses pinned MikkTSpace; generated tangents match Blender's export for a baked normal map, split mirrored UVs, handle degenerate UVs and preserve source tangents |
| M3.4 API reference | Automatically tested | `engine_api_reference` checks the generated reference includes the five asset commands |
| M2.2 clean clone | Automatically tested | `scripts/acceptance_m1.sh` cloned `m2/vulkan-renderer` at `e563c9b`, built `linux-debug`, ran 11/11 CTest including `engine_swapchain_test` and `M1 acceptance passed` |
| M1 ASan/UBSan | Automatically tested | `linux-asan` → 8/8 CTest locally |
| M1 TSan | Locally automatically tested | `linux-tsan` → 8/8 CTest |
| EnTT and json-schema-validator | Automatically tested | `engine_dependencies_smoke` creates an EnTT entity and validates a document with json-schema-validator |
| External documentation links | Manually verified | 11 unique external URLs in the documentation returned HTTP 200 on 2026-10-29 |
| ADR set | Complete | `docs/decisions/` contains ADR-0001–0008 and an index; each records status, date, context, decision, consequences and alternatives |
| Third-party notices | Complete | `THIRD_PARTY_NOTICES.md` lists runtime, test and build dependencies with licenses and includes the MIT, BSD-3-Clause and BSL-1.0 texts |
| Commands in documentation | Automatically tested | `engine_version_json` and `engine_cli_contract` run the documented `--version` and `doctor` commands; CTest commands run in CI |
| vcpkg manifest install | Manually verified | First configure installed the pinned dependencies in 51 s |
| Clean clone on Linux (M0.2) | Automatically tested | Clone of `a28b51c`, `cmake --preset linux-debug`, build, `ctest` → 3/3 passed; `engine --version --json` returned valid JSON |
| Clean clone on Linux (M0.3) | Automatically tested | Clone of `c94b617`, `cmake --preset ci-linux-gcc`, build, `ctest` → 3/3 passed; 20 test cases / 126 assertions; `engine --version --json` returned valid JSON |
| Clean clone on Linux (M0.4) | Automatically tested | Clone of `dd3d127`, venv from `requirements-dev.txt`, `cmake --preset ci-linux-gcc`, build, `ctest` → 4/4 passed; `doctor --require vulkan` exit 0 |
| Clean clone on Linux (M0.7) | Automatically tested | `scripts/acceptance_m0.sh` cloned `v0.1.0`, built `linux-debug`, ran 6/6 CTest, and validated `version@1` and `doctor@1`; output ended with `M0 clean-clone acceptance passed` |
| ASan/UBSan (M0.7) | Automatically tested | `linux-asan` → 6/6 CTest locally; CI `Linux ASan/UBSan` → 6/6 |
| TSan (M0.7) | Locally automatically tested | `linux-tsan` → 6/6 CTest |
| Format and tidy (M0.7) | Automatically tested | `clang-format --dry-run --Werror` over all C++ files → clean; `clang-tidy -p build/ci-linux-clang --warnings-as-errors='*'` → exit 0 |
| Windows headless (M0.7) | Automatically tested | CI `Windows (MSVC)` → 6/6 CTest, no GPU; graphics remains not verified |
| Release tag | Published | Annotated tag `v0.1.0` on the M0 release commit, pushed to GitHub |
| Norwegian prompt files excluded | Automatically checked | Clean clone contained no `AGENT_RULES.md`, `FIRST_TASK.md`, `MASTERPROMPT_v0.2.md` or root `VISION.md` |
| Exit code for unknown argument | Manually verified | `engine --bogus` → exit code 2 and usage on stderr |
| Vulkan loader and GPU | Manually verified | `vulkaninfo --summary` → Intel Iris Xe, API 1.4.354, Mesa 26.2.5 |
| Lavapipe | Manually verified | `VK_ICD_FILENAMES=/usr/share/vulkan/icd.d/lvp_icd.json vulkaninfo --summary` → llvmpipe, API 1.4.354 |
| Validation layer | Manually verified | `vulkaninfo --summary` lists `VK_LAYER_KHRONOS_validation` 1.4.363; layer-enabled run exits 0 |
| vcpkg | Manually verified | `vcpkg version` → 2026-09-26; HEAD `0e5eb43501dab17aa055dd54bd7a8c018f330fe2` |
| Windows build | Automatically tested | `ci-windows` in GitHub Actions run [40804123029](https://github.com/chl84/3D_game_engine/actions/runs/40804123029) → configure, build and `ctest` all succeeded with MSVC, vcpkg, CMake 4.4.5 and Ninja 1.13.2; 6/6 tests passed, including `engine_docs_links` and `engine_sensitive_scan` |
| Windows offscreen graphics | Automatically tested | Pinned Mesa lavapipe 26.2.6; Windows CTest 11/11 including the golden, screenshot CLI and device render tests on `llvmpipe (LLVM 23.1.4)` |
| Windows presentation | Not verified | Window, swapchain and overlay tests are not registered on Windows; the runner has no verified presentation path |
| GitHub Actions | Automatically tested | Manual dispatch run [40804123029](https://github.com/chl84/3D_game_engine/actions/runs/40804123029) → Linux GCC 14, Linux Clang, Windows MSVC, Quality and Linux ASan/UBSan all succeeded |
| CI trigger policy | Configured | Feature-branch pushes do not trigger CI; pull requests to `main`, pushes to `main` and manual dispatches run all five jobs |
| clang-format in CI | Automatically tested | `Quality` job, clang-format 18 → no changes |
| clang-tidy in CI | Automatically tested | `Quality` job, clang-tidy 18 with `--warnings-as-errors='*'` → success |
| ASan/UBSan in CI | Automatically tested | `Linux ASan/UBSan` job → 6/6 CTest passed |
| Lavapipe in Linux CI | Automatically tested | `Vulkan doctor detection` step → CI loader 1.3.275, lavapipe true, device `llvmpipe (LLVM 20.1.2, 256 bits)`, `--require vulkan` exit 0. The local loader is 1.4.363 |
| Missing ICD in Linux CI | Automatically tested | `Vulkan doctor missing ICD` step → exit 8 and `vulkan` is present in `requirements.failed`. The test asserts membership, not exact array equality; a local run produced exactly `["vulkan"]` |
| Failure artifacts | Implemented | CTest `Testing/` and doctor JSON are uploaded on failed Linux/Windows/ASan jobs |
| Branch protection | Automatically verified | GitHub API accepted the protection rule for `main`; required checks are the five CI jobs, `strict` is true, force pushes and deletions are disabled. Applied by [setup script](../scripts/setup_branch_protection.sh) |

## M0.3 CI history

The Windows job caught two real portability issues before the milestone closed:

- Run [40789411922](https://github.com/chl84/3D_game_engine/actions/runs/40789411922):
  Windows failed on MSVC C4996 for `gmtime`. The timestamp formatter was
  replaced with a portable civil-date calculation.
- Run [40789699749](https://github.com/chl84/3D_game_engine/actions/runs/40789699749):
  Windows failed a path test because `std::filesystem::canonical` produced a
  different path spelling than `temp_directory_path()`. The test now compares
  path components and the canonical sandbox root.
- Run [40790041428](https://github.com/chl84/3D_game_engine/actions/runs/40790041428):
  Linux GCC, Linux Clang and Windows MSVC all green.
- Run [40791373717](https://github.com/chl84/3D_game_engine/actions/runs/40791373717):
  the `std::chrono::year_month_day` timestamp change and the `SKIP`/summary
  change were green on all three compilers. The Windows summary reported
  21/21 test cases and 132 assertions with no skipped cases, so the symlink
  test ran on the Windows runner.

## M0.4 CI history

- Run [40792296820](https://github.com/chl84/3D_game_engine/actions/runs/40792296820):
  Linux GCC, Linux Clang and Windows MSVC all green. Windows ran 4/4 CTest
  tests, including the CLI contract test, and its core summary reported 21/21
  test cases with no skipped cases.

## M0.5 CI history

- Run [40793337666](https://github.com/chl84/3D_game_engine/actions/runs/40793337666):
  the first quality run failed because CMake's C++ module scanning produced
  `.modmap` arguments that clang-tidy 18 could not read. Module scanning was
  disabled because the engine does not use C++ modules.
- Run [40793839932](https://github.com/chl84/3D_game_engine/actions/runs/40793839932):
  all five jobs green — Linux GCC, Linux Clang, Windows MSVC, Quality and
  Linux ASan/UBSan. Linux detected lavapipe and the missing-ICD path exited 8.
- Run [40794292610](https://github.com/chl84/3D_game_engine/actions/runs/40794292610):
  the same five jobs green after pinning GCC 14 (`ci-linux-gcc14`) and using
  `g++-14` for ASan/UBSan. Lavapipe detection and missing-ICD exit 8 passed
  again.
- Run [40796650668](https://github.com/chl84/3D_game_engine/actions/runs/40796650668):
  all five jobs green after the review fixes. Every job reports
  `cmake version 4.4.5` and Ninja `1.13.2`; clang-tidy uses the corrected
  header filter and exits 0; the Vulkan detection step captures exit 0;
  missing ICD exits 8; ASan/UBSan detects `llvmpipe` under the sanitizers;
  Windows passes 4/4 CTest with 21/21 test cases.
- Run [40797389796](https://github.com/chl84/3D_game_engine/actions/runs/40797389796):
  all five jobs green on the documentation commit (`91a6935`), with the same
  tool versions and Vulkan checks.
- Run [40798154876](https://github.com/chl84/3D_game_engine/actions/runs/40798154876):
  all five jobs green on `ef97fa7` after correcting the CI evidence.

## M0.6 CI history

- Run [40800147659](https://github.com/chl84/3D_game_engine/actions/runs/40800147659):
  all five jobs green. Each Linux and Windows job ran 5/5 CTest tests,
  including `engine_docs_links`; `engine_core_tests` still reports 21 cases /
  132 assertions. Pinned CMake 4.4.5, Ninja 1.13.2, lavapipe detection and
  ASan/UBSan remained green.

## M0.7 CI history

- Run [40802140579](https://github.com/chl84/3D_game_engine/actions/runs/40802140579):
  all five jobs green on `81c4b4c` with 5/5 CTest.
- Run [40802998153](https://github.com/chl84/3D_game_engine/actions/runs/40802998153):
  all five jobs green on `c446c8c` with 6/6 CTest, including the new
  `engine_sensitive_scan`.
- Run [40804123029](https://github.com/chl84/3D_game_engine/actions/runs/40804123029):
  manual dispatch after the trigger change; all five jobs green with 6/6 CTest.

## M1 CI history

- Run [40809628430](https://github.com/chl84/3D_game_engine/actions/runs/40809628430):
  the first M1 quality run failed on a clang-format 18 difference in
  `reflect.hpp`; the header was formatted with clang-format 18.
- Run [40810065232](https://github.com/chl84/3D_game_engine/actions/runs/40810065232):
  clang-tidy 18 flagged unchecked optional access in `project.cpp`; the loader
  now uses a resolved entity handle.
- Run [40810720784](https://github.com/chl84/3D_game_engine/actions/runs/40810720784):
  all five jobs green on `35dec3d` with 7/7 CTest.

## Known issues and gaps

- Branch protection is enabled on `main`. The repository is public, so the
  Free-plan restriction no longer applies.
- The development machine has CMake 4.4.4 while CI pins 4.4.5. A clean local
  build with 4.4.5 passed, so this is a documented version difference, not an
  unverified CI path.
- CI actions were upgraded from `actions/checkout@v4`, `actions/cache@v4` and
  `actions/upload-artifact@v4` to `@v7`, `@v6` and `@v7`. This removes the
  Node 20 deprecation from those actions; `ilammy/msvc-dev-cmd@v1` still emits
  a Node 20 warning. The actions are CI tooling, not runtime dependencies.
- The console log sink is implemented with spdlog but is not automatically
  tested; the JSONL sink is.
- `docs/API_REFERENCE.md` is generated by
  `scripts/generate_api_reference.py` from `engine schema --json`; the
  `engine_api_reference` test fails if it is out of date.
- `--dry-run` is implemented for state-changing offline commands.
- `docs/PROJECT_FORMAT.md` describes the implemented M1 format.
- `engine doctor` reports the current working directory as the project root
  until project-root discovery is added.
- Windows offscreen graphics is verified on lavapipe: the CI job installs
  pinned Mesa lavapipe 26.2.6, registers its ICD, and runs the golden and
  screenshot tests plus the device render tests (11/11). Windows window,
  swapchain and presentation remain unverified and those tests are not
  registered there.
- Scene format v2 adds `engine.scene.Mesh`; v1 scenes load and are upgraded on
  save. The M1 release cannot read v2 scenes. ADR-0009 records the change.
- `Mesh` asset UUIDs are not resolved until the asset pipeline in M3. M2.4
  resolves only `engine.builtin.cube` and `engine.builtin.plane`.
- The full Linux CTest suite runs under Xvfb + openbox in CI, so the window,
  swapchain and overlay tests execute there. A separate step keeps the strict
  `--require-minimize` contract. Windows installs pinned lavapipe and runs the
  offscreen golden and screenshot tests plus the device render tests; window,
  swapchain and overlay remain Linux-only because Windows presentation is not
  verified.
- Descriptor indexing is a hard requirement. A device that lacks it is rejected
  by `Device::create` and by `engine doctor --require vulkan`; there is no
  fallback. A fallback would require a new ADR for a concrete target device.
- Device loss is not simulated in M2.7. Resize, minimize/restore and swapchain
  recreation are covered; device loss remains an M2.8 hardening item where
  practical.
- The device-rejection path is tested through the pure descriptor requirement
  evaluator and error builder. There is no fake-Vulkan end-to-end test that
  drives `select_physical_device` with a synthetic unsupported device.
- TSan covers the CPU-side tests only. The window, swapchain and overlay tests
  are excluded because the Khronos validation layer reports a lock-order
  inversion inside the uninstrumented layer; GPU drivers are not instrumented.
- `engine screenshot --out` is confined to the project sandbox, and width,
  height and frame count have upper bounds.
- The renderer requires Vulkan 1.3 with Synchronization2 and Dynamic
  Rendering. Older devices are rejected with `E_UNSUPPORTED` and the rejected
  devices are listed in the error details.
- volk keeps process-wide function tables, so the engine allows one render
  device per process. Device creation must happen before worker threads start.
- The Vulkan SDK is not bundled. Validation layers and the loader are runtime
  requirements of the debug and test builds.
- The English `VISION.md` translation is complete; the Norwegian master prompt
  remains the source of truth outside the repository.
- **Git history:** rewritten on 2026-10-29 to remove a local absolute path
  that appeared in earlier `docs/STATUS.md` revisions. The current history
  contains no such path. No tokens, private keys, passwords or non-noreply
  email addresses were found in tracked files or history.
- Local review reports under `reviews/` are ignored by git and are not part of
  the repository. They may contain local paths and must not be force-added.
- CI does not run on every feature-branch push. Open a pull request or use
  `gh workflow run ci.yml --ref <branch>` when a branch needs verification.

## Next step

M3.8 is authorized. Complete the TSan-scope correction and Claude re-review,
then publish for exact-source reference CI, extended fuzz/cache verification
and public clean-clone acceptance. Deliver the consolidated A.5 report only
after those gates pass. Merge/tag and M4 remain subject to owner authorization.

## M3.5 implementation and verification

Shared `PbrRenderer` resources serve window and offscreen rendering with
explicit cooked-asset uploads, 4096 sampled images/material records, GPU mip
creation, metallic-roughness/normal/AO/emissive inputs, OPAQUE/MASK, authored
DirectionalLight and Reinhard/sRGB output (ADR-0014). Retirement holds buffers
and descriptor slots until an idle collection point. Budgets count VMA blocks
plus documented conservative native-resource reservations. The descriptor pool
gate is corrected from 4098 to 4099 to include its SSBO.

Cooked scene-root membership is preserved through existing sidecar metadata;
`gltf-import@3` regenerates older caches without changing v1 schemas. Matching-
key reimport also repairs corrupt buffers. CLI placement and automatic project
asset resolution remain M3.6; live control remains M3.7.

Review corrections:

- Scene preparation and overlay budget validation finish after the frame-slot
  fence wait and before acquire. Fence reset occurs immediately before submit.
  Data errors leave the slot reusable; post-acquire Vulkan errors are terminal
  until renderer recreation. Tests recover from missing upload/camera, multiple
  lights and a retired scene asset with Required validation and a 30-second
  timeout. Deliberately reversing the order reproduces the fence hang.
- Golden and Blender-reference comparisons use one strict tolerance: maximum
  byte delta 8, fewer than 0.1% of bytes with delta greater than 3. Removing the
  normal map must fail the reference comparison. The fixed golden is unchanged.
- An analytical reflected-normal probe expects red 137; omitting determinant
  handedness gives 75 and fails. The probe also runs at scale 1e-14; omitting
  shader scale normalization gives 48 and fails. Collapsed nodes are skipped,
  while small uniform scales remain valid under the relative determinant test.
- Four UV coordinates distinguish repeat/clamp/mirror on each axis. Asymmetric
  S/T modes distinguish swapped axes. Both shader mutations now fail at 188
  versus expected 0. Fractional-LOD probes cover all min/mip and mag filters.

| Check | Result / evidence |
|---|---|
| Clang warnings-as-errors and full lavapipe CTest | Original `eceb5f9` CI 20/20; current follow-up 21/21 is captured in the committed evidence below |
| Exact clean clone of `eceb5f9` | Historically 20/20 and CLI acceptance, independently confirmed in review; follow-up `59c2e97` 21/21 and acceptance are captured in the committed evidence below |
| Iris Xe final offscreen controls | Blender image difference 0, analytical red 137, wrap/underflow probes pass; [committed Iris Xe output](verification/M3_ACCEPTANCE_EVIDENCE.txt) |
| Iris Xe window recovery/lifetime | Passed with Required validation; [committed Iris Xe output](verification/M3_ACCEPTANCE_EVIDENCE.txt) |
| Mutations | Wrap removed, S/T swapped and normalization removed each fail; correct artifacts restored; reviewer-verified mutation results summarized in [M3.5 report](verification/M3_5_STATUS_REPORT.md) |
| Static checks | Changed C++ passes clang-tidy 23 and compatible local clang-tidy 18 with warnings-as-errors; clang-format 18, docs/sensitive scan and both shaders' spirv-val pass |
| Asset/fuzz | Asset suite 18 cases / 22,693 assertions; 30-second fuzz targets passed (GLB 4335 runs, image 8640); [asset output](verification/M3_ACCEPTANCE_EVIDENCE.txt) and the linked CI Fuzz job |
| Full local ASan/UBSan on `b417101` | 12/20; eight graphics-related failures with 112-byte unknown-native-module leaks; native source subsequently verified in [diagnosis/evidence](verification/M3_SANITIZER_DIAGNOSIS.md) |
| Local ASan PBR after final test correction | Same two PBR leak failures, no suppression; same source recorded in [committed evidence](verification/M3_SANITIZER_EVIDENCE.txt) |
| Original implementation CI | All six jobs succeeded on `47569ea`, run 41051934696; Ubuntu 20/20 including ASan, Windows 14/14 including PBR lavapipe golden |
| Correction CI | Run 41055106806 on `b417101`: Ubuntu/Windows/Fuzz passed; Quality failed with six clang-tidy 18 optional-access warnings; failure retained in the linked CI history |
| Final correction CI | [Run 41057830415](https://github.com/chl84/3D_game_engine/actions/runs/41057830415) completed/success on `eceb5f9`: all six jobs green; Ubuntu Clang/GCC14/ASan 20/20, Windows MSVC 14/14 including PBR lavapipe golden, Fuzz and Quality success |
| Independent review | GODKJENT (round 3), including a separate GCC clean clone 20/20, Iris Xe controls/window recovery, wrap/underflow/handedness mutation failures and all six CI job results |

The clang-tidy 18 warnings are corrected by copying the terminal optional to a
local checked value and returning local errors immediately after storing them.
This preserves the recovery/terminal behavior; no check is suppressed. Local
clang-tidy 23 had accepted the former member-access flow. Final CI verified
18.1.3 across every C++ source with warnings as errors. Local 18.1.8 also passes
the changed units with
checksum-verified Ubuntu GCC 14 headers (verified locally and then by the linked successful Quality job);
the system GCC 16 headers are too new for that parser.

The formerly unknown local112-byte source is now proved to be Mesa RTASM
arena metadata; see the follow-up diagnosis below. Original Ubuntu ASan passed
20/20; follow-up unsuppressed Ubuntu ASan passes21/21. Windows presentation
remains unverified.
Initial development exposed an embedded-shader inline-symbol collision and an
optional SPIR-V discard capability; unique symbols and explicit SPIR-V 1.5 fixed
both before delivery. No existing tests or validation requirements were weakened.
No new dependency, merge, tag or milestone closure is included.

## Owner-requested M3.4/M3.5 reporting follow-up

At the reporting audit, M3.6 was stopped pending owner assessment (since
approved below). Full retrospective reports are available:
[M3.4](verification/M3_4_STATUS_REPORT.md) and
[M3.5](verification/M3_5_STATUS_REPORT.md).

The 112-byte source is now proved: Mesa 26.2.5 RTASM's process-lifetime arena
metadata, two 56-byte objects from u_mmInit via rtasm_exec_malloc. A standalone
Build-ID-guarded C probe reproduces it after freeing all requested code; an
intentional 73-byte application leak still fails. Exact normalized stack/source
correlation and captured evidence are in the [diagnosis](verification/M3_SANITIZER_DIAGNOSIS.md).
The opt-in local sanitizer preset uses the supported loader-retention switch
and the former documented LSan file; all 21 tests passed with leak detection active. The
function rule is inactive/unverified on available stripped binaries. No
patterns are consumed: retention preserves the arena root. The ordinary preset and reference Ubuntu CI remain unsuppressed.

The former color checks were numeric GPU probes, not a separate fixed PNG.
`engine_pbr_color_golden` now supplies that explicit six-patch PNG test on Linux
and Windows. Local Clang 21/21 and local-policy ASan 21/21 pass. Follow-up
[CI41065543569](https://github.com/chl84/3D_game_engine/actions/runs/41065543569)
on `59c2e97` completed/success in attempt 2: all six jobs green, Ubuntu 21/21
including unsuppressed ASan and Windows MSVC 15/15 including the new color-space
golden on lavapipe. Initial Windows SDK download failed with HTTP 504 before
build; its unchanged retry passed. Technical audit confirmed the source and
policy; required report corrections are applied. Permanent
[CI](verification/M3_CI_EVIDENCE.txt),
[local acceptance](verification/M3_ACCEPTANCE_EVIDENCE.txt) and
[native diagnostic](verification/M3_SANITIZER_EVIDENCE.txt) evidence is committed.
At that reporting SHA, rendering/import runtime and Blender golden were
unchanged. The subsequent owner approval authorizes M3.6, recorded below.
No merge or tag is authorized or performed.

## Owner correction before M3.6 — 2026-10-30

- Emissive textures now cook as sRGB, matching baseColor and glTF 2.0. Factors
  and normal/MR/AO inputs remain linear. Importer@4 invalidates old cooked caches
  while preserving stable IDs and v1 source/cache schemas.
- The fixed color PNG is regenerated: emissive 117, with a deliberately linear
  control 156. Import/cooked metadata tests also cover shared base/emissive
  textures and rejection of mixed sRGB/data uses.
- The inactive suppression file is removed, together with the preset's
  LSAN_OPTIONS reference. The local preset retains only the documented loader
  retention switch and active ASan/UBSan/leak checks. Historical leak evidence
  remains unchanged.
- MikkTSpace, the pinned mmikk commit/zlib license and marked UB-corrected build
  copy are formally owner-approved.
- Local Clang asset/PBR verification and subsequent M3.6 results are recorded
  below as they complete. No merge/tag has been performed.

## M3.6 implementation and local verification

- Shared asset.place@1 creates one Transform/AssetInstance entity after validating
  the referenced cache. CLI supports name, double position and dry-run; placement
  uses project locking and the same command bus as scene batches.
- Undo/redo preserves instance UUID; seeded placement is deterministic. Failed
  transactions roll back. Delete/undo now restores the asset component as well.
- Screenshot/run resolve only referenced assets and upload each UUID once.
  Existing PBR traverses cooked node hierarchies without modifying the scene;
  instances share GPU resources. Missing/stale cache requires explicit reimport.
- The public acceptance runs import/place/two visible instances/render/save/load/
  changed-source reimport, unchanged scene bytes, missing-cache recovery and
  Linux hidden-window frames. Existing v1/v2 migration tests remain unchanged.
- Full local Clang and ASan/UBSan:22/22 on lavapipe. Leak detection remains on,
  local retention has no suppression file. New Windows results are not claimed
  from earlier M3.5 CI. Local static-analysis/docs checks are recorded in
  [M3.6 evidence](verification/M3_6_LOCAL_EVIDENCE.txt).
- [Full A.5 report](verification/M3_6_STATUS_REPORT.md). Exact-SHA CI and
  independent Claude Opus 5.5 high review are the next acceptance gates.
  No M3.7 work, merge or tag has started.

## M3.6 review handled — verification complete

Claude Opus 5.5 high approved `4ce8cac` with no blocking findings. Initial
CI 41073848866 completed/success: all six jobs, Linux 22/22 and Windows 16/16, including
placement and emissive 117. The exact clean public clone also passed 22/22 and
M1 acceptance. Permanent [CI excerpts](verification/M3_6_CI_EVIDENCE.txt) and
[local/clean-clone output](verification/M3_6_LOCAL_EVIDENCE.txt) are committed.

L1 is corrected: upload deduplication and PBR lookup both use canonical UUID
identity, without rewriting scene references. Uppercase-only and mixed-case
regressions pass; each inverse mutation fails with the expected original error.
Clang and local-policy ASan/UBSan both pass22/22; local clang-tidy23 passes.
I1 empty CLI name behavior is documented; I2 historical suppression prose is
clarified; I3 required no code change. This small low-severity correction is
handled directly, with no new delegated review. Final exact-SHA CI 41076353145 on `ef6775a` completed/success: all six jobs,
LinuxClang/GCC14/ASan22/22 and WindowsMSVC16/16, no unexplained skips.
Both lavapipe platforms passed emissive 117 and Blender difference 0. Quality
clang-format18/clang-tidy18 and Fuzz passed. The final corrected clean public
clone passed 22/22 plus M1 acceptance; explicit Iris Xe acceptance also passed.
At the M3.6 handback its consolidated report was ready and M3.7 was unstarted.
M3.7 was subsequently authorized; its current status is recorded below.

## M3.7 implementation and verification

- Foreground live serve plus live call client,127.0.0.1 ephemeral TCP, one
  newline-delimited UTF-8 object/connection. Same shared project command
  registration and bus as CLI; no duplicate authoring implementation.
- Per-request CSPRNG credential; private atomic discovery with pre-write
  permission/owner readback; generic denial before method/protocol details.
- Bounds:64KiB input,4MiB output, depth32,8accepted clients, absolute receive/
  send deadlines; incomplete/oversized/aborted connections close safely.
- Lifetime project writer lock; offline screenshot acquires it, active
  force-unlock is denied. All client paths use PathSandbox, including confined
  source reimport; embedded NUL paths are rejected centrally.
- Live acceptance covers authoring/PNG, auth and private permissions, cap/
  timeout/disconnect, lock conflicts, symlink/path escapes, actual command log,
  rotation/shutdown and shared light/asset undo.
- Permanent engine_rpc_fuzz added to the same CI job as GLB/image fuzzers.
  Local30s results:4994/8284/66839runs, no sanitizer findings.
- Initial local Clang/retention ASan suites passed23/23; review corrections add
  the queued-deadline regression and pass24/24 locally. Reference CI7c24116 now
  passes Linux GCC/Clang/ASan24/24 and Windows18/18 (including live ACL checks);
  Fuzz and Quality passed. All six jobs completed/success.
- [Consolidated report](verification/M3_7_STATUS_REPORT.md) and
  [local evidence](verification/M3_7_LOCAL_EVIDENCE.txt). Review findings and
  required CI are handled; M3.7 awaits owner assessment.

### M3.7 independent review round1

GPT-6 Astra high reviewed6e3fb73 against16c5805: changes required for an MSVC
checked-iterator declaration, clang-format18 compatibility and a queued-client
deadline after a blocking handler. Parent corrections are verified;
engine_local_socket_deadline adds a discriminating cross-platform regression.
The initial CI41091300737 failed Windows compilation and Quality formatting;
those results are not final acceptance evidence. Corrected CI passed all six
jobs with Linux24/24 and Windows18/18. Review/closure details are in
[round1](verification/M3_7_REVIEW_ROUND1.md). The consolidated report is complete.

### M3.7 platform evidence snapshot before Quality completed

Verification source7c241168583e5cd1b37c6b14aa60fd959819753d.
[CI41093710140](https://github.com/chl84/3D_game_engine/actions/runs/41093710140):
GCC, Clang and ASan24/24; Windows18/18; Fuzz success. Windows live/DACL checks,
queued-deadline regression and PBR color golden all passed. At this historical
snapshot Quality was still in progress; final success is recorded below.
Exact-source public clean clone passed24/24 and both acceptance summaries.
Local clang-tidy18 passed all17changed C++ units with compatible GCC14 headers;
its incompatible GCC16-header attempt is recorded honestly.
[CI excerpts](verification/M3_7_CI_EVIDENCE.txt) and
[local evidence](verification/M3_7_LOCAL_EVIDENCE.txt) preserve the proof.
The three independent-review findings are corrected and verified by the parent.
No second independent verdict is claimed. Reporting was held until Quality
completed, as recorded below.

### M3.7 final closure

All six jobs in [CI41093710140](https://github.com/chl84/3D_game_engine/actions/runs/41093710140)
completed/success on7c241168583e5cd1b37c6b14aa60fd959819753d. Linux GCC, Clang
and ASan/UBSan passed24/24; Windows MSVC passed18/18, including live authoring,
ACL checks, queued deadlines and color golden on lavapipe. Quality passed the
reference clang-format18/clang-tidy18 gates. All three fuzz targets passed.
Exact-source public clean clone passed24/24 and both acceptance summaries.

The original independent GPT-6 Astra high review requested three changes.
All are corrected and verified with discriminating tests and reference CI;
no second independent verdict is claimed. The report and permanent evidence
are complete. Documentation commits after7c24116 do not change tested code.
No unresolved confirmed M3.7 source defect is known. Physical Windows
presentation remains unverified; I/O deadlines do not cancel accepted CPU/GPU
commands; the log is bounded in memory. See the full report for limitations.

M3.7 now awaits owner assessment. No merge/tag or M3.8 work is authorized.

### Latest owner clarification: durable journal and mandatory re-review

The earlier Astra reviewer choice came from an explicit owner message. The owner
now restores Claude Opus as reviewer and requires re-review of all three fixes,
especially queued deadlines. Parent-only closure is insufficient for M3.7
approval. Reviewer selection belongs exclusively to the owner.

Implement continuous, synchronized JSONL command journaling under .engine/ in
M3.7, then run combined Claude Opus5.5 high re-review before an owner report.
No interim A.5 report, merge/tag or M3.8 is authorized.

### Claude re-review outcome on76ecc7d

All three original fixes independently hold. Claude requires a new serialization
fail-closed correction (F1), and recommends mode0644 coverage (F2), registered
post-execution fsync fault tests (F3), and non-zero/visible terminal journal
errors (F4). All four were corrected together and independently approved in round2 below.
[Review record](verification/M3_7_CLAUDE_REVIEW_ROUND1.md). Reference totals for
new source: Linux GCC/Clang26, ASan25, Windows19 because the new fault interposer
is Linux non-sanitized only. No existing tests are disabled or weakened.

Follow-up local verification: Clang26/26; ASan/UBSan25/25 with leak checks on.
All four new defect mutations fail the new tests; restored checks pass. New
exact-source CI and Claude re-check were required and are now complete. Evidence is in
[follow-up](verification/M3_7_CLAUDE_FIX_EVIDENCE.txt).

### Historical Claude round2 handback before Quality completed

Claude Opus5.5 high approvesd0797e1 with no required/blocking findings. All three
original fixes hold; F1–F4 are fixed and nine independent mutations are caught.
Independent Clang26/26, ASan25/25, restored full suite26/26. The no-op preload
negative control fails, confirming the fault test needs actual fsync interposition.
The source tree was restored and clean in the isolated worktree.
[Review approval](verification/M3_7_CLAUDE_REVIEW_ROUND2.md) and
[reference job excerpts](verification/M3_7_CLAUDE_CI_EVIDENCE.txt).

Parent verified GCC/Clang26/26, ASan25/25, Windows19/19 and Fuzz in actual logs;
Quality was still running at that snapshot. Exact-source public clean clone26/26
and both acceptance summaries pass. Reporting was held until Quality completed.
Final CI closure and the subsequent owner approval are recorded below.

### M3.7 owner approval and final closure —2026-10-30

The owner approved M3.7 after the mandatory Claude Opus re-reviews. All three
original corrections and the durable-journal F1–F4 follow-ups are independently
approved atd0797e1. CI41108912088 is completed/success, all six jobs green,
including Quality. Actual job totals: GCC/Clang26/26, ASan25/25, Windows19/19.
The new Linux-only non-sanitized fault test explains the count difference; no
existing test is disabled. Fuzz and exact-source public clean clone pass.

[Final CI proof](verification/M3_7_CLAUDE_CI_EVIDENCE.txt),
[Claude technical approval](verification/M3_7_CLAUDE_REVIEW_ROUND2.md) and
[full report](verification/M3_7_STATUS_REPORT.md) are complete. Documentation
commits afterd0797e1 do not change the tested source. The pre-existing non-UTF-8
asset-import orphan-directory case remains documented for hardening assessment.
No new M3.7 blocking/required finding remains. No merge, tag or M3.8 is authorized.

## M3.8 authorized hardening — 2026-10-30

The owner authorized M3.8 after approving M3.7. Local implementation adds:

- One public CLI/live acceptance flow using the original mirrored Blender
  model without tangents and the unchanged M3.5 PBR golden. It checks
  save/load, changed-source reimport, all stable IDs, identical CLI/live pixels
  and actual schema-valid durable JSONL; registered on Linux and Windows.
- Early UTF-8 metadata validation closes the known asset-import leftover-file
  observation. New unit/CLI regressions preserve project files/directories on
  rejection, including a valid alias to an invalid POSIX source filename.
- Optional extended fuzz mode: 600 seconds per GLB/image/RPC target, nightly or
  manual dispatch, cached corpus across successful runs, retained diagnostics.
  Ordinary six-job CI and its short fuzz requirement remain unchanged.
- Current build/testing/dependency/format/architecture/vision/agent docs and an
  explicit clean-clone M3 acceptance entry point. No format, dependency, module
  boundary, shader, golden, tolerance or public command schema is changed.

Local validation is captured in [committed evidence](verification/M3_8_LOCAL_EVIDENCE.txt).
Independent Claude review and new-source reference CI/public clean clone are
still pending at this snapshot. No interim A.5 report is delivered. Review is
performed locally before publishing this delivery for CI. This authorization
does not permit merge/tag or M4 work.

### M3.8 Claude round 1 and corrections

Claude reviewed014ba20 againstafea88a and requested one required Medium
correction: the nine-test TSan selection omitted four viable contracts/device
tests from the intended scope. It must include13tests and build all targets.
The initial nine-test evidence is retained as historical and superseded.
[Review record](verification/M3_8_REVIEW_ROUND1.md).

The correction also retains20minutes for ordinary fuzz CI (55for extended),
documents measured corpus storage without claiming minimization, and makes the
clean-clone script report/reject skipped CTest tests or an empty report.
All TSan targets now build and the corrected scope passes13/13 on lavapipe,
including all internal headless-device cases. Skip/empty-report negative
controls pass. [Correction evidence](verification/M3_8_REVIEW_CORRECTION_EVIDENCE.txt).
Independent re-review is pending; this is not final technical/owner approval.
Source remains unpublished.
