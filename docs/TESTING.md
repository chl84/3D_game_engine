# Testing

**Status:** M0.5. Four CTest tests exist; `engine_core_tests` contains 21 test
cases and 132 assertions. CI also runs clang-format, clang-tidy and ASan/UBSan.

## Current tests

| Test | Type | Labels |
|---|---|---|
| `engine_core_tests` | Catch2 unit test | `unit` |
| `engine_dependencies_smoke` | Catch2 compile/link smoke test for all M0 dependencies | `unit`, `dependencies` |
| `engine_version_json` | CLI JSON smoke test | `unit`, `cli` |
| `engine_cli_contract` | JSON Schema contract test for version and doctor output | `contract`, `cli` |

`engine_version_json` runs `engine --version --json`, parses stdout as JSON and
checks the `name`, `version` and `cxxStandard` fields plus the presence of build
metadata. `engine_cli_contract` validates `engine --version --json` against
`version@1` and `engine doctor --json` against `doctor@1`, checks the
`log-record@1` fixture, and verifies the `--require` exit codes.
`engine_dependencies_smoke` includes and links nlohmann/json, spdlog, CLI11,
`tl::expected`, Catch2 and volk, so missing include paths or link errors fail
on both Linux and Windows.

`engine_core_tests` covers:

- `Result<T>`/`Error` codes, JSON serialization and exit-code mapping
- UUID parsing, formatting, v4 bits and deterministic seeded generation
- filesystem sandbox escapes, symlinks and invalid roots
- JSON configuration loading, type errors and recursive merge
- JSONL logging, level filtering and required record fields
- UTC timestamp formatting at the epoch, leap day, month boundary and year boundary

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
| `Quality` | clang-format 18 and clang-tidy 18 with warnings as errors |
| `Linux ASan/UBSan` | ASan + UBSan build and CTest |

The Linux jobs install `mesa-vulkan-drivers` and `libvulkan1`, verify that
`engine doctor --require vulkan --json` reports `llvmpipe` and exits 0, and
verify that a run with `VK_ICD_FILENAMES`/`VK_DRIVER_FILES` pointing to a
missing file exits 8. This also makes lavapipe available for the M2 golden
image tests.

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
