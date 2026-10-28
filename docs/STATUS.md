# Status

**Last updated:** 2026-10-29
**Current milestone:** M0 — project foundation
**Current substep:** M0.1 complete; M0.2 is next

## Completed

| Substeps | Result |
|---|---|
| M0.0 | Vulkan development packages installed by the owner; vcpkg bootstrapped at the pinned commit; git identity configured; `vulkaninfo` and lavapipe verified |
| M0.1 | Repository initialized; MIT license; CMake skeleton with presets; `engine --version --json`; `engine_version_json` smoke test; ADR-0001–0003; initial documentation skeleton |

## Verification evidence

| Item | Status | Evidence |
|---|---|---|
| GCC C++20 build | Automatically tested | `cmake --preset linux-debug && cmake --build --preset linux-debug` completed; `ctest --preset linux-debug` → 1/1 passed |
| Clang C++20 build | Automatically tested | `cmake --preset ci-linux-clang && cmake --build --preset ci-linux-clang`; `ctest --preset ci-linux-clang` → 1/1 passed |
| `engine --version --json` | Automatically tested | `tests/smoke/version_json.py` parsed the JSON and checked required fields |
| Clean clone on Linux | Automatically tested | `git clone --branch m0/foundation` → `cmake --preset linux-debug`, build, `ctest` → 1/1 passed; `engine --version --json` returned valid JSON |
| Norwegian prompt files excluded | Automatically checked | Clean clone contained no `AGENT_RULES.md`, `FIRST_TASK.md`, `MASTERPROMPT_v0.2.md` or root `VISION.md` |
| Exit code for unknown argument | Manually verified | `engine --bogus` → exit code 2 and usage on stderr |
| Vulkan loader and GPU | Manually verified | `vulkaninfo --summary` → Intel Iris Xe, API 1.4.354, Mesa 26.2.5 |
| Lavapipe | Manually verified | `VK_ICD_FILENAMES=/usr/share/vulkan/icd.d/lvp_icd.json vulkaninfo --summary` → llvmpipe, API 1.4.354 |
| Validation layer | Manually verified | `vulkaninfo --summary` lists `VK_LAYER_KHRONOS_validation` 1.4.363; layer-enabled run exits 0 |
| vcpkg | Manually verified | `vcpkg version` → 2026-09-26; HEAD `0e5eb43501dab17aa055dd54bd7a8c018f330fe2` |
| Windows build | Not verified | No Windows machine and no CI run yet |
| Windows graphics | Not verified | Out of scope for M2/M3 approval |
| GitHub Actions | Not verified | Workflow exists as a stub; it has not run |

## Known issues and gaps

- vcpkg manifest is not created yet; M0.2 adds the first dependencies.
- `docs/VISION.md` is a partial translation; B.1 and B.2 are present, B.3–B.13
  are planned for M0.6.
- `docs/ARCHITECTURE.md`, `docs/TESTING.md`, `docs/DEPENDENCIES.md`,
  `docs/AI_AGENT_GUIDE.md` and `docs/PROJECT_FORMAT.md` are skeletons or drafts.
- No CI run has happened yet.
- Windows compilation and headless tests are not verified.

## Next step

M0.2: add the vcpkg manifest with the first dependencies (nlohmann/json, spdlog,
CLI11, tl::expected, Catch2, volk), warning flags, clang-format, clang-tidy and
sanitizer presets.
