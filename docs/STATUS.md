# Status

**Last updated:** 2026-10-29
**Current milestone:** M0 — project foundation
**Current substep:** M0.2 complete locally; M0.3 is next

## Completed

| Substeps | Result |
|---|---|
| M0.0 | Vulkan development packages installed by the owner; vcpkg bootstrapped at the pinned commit; git identity configured; `vulkaninfo` and lavapipe verified |
| M0.1 | Repository initialized; MIT license; CMake skeleton with presets; `engine --version --json`; `engine_version_json` smoke test; ADR-0001–0003; initial documentation skeleton |
| M0.2 | vcpkg manifest with nlohmann/json, spdlog, CLI11, `tl::expected` and Catch2; warning flags with `-Werror` in CI presets; clang-format and clang-tidy configuration; ASan/UBSan and TSan presets; Catch2 unit and dependency smoke tests; Windows MSVC + vcpkg CI job moved from M0.5 |

## Verification evidence

| Item | Status | Evidence |
|---|---|---|
| GCC C++20 build | Automatically tested | `cmake --preset ci-linux-gcc`, build, `ctest` → 3/3 passed |
| Clang C++20 build | Automatically tested | `cmake --preset ci-linux-clang`, build, `ctest` → 3/3 passed |
| ASan + UBSan | Automatically tested | `cmake --preset linux-asan`, build, `ctest` → 3/3 passed |
| TSan | Automatically tested | `cmake --preset linux-tsan`, build, `ctest` → 3/3 passed |
| Dependency compile/link | Automatically tested | `engine_dependencies_smoke` includes and links nlohmann/json, spdlog, CLI11, `tl::expected` and Catch2 |
| `engine --version --json` | Automatically tested | `tests/smoke/version_json.py` parsed the JSON and checked required fields |
| clang-format | Manually verified | `clang-format --dry-run --Werror` on all C++ files → no changes |
| clang-tidy | Manually verified | `clang-tidy -p build/ci-linux-clang` on the C++ sources → exit 0; all warnings were in non-user code |
| vcpkg manifest install | Manually verified | First configure installed the pinned dependencies in 51 s |
| Clean clone on Linux | Automatically tested (M0.1) | Clone of `m0/foundation`, configure, build, `ctest` → 1/1 passed at M0.1 |
| Norwegian prompt files excluded | Automatically checked | Clean clone contained no `AGENT_RULES.md`, `FIRST_TASK.md`, `MASTERPROMPT_v0.2.md` or root `VISION.md` |
| Exit code for unknown argument | Manually verified | `engine --bogus` → exit code 2 and usage on stderr |
| Vulkan loader and GPU | Manually verified | `vulkaninfo --summary` → Intel Iris Xe, API 1.4.354, Mesa 26.2.5 |
| Lavapipe | Manually verified | `VK_ICD_FILENAMES=/usr/share/vulkan/icd.d/lvp_icd.json vulkaninfo --summary` → llvmpipe, API 1.4.354 |
| Validation layer | Manually verified | `vulkaninfo --summary` lists `VK_LAYER_KHRONOS_validation` 1.4.363; layer-enabled run exits 0 |
| vcpkg | Manually verified | `vcpkg version` → 2026-09-26; HEAD `0e5eb43501dab17aa055dd54bd7a8c018f330fe2` |
| Windows build | Not verified | The `ci-windows` job exists; it has not run yet |
| Windows graphics | Not verified | Out of scope for M2/M3 approval |
| GitHub Actions | Not verified | The Linux job is pushed; the Windows job is new; neither result is recorded yet |

## Known issues and gaps

- The Windows MSVC + vcpkg CI job has not run yet. Its first run is the proof
  for MSVC warnings and dependency builds.
- `docs/VISION.md` is a partial translation; B.1 and B.2 are present, B.3–B.13
  are planned for M0.6.
- `docs/ARCHITECTURE.md`, `docs/TESTING.md`, `docs/DEPENDENCIES.md`,
  `docs/AI_AGENT_GUIDE.md` and `docs/PROJECT_FORMAT.md` are skeletons or drafts.
- No GitHub Actions result has been recorded yet.
- volk and `engine doctor` are not implemented; they belong to M0.4.

## Next step

M0.3: core foundations — logging (console and JSONL), configuration, filesystem
sandbox, UUID v4 plus the seeded generator from ADR-0003, `Result<T>`/`Error`,
version metadata and unit tests.
