# Testing

**Status:** M2.1. Ten CTest tests exist on Linux; Windows registers nine
because the window smoke test is not registered there. `engine_core_tests`
contains 56 test cases and 417 assertions. CI also runs clang-format,
clang-tidy with project headers included, ASan/UBSan, and Vulkan detection with
pinned CMake 4.4.5 and Ninja 1.13.2.

## Current tests

| Test | Type | Labels |
|---|---|---|
| `engine_core_tests` | Catch2 unit test | `unit` |
| `engine_dependencies_smoke` | Catch2 compile/link smoke test for all M0/M1 dependencies | `unit`, `dependencies` |
| `engine_version_json` | CLI JSON smoke test | `unit`, `cli` |
| `engine_cli_contract` | JSON Schema contract test for version and doctor output | `contract`, `cli` |
| `engine_docs_links` | Local Markdown links and the B.10 documentation file list | `docs` |
| `engine_sensitive_scan` | Credentials, private keys, personal paths and email addresses in tracked files | `docs`, `security` |
| `engine_acceptance_m1` | CLI-only M1 acceptance: project, hierarchy, components, undo, round-trip and deterministic simulation | `contract`, `acceptance` |
| `engine_api_reference` | Generated `API_REFERENCE.md` matches `engine schema --json` | `docs`, `contract` |
| `engine_render_tests` | Headless Vulkan 1.3 device, required validation layers, single-device guard and sequential device creation | `unit`, `render` |
| `engine_window_smoke` | SDL3 window, Vulkan surface, present queue and zero validation errors | `unit`, `render` |

`engine_version_json` runs `engine --version --json`, parses stdout as JSON and
checks the `name`, `version` and `cxxStandard` fields plus the presence of build
metadata. `engine_cli_contract` validates `engine --version --json` against
`version@1` and `engine doctor --json` against `doctor@1`, checks the
`log-record@1` fixture, and verifies the `--require` exit codes.
`engine_dependencies_smoke` includes and links nlohmann/json, spdlog, CLI11,
`tl::expected`, Catch2, volk, SDL3, VMA and GLM, so missing include paths or
link errors fail on both Linux and Windows.

`engine_render_tests` creates a headless Vulkan 1.3 device, verifies the
graphics queue and VMA allocator, requires `VK_LAYER_KHRONOS_validation` and
asserts that the debug messenger counts zero errors, checks the single-device
guard, and creates devices sequentially to prove that volk can be initialized
and finalized repeatedly. If the Vulkan loader is missing, the cases are
reported as skipped. `engine_window_smoke` creates an SDL3 window, checks the
surface extension, creates a surface and checks the present queue. It exits
with code 77 when no display server or Vulkan loader is available; CTest
reports that as skipped. The window test is not registered on Windows because
Windows graphics is not verified.

The Linux CI job runs the render tests against the default Mesa device; the
lavapipe golden-image work starts in M2.5.

`engine_core_tests` covers:

- `Result<T>`/`Error` codes, JSON serialization and exit-code mapping
- UUID parsing, formatting, v4 bits and deterministic seeded generation
- filesystem sandbox escapes, symlinks and invalid roots
- JSON configuration loading, type errors and recursive merge
- JSONL logging, level filtering and required record fields
- UTC timestamp formatting at the epoch, leap day, month boundary and year boundary
- reflection registration, field metadata, JSON round-trip, partial updates,
  unknown fields, type errors and generated draft-07 component schemas
- deterministic entity UUIDs, duplicate UUID rejection, parent/child links,
  world transforms, hierarchy cycle rejection and recursive subtree destruction
- command-name validation, JSON Schema argument validation, handler success and
  failure, unknown commands, duplicate registration, command-log entries,
  undo/redo, redo invalidation, transactions and rollback
- project create/load, canonical byte-identical scene round-trip, entity
  hierarchy and components, unknown fields, newer format versions, path
  traversal and exclusive project locks
- CLI project creation, scene inspection/validation, entity hierarchy,
  component setting and batch undo, with every response validated against
  `command-result@1`
- fixed-tick motion, unchanged entities and identical state dumps from two
  identical simulation runs at 60 Hz
- `--dry-run` for state-changing CLI commands, including project create, entity
  create, component set, scene create and batch
- generated API reference parity with the command registry and reflection
  schemas

The `log-record@1` schema is in `schemas/log-record@1.schema.json` and is
validated against `tests/fixtures/log-record.jsonl` by the contract test.
Tests that cannot run on a platform use Catch2 `SKIP`, and the CI
`Core test summary` step runs `ctest -R engine_core_tests -V` so skipped cases
are visible instead of being reported as ordinary passes.

## CI jobs

| Job | Purpose |
|---|---|
| `Linux (ci-linux-gcc14)` | GCC 14 build, CTest, lavapipe detection and missing-ICD exit-code test |
| `Linux (ci-linux-clang)` | Clang 18 build, CTest, lavapipe detection and missing-ICD exit-code test |
| `Windows (MSVC)` | MSVC build and headless tests; no GPU |
| `Quality` | clang-format 18 and clang-tidy 18 with warnings as errors; project headers are analysed |
| `Linux ASan/UBSan` | ASan + UBSan build, CTest and Vulkan detection under the sanitizers |

