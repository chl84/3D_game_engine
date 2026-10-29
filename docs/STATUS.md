# Status

**Last updated:** 2026-10-29
**Current milestone:** M2 — Vulkan renderer
**Current substep:** M2.5 implemented — offscreen rendering, `engine screenshot`, PNG output, golden images and the `stb_image_write` dependency are on `m2/vulkan-renderer`. Reviewer and owner approval are pending.

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
| M2.5 | Offscreen rendering, `engine screenshot`, PNG, golden images and Windows ICD follow-up | Implemented — review pending |
| M2.6 | ImGui debug overlay | Planned |
| M2.7 | Descriptor strategy and validation hardening | Planned |
| M2.8 | Hardening and M2 acceptance | Planned |

## Repository state

- `main`: the M1.10 release `fffe346`, merged from `m1.10/maintenance` on
  2026-10-29 and tagged `v0.2.1`.
- `m2/vulkan-renderer`: the M2 implementation branch; not merged.
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
| GCC 14 C++20 build | Automatically tested | `cmake --preset ci-linux-gcc14`, build, `ctest` → 13/13 passed; `engine_core_tests` → 60 cases / 468 assertions |
| Clang C++20 build | Automatically tested | `cmake --preset ci-linux-clang`, build, `ctest` → 13/13 passed; `engine_core_tests` → 60 cases / 468 assertions |
| ASan + UBSan | Automatically tested | `cmake --preset linux-asan`, build, `ctest` → 13/13 passed locally; the CI `Linux ASan/UBSan` job also runs `doctor --require vulkan` under the sanitizers |
| TSan | Locally automatically tested (not in CI) | `cmake --preset linux-tsan`, build, `ctest` → 13/13 passed |
| UUID parsing and seeded generation | Automatically tested | `uuid_test.cpp` → round-trip, invalid input, v4 bits, same seed/stream sequence |
| Path sandbox | Automatically tested | `path_sandbox_test.cpp` → `../`, absolute paths, symlink escapes, missing/non-directory roots |
| Config loading and merge | Automatically tested | `config_test.cpp` → nested values, type errors, missing keys, invalid JSON, recursive merge |
| JSONL logging | Automatically tested | `log_test.cpp` → filtered records, required fields, `fields` object, level names |
| log-record@1 schema | Automatically tested | `tests/contract/validate_cli.py` validates `tests/fixtures/log-record.jsonl` against `schemas/log-record@1.schema.json`; `log_test.cpp` checks the C++ output structurally |
| UTC timestamp formatting | Automatically tested | `std::chrono::year_month_day` and `hh_mm_ss`; tests cover epoch, leap day, month boundary and year boundary |
| Symlink test on Windows | Automatically tested | Run [40792296820](https://github.com/chl84/3D_game_engine/actions/runs/40792296820) Windows summary reports 21/21 test cases and 132 assertions with no skipped cases, so the symlink test ran. `SKIP` is used if symlink creation is unavailable, and `ctest -R engine_core_tests -V` in CI displays skipped cases |
| Dependency compile/link | Automatically tested | `engine_dependencies_smoke` includes and links nlohmann/json, spdlog, CLI11, `tl::expected`, Catch2 and volk |
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
| Fixed-tick simulation | Automatically tested | `simulation_test.cpp` runs 60/120 ticks at 60 Hz, checks motion, unchanged entities and byte-identical dumps from two identical runs; `validate_cli.py` compares two CLI simulation dumps; 60 core cases / 468 assertions |
| Component schemas | Automatically tested | `engine schema --json` returns draft-07 schemas for `Transform`, `Camera` and `Motion`; `validate_cli.py` validates the envelope |
| M1 CLI acceptance | Automatically tested | `engine_acceptance_m1` runs project create, hierarchy, components, batch undo, byte-identical save/load/save and two identical simulations; 8/8 CTest |
| `--dry-run` | Automatically tested | `validate_cli.py` checks project create, scene create, entity create/delete/reparent, component set/add/remove and batch dry-runs; `project unlock --dry-run` remains manually inspected |
| Generated API reference | Automatically tested | `engine schema --json` exposes 3 components and 8 command schemas; `engine_api_reference` checks `docs/API_REFERENCE.md` against the generator |
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
| M2.5 offscreen render | Automatically tested | `engine_screenshot_test` renders a 64×64 scene through the headless Vulkan device and compares it to `tests/fixtures/golden/m2_scene_64.png` on lavapipe |
| M2.5 screenshot CLI | Automatically tested | `tests/smoke/screenshot.py` creates a project, camera and cube, runs `engine screenshot --frames 2 --width 64 --height 64`, validates `command-result@1`, and checks the PNG signature and size |
| M2.5 stb dependency | Automatically tested | `stb` 2026-08-02 is pinned in vcpkg; `stb_image_write` is linked only into the CLI/test path and documented in `DEPENDENCIES.md` and `THIRD_PARTY_NOTICES.md` |
| M2.5 CI | Automatically tested | Manual dispatch [40923042186](https://github.com/chl84/3D_game_engine/actions/runs/40923042186) → Linux 13/13 including `engine_screenshot_golden` on lavapipe and `engine_screenshot_cli`; Windows 9/9 with graphics tests skipped; Quality green |
| M2.5 clean clone | Automatically tested | `scripts/acceptance_m1.sh` cloned `m2/vulkan-renderer` at `67e7ec1`, built with glslc and stb, ran 13/13 CTest; the golden test skips on the local Intel device and runs on lavapipe CI |
| M2.5 Windows ICD evaluation | Evaluated | The Windows runner has loader and validation layers but no ICD; lavapipe/SwiftShader installation is deferred to M2.8. Windows graphics remains not verified |
| M2.5 Linux glslc evaluation | Evaluated | The LunarG Linux SDK is 366 MB per CI job; the Ubuntu distro `glslc` is kept for M2. Pinned SDK glslc is proposed for M2.8 with caching |
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
| Windows graphics | Not verified | The Windows runner has no GPU; graphics is not tested there and is outside the M2/M3 approval |
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
- Windows graphics is **not verified**: the runner has no GPU. The Windows CI
  job compiles the render module and runs the headless tests; the device cases
  skip when the Vulkan loader or an ICD is missing. The window and swapchain
  tests are not registered on Windows.
- Scene format v2 adds `engine.scene.Mesh`; v1 scenes load and are upgraded on
  save. The M1 release cannot read v2 scenes. ADR-0009 records the change.
- `Mesh` asset UUIDs are not resolved until the asset pipeline in M3. M2.4
  resolves only `engine.builtin.cube` and `engine.builtin.plane`.
- The window smoke test is skipped in headless CI. It runs locally and on any
  Linux machine with a display server.
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

M2.1 review, then M2.2 — swapchain and frame loop. Merging to `main`, creating
or pushing a tag and closing M2 as `v0.3.0` require the owner's explicit
approval. The agent does not merge, tag or use `--admin` without that approval.
