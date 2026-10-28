# Status

**Last updated:** 2026-10-29
**Current milestone:** M0 — project foundation
**Current substep:** M0.3 complete; M0.4 is next

## Completed

| Substeps | Result |
|---|---|
| M0.0 | Vulkan development packages installed by the owner; vcpkg bootstrapped at the pinned commit; git identity configured; `vulkaninfo` and lavapipe verified |
| M0.1 | Repository initialized; MIT license; CMake skeleton with presets; `engine --version --json`; `engine_version_json` smoke test; ADR-0001–0003; initial documentation skeleton |
| M0.2 | vcpkg manifest with nlohmann/json, spdlog, CLI11, `tl::expected` and Catch2; warning flags with `-Werror` in CI presets; clang-format and clang-tidy configuration; ASan/UBSan and TSan presets; Catch2 unit and dependency smoke tests; Windows MSVC + vcpkg CI job moved from M0.5 and green |
| M0.3 | Core foundations: `Result<T>`/`Error` with stable codes, UUID v4 and seeded `UuidGenerator`, `PathSandbox`, JSON `Config`, `Log` with console and JSONL sinks, `log-record@1` schema, and 20 core test cases; Linux and Windows CI green |

## Repository state

- `main`: initial bootstrap commit, pushed to GitHub and set as the default branch.
- `m0/foundation`: all M0 work, pushed; not merged into `main`.
- Branch protection/rulesets are planned for M0.5.

## Verification evidence

| Item | Status | Evidence |
|---|---|---|
| GCC C++20 build | Automatically tested | `cmake --preset ci-linux-gcc`, build, `ctest` → 3/3 passed; `engine_core_tests` → 20 cases / 126 assertions |
| Clang C++20 build | Automatically tested | `cmake --preset ci-linux-clang`, build, `ctest` → 3/3 passed; `engine_core_tests` → 20 cases / 126 assertions |
| ASan + UBSan | Automatically tested | `cmake --preset linux-asan`, build, `ctest` → 3/3 passed |
| TSan | Automatically tested | `cmake --preset linux-tsan`, build, `ctest` → 3/3 passed |
| UUID parsing and seeded generation | Automatically tested | `uuid_test.cpp` → round-trip, invalid input, v4 bits, same seed/stream sequence |
| Path sandbox | Automatically tested | `path_sandbox_test.cpp` → `../`, absolute paths, symlink escapes, missing/non-directory roots |
| Config loading and merge | Automatically tested | `config_test.cpp` → nested values, type errors, missing keys, invalid JSON, recursive merge |
| JSONL logging | Automatically tested | `log_test.cpp` → filtered records, required fields, `fields` object, level names |
| log-record@1 schema | Implemented | `schemas/log-record@1.schema.json`; structural checks in `log_test.cpp`; full JSON Schema validation arrives with the M0.4 schema infrastructure |
| Dependency compile/link | Automatically tested | `engine_dependencies_smoke` includes and links nlohmann/json, spdlog, CLI11, `tl::expected` and Catch2 |
| `engine --version --json` | Automatically tested | `tests/smoke/version_json.py` parsed the JSON and checked required fields |
| clang-format | Manually verified | `clang-format --dry-run --Werror` on all C++ files → no changes |
| clang-tidy | Manually verified | `clang-tidy -p build/ci-linux-clang` on the C++ sources → exit 0; all warnings were in non-user code |
| vcpkg manifest install | Manually verified | First configure installed the pinned dependencies in 51 s |
| Clean clone on Linux (M0.2) | Automatically tested | Clone of `a28b51c`, `cmake --preset linux-debug`, build, `ctest` → 3/3 passed; `engine --version --json` returned valid JSON |
| Clean clone on Linux (M0.3) | Automatically tested | Clone of `c94b617`, `cmake --preset ci-linux-gcc`, build, `ctest` → 3/3 passed; 20 test cases / 126 assertions; `engine --version --json` returned valid JSON |
| Norwegian prompt files excluded | Automatically checked | Clean clone contained no `AGENT_RULES.md`, `FIRST_TASK.md`, `MASTERPROMPT_v0.2.md` or root `VISION.md` |
| Exit code for unknown argument | Manually verified | `engine --bogus` → exit code 2 and usage on stderr |
| Vulkan loader and GPU | Manually verified | `vulkaninfo --summary` → Intel Iris Xe, API 1.4.354, Mesa 26.2.5 |
| Lavapipe | Manually verified | `VK_ICD_FILENAMES=/usr/share/vulkan/icd.d/lvp_icd.json vulkaninfo --summary` → llvmpipe, API 1.4.354 |
| Validation layer | Manually verified | `vulkaninfo --summary` lists `VK_LAYER_KHRONOS_validation` 1.4.363; layer-enabled run exits 0 |
| vcpkg | Manually verified | `vcpkg version` → 2026-09-26; HEAD `0e5eb43501dab17aa055dd54bd7a8c018f330fe2` |
| Windows build | Automatically tested | `ci-windows` in GitHub Actions run [40790041428](https://github.com/chl84/3D_game_engine/actions/runs/40790041428) → configure, build and `ctest` all succeeded with MSVC and vcpkg |
| Windows graphics | Not verified | Out of scope for M2/M3 approval |
| GitHub Actions | Automatically tested | Run [40790041428](https://github.com/chl84/3D_game_engine/actions/runs/40790041428) → Linux GCC, Linux Clang and Windows MSVC all succeeded |

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

## Known issues and gaps

- The M0.2 Linux GCC CI job uses the Ubuntu 24.04 runner default GCC 13.3;
  M0.5 pins GCC 14. This is the only toolchain deviation from the approved CI
  matrix.
- Full JSON Schema validation of `log-record@1` is deferred to M0.4, when the
  schema and contract-test infrastructure is introduced. M0.3 checks the
  required fields structurally.
- The console log sink is implemented with spdlog but is not automatically
  tested; the JSONL sink is.
- `docs/VISION.md` is a partial translation; B.1 and B.2 are present, B.3–B.13
  are planned for M0.6.
- `docs/ARCHITECTURE.md`, `docs/TESTING.md`, `docs/DEPENDENCIES.md`,
  `docs/AI_AGENT_GUIDE.md` and `docs/PROJECT_FORMAT.md` are skeletons or drafts.
- volk and `engine doctor` are not implemented; they belong to M0.4.

## Planned for M0.5

- clang-format and clang-tidy checks in CI, so they are automatically tested
  instead of manually verified.
- An ASan/UBSan Linux CI job.
- Linux CI matrix, caching, branch protection and artifacts.
- The Windows job from M0.2 stays green.

## Next step

M0.4: CLI skeleton and `engine doctor --json` — CLI11-based command dispatch,
version and doctor commands, JSON schemas for both, contract tests, and Vulkan
detection through volk (including lavapipe and validation layers).