CI runs on pull requests to `main`, pushes to `main` and manual dispatches.
Feature-branch pushes do not trigger CI. To verify a feature branch, open a
pull request or run:

```bash
gh workflow run ci.yml --ref <branch>
```

This keeps the full five-job matrix as the merge gate without running it on
every intermediate commit. `concurrency` cancels superseded runs on the same
ref.

Every job installs CMake 4.4.5 and Ninja 1.13.2 from the official release
archives and fails if the reported versions differ. The Linux jobs install
`mesa-vulkan-drivers`, `libvulkan1`, `vulkan-validationlayers` and the X11 and
Wayland development headers needed to build SDL3. They capture the exit code
from `engine doctor --require vulkan --json` explicitly, require exit 0 and an
`llvmpipe` device, and require exit 8 when `VK_ICD_FILENAMES` and
`VK_DRIVER_FILES` point to a missing file. This also makes lavapipe available
for the M2 golden image tests.

clang-tidy uses `HeaderFilterRegex: '.*/(engine|tools|tests)/.*'`. The paths in
`compile_commands.json` are absolute, so a regex without the leading `.*/`
silently classifies every project header as non-user code. The corrected filter
found `performance-enum-size` in `engine/core/include/engine/core/log.hpp`;
`LogLevel` now uses `std::uint8_t` as its underlying type. A local clang-tidy
run with the corrected filter exits 0, and CI verifies the same with
clang-tidy 18.

## How to run

```bash
export VCPKG_ROOT="$HOME/.local/share/vcpkg"
python3 -m venv .venv
.venv/bin/python -m pip install -r requirements-dev.txt
cmake --preset linux-debug
cmake --build --preset linux-debug
ctest --preset linux-debug --output-on-failure
```

Clang with warnings as errors:

```bash
cmake --preset ci-linux-clang
cmake --build --preset ci-linux-clang
ctest --preset ci-linux-clang --output-on-failure
```

AddressSanitizer + UndefinedBehaviorSanitizer:

```bash
cmake --preset linux-asan
cmake --build --preset linux-asan
ctest --preset linux-asan --output-on-failure
```

ThreadSanitizer:

```bash
cmake --preset linux-tsan
cmake --build --preset linux-tsan
ctest --preset linux-tsan --output-on-failure
```

Documentation links and sensitive-data scan:

```bash
python3 tests/docs/check_links.py
python3 tests/docs/check_sensitive.py
```

The sensitive-data scan reads tracked files only, reports the pattern name and
location without printing the matched text, and fails on private keys, known
token formats, credential assignments, absolute home paths, Windows user paths,
email addresses and SSH public keys. It does not scan git history; history is
reviewed separately before a repository is made public.

Clean-clone M0 acceptance:

```bash
export VCPKG_ROOT="$HOME/.local/share/vcpkg"
scripts/acceptance_m0.sh
```

The script clones the current branch, builds `linux-debug`, runs all five
CTest tests and validates the version and doctor JSON against their schemas.

Clean-clone M1 acceptance:

```bash
export VCPKG_ROOT="$HOME/.local/share/vcpkg"
scripts/acceptance_m1.sh
```

The script runs all seven CTest tests and the CLI-only M1 acceptance script,
which covers project creation, hierarchy, components, batch undo, byte-identical
save/load/save and deterministic simulation.

Windows MSVC + vcpkg:

```powershell
cmake --preset ci-windows
cmake --build --preset ci-windows
ctest --preset ci-windows --output-on-failure
```

The Windows preset requires an MSVC developer environment and
`VCPKG_ROOT` to be set. CI provides both.

## Planned test types

| Type | Purpose | Milestone |
|---|---|---|
| Unit tests | Isolated module logic | M0.2+ |
| Integration tests | Command bus, scene and serialization together | M1 |
| Contract tests | CLI JSON validates against published schemas | M0.4/M1 |
| Round-trip tests | Save → load → save is byte-identical | M1 |
| Determinism tests | Two runs with the same seed produce the same state dump | M1 |
| Sanitizer tests | ASan, UBSan and TSan | M0.2+ |
| Golden image tests | Offscreen rendering against lavapipe references | M2 |
| Vulkan validation tests | Zero validation errors in documented scenarios | M2 |

## Labels

CTest labels are used to select subsets:

- `unit`
- `dependencies`
- `integration`
- `cli`
- `contract`
- `docs`
- `security`
- `roundtrip`
- `determinism`
- `render`
- `golden`
- `slow`

Tests must not require a GPU, a window or network access unless they are
labelled `render` or `golden`.

## Not verified yet

- Windows graphics; the Windows CI job compiles and runs headless tests, but
  the runner has no GPU
- Anything that requires creating a Vulkan device; `doctor` only creates an
  instance and enumerates devices
- The console log sink; the JSONL sink is automatically tested
- Branch protection enforcement; the GitHub plan blocks it (see `STATUS.md`)
