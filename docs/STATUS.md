# Status

**Last updated:** 2026-10-29
**Current milestone:** M0 — project foundation
**Current substep:** M0.7 deliverables complete — clean clone, sanitizers, format/tidy, Windows headless CI, sensitive-data scan and tag `v0.1.0` are in place. Awaiting owner approval before merge to `main`.

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
| M0.7 | Clean-clone acceptance script, ASan/UBSan, TSan, format/tidy, Windows headless CI, sensitive-data scan, tag `v0.1.0` and the M0 acceptance report. Deliverables complete; awaiting owner approval |

## Repository state

- `main`: initial bootstrap commit, pushed to GitHub and set as the default branch.
- `m0/foundation`: all M0 work, pushed; not merged into `main`.
- The repository is public, and branch protection is **enabled** on `main` via
  `scripts/setup_branch_protection.sh`. Required checks are
  `Linux (ci-linux-gcc14)`, `Linux (ci-linux-clang)`, `Windows (MSVC)`,
  `Quality` and `Linux ASan/UBSan`; force pushes and deletions are disabled,
  and conversations must be resolved before merging.

## Verification evidence

| Item | Status | Evidence |
|---|---|---|
| GCC 14 C++20 build | Automatically tested | `cmake --preset ci-linux-gcc14`, build, `ctest` → 6/6 passed; `engine_core_tests` → 21 cases / 132 assertions |
| Clang C++20 build | Automatically tested | `cmake --preset ci-linux-clang`, build, `ctest` → 6/6 passed; `engine_core_tests` → 21 cases / 132 assertions |
| ASan + UBSan | Automatically tested | `cmake --preset linux-asan`, build, `ctest` → 6/6 passed locally; the CI `Linux ASan/UBSan` job also runs `doctor --require vulkan` under the sanitizers |
| TSan | Locally automatically tested (not in CI) | `cmake --preset linux-tsan`, build, `ctest` → 6/6 passed |
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
| Sensitive-data scan | Automatically tested | `engine_sensitive_scan` checks tracked files for private keys, token formats, credential assignments, absolute home paths, Windows user paths, email addresses and SSH public keys; 71 tracked files passed |
| External documentation links | Manually verified | 11 unique external URLs in the documentation returned HTTP 200 on 2026-10-29 |
| ADR set | Complete | `docs/decisions/` contains ADR-0001–0006 and an index; each records status, date, context, decision, consequences and alternatives |
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
| Release tag | Created locally | Annotated tag `v0.1.0` on the M0.7 commit; pushed after reviewer approval |
| Norwegian prompt files excluded | Automatically checked | Clean clone contained no `AGENT_RULES.md`, `FIRST_TASK.md`, `MASTERPROMPT_v0.2.md` or root `VISION.md` |
| Exit code for unknown argument | Manually verified | `engine --bogus` → exit code 2 and usage on stderr |
| Vulkan loader and GPU | Manually verified | `vulkaninfo --summary` → Intel Iris Xe, API 1.4.354, Mesa 26.2.5 |
| Lavapipe | Manually verified | `VK_ICD_FILENAMES=/usr/share/vulkan/icd.d/lvp_icd.json vulkaninfo --summary` → llvmpipe, API 1.4.354 |
| Validation layer | Manually verified | `vulkaninfo --summary` lists `VK_LAYER_KHRONOS_validation` 1.4.363; layer-enabled run exits 0 |
| vcpkg | Manually verified | `vcpkg version` → 2026-09-26; HEAD `0e5eb43501dab17aa055dd54bd7a8c018f330fe2` |
| Windows build | Automatically tested | `ci-windows` in GitHub Actions run [40800147659](https://github.com/chl84/3D_game_engine/actions/runs/40800147659) → configure, build and `ctest` all succeeded with MSVC, vcpkg, CMake 4.4.5 and Ninja 1.13.2; 5/5 tests passed, including `engine_docs_links` |
| Windows graphics | Not verified | The Windows runner has no GPU; graphics is not tested there and is outside the M2/M3 approval |
| GitHub Actions | Automatically tested | Run [40800147659](https://github.com/chl84/3D_game_engine/actions/runs/40800147659) → Linux GCC 14, Linux Clang, Windows MSVC, Quality and Linux ASan/UBSan all succeeded |
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
  all five jobs green on the documentation commit (`3381761`), with the same
  tool versions and Vulkan checks.
- Run [40798154876](https://github.com/chl84/3D_game_engine/actions/runs/40798154876):
  all five jobs green on `9915170` after correcting the CI evidence.

## M0.6 CI history

- Run [40800147659](https://github.com/chl84/3D_game_engine/actions/runs/40800147659):
  all five jobs green. Each Linux and Windows job ran 5/5 CTest tests,
  including `engine_docs_links`; `engine_core_tests` still reports 21 cases /
  132 assertions. Pinned CMake 4.4.5, Ninja 1.13.2, lavapipe detection and
  ASan/UBSan remained green.

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
- `docs/API_REFERENCE.md` and the command schemas are generated in M1.
  `docs/PROJECT_FORMAT.md` describes the planned M1 format.
- `engine doctor` reports the current working directory as the project root
  until the project format and project-root discovery land in M1.
- The English `VISION.md` translation is complete; the Norwegian master prompt
  remains the source of truth outside the repository.
- **Git history:** earlier revisions of `docs/STATUS.md` (commits `a35cd53`
  through `ed9784d`) contain one local absolute path with the username
  `chrlep`. It was removed from the current file in `66602f4`. No tokens,
  private keys, passwords or non-noreply email addresses were found in tracked
  files or history. Removing the path from history requires a history rewrite
  and force-push, which needs owner approval.
- Local review reports under `reviews/` are ignored by git and are not part of
  the repository. They may contain local paths and must not be force-added.

## Next step

Reviewer approval of M0.7. After approval, the owner decides whether to merge
`m0/foundation` into `main` and publish tag `v0.1.0` as the M0 release. The
agent does not merge to `main` without explicit owner approval.
