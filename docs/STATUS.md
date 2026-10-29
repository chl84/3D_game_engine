# Status

**Last updated:** 2026-10-29
**Current milestone:** M2 — Vulkan renderer
**Current milestone:** M3 — asset pipeline and live control
**Current substep:** M3.1 implemented — decisions, scene format v3, asset schemas, fastgltf dependency, `ilammy` replacement, `stb_image` approval and PointLight warnings. Reviewer approval is pending before M3.2.

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
| M3.1 | ADRs, scene format v3, asset schemas, fastgltf dependency, `ilammy` replacement and PointLight warnings | Implemented — reviewer pending |
| M3.2 | GLB import core; animation channels reported only | Planned |
| M3.3 | Cooked asset cache, stable IDs and reimport | Planned |
| M3.4 | Asset CLI | Planned |
| M3.5 | PBR renderer, bindless assets and `DirectionalLight` | Planned |
| M3.6 | `AssetInstance` placement and scene v3 save/load | Planned |
| M3.7 | Local JSON-RPC live API | Planned |
| M3.8 | M3 hardening and acceptance | Planned |

## Repository state

- `main`: the M2 release `bb8514c`, merged from `m2/vulkan-renderer` on
  2026-10-30 and tagged `v0.3.0`.
- `m3/plan`: the approved M3 plan, not merged.
- `m3/asset-pipeline`: the M3.1 implementation branch.
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

## Owner approval required

- Merging to `main`, creating or pushing a tag, and using GitHub admin bypass
  (`--admin`) **always require the owner's explicit approval**.
- Reviewer approval covers a substep only; it never authorizes a merge, tag or
  milestone closure.
- When branch protection blocks a merge, stop and ask the owner; do not use
  `--admin`.

## Verification evidence

| Item | Status | Evidence |
|---|---|---|
| GCC 14 C++20 build | Automatically tested | `cmake --preset ci-linux-gcc14`, build, `ctest` → 16 registered; CI runs 16/16 with lavapipe and Xvfb. Locally `linux-debug` passes 15/16 and skips the golden test without lavapipe; `engine_core_tests` → 63 cases / 494 assertions |
| Clang C++20 build | Automatically tested | `cmake --preset ci-linux-clang`, build, `ctest` → 16 registered; locally 15/16 with the golden test skipped without lavapipe. CI runs 16/16 with lavapipe and Xvfb |
| ASan + UBSan | Automatically tested | `cmake --preset linux-asan`, build, `ctest` → 16 registered; locally 15/16 with the golden test skipped. The window, swapchain and overlay tests now execute. The CI `Linux ASan/UBSan` job also runs `doctor --require vulkan` under the sanitizers |
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
| M3.1 scene format v3 | Automatically tested | Scene format 3 adds `AssetInstance`, `DirectionalLight` and reserved `PointLight`; v1/v2 scenes upgrade to v3, v3 components are rejected in v2, and v4 is rejected. `engine_core_tests` is 63 cases / 494 assertions |
| M3.1 asset schemas | Documented | `schemas/asset@1.schema.json` and `schemas/asset-cooked@1.schema.json` define the approved asset sidecar and cooked manifest, including stable node IDs, original source path and source copy |
| M3.1 fastgltf dependency | Automatically tested | fastgltf 0.9.1 and simdjson 5.0.5 are pinned in vcpkg with overrides; `engine_dependencies_smoke` constructs a `fastgltf::Parser` and passes |
| M3.1 cache and source rules | Documented | `.engine/cache/` is explicitly ignored by git; `asset@1` stores the original reimport source and a project-relative source copy (ADR-0012) |
| M3.1 stb_image approval | Documented | PNG/JPEG only via `STBI_ONLY_PNG`/`STBI_ONLY_JPEG`, 8192×8192 and 256 MiB pre-decode limits, `E_VALIDATION`, and truncated/corrupt/oversized ASan/UBSan tests |
| M3.1 PointLight warnings | Automatically tested | `engine_point_light_warning` checks `project validate`, `engine run` and `engine screenshot` warnings; no silent ignore |
| M3.1 `ilammy` replacement | Automatically tested | The Windows job uses `vswhere` + `vcvarsall.bat x64`; `Verify MSVC` passes `cl.exe`, and the Windows matrix is green |
| M3.1 CI | Automatically tested | Manual dispatch [40973936972](https://github.com/chl84/3D_game_engine/actions/runs/40973936972) on `a55d568` → all five jobs green. Linux 15/15; Windows 11/11 with native MSVC setup, fastgltf and scene v3; ASan 15/15; Quality green |
| M3.1 clean clone | Automatically tested | `scripts/acceptance_m1.sh` on `a55d568` → 15 registered tests (14 passed, golden skipped on local Intel) and `M1 acceptance passed` |
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

M2.9 review and the owner's manual `engine run` test on Hyprland. After both,
merge through a normal pull request without `--admin` and tag `v0.3.0`, subject
to the owner's explicit approval. If branch protection blocks the merge, stop
and ask the owner.
