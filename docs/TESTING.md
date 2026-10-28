# Testing

**Status:** M0.3. Three CTest tests exist; `engine_core_tests` contains 20 test
cases and 126 assertions.

## Current tests

| Test | Type | Labels |
|---|---|---|
| `engine_core_tests` | Catch2 unit test | `unit` |
| `engine_dependencies_smoke` | Catch2 compile/link smoke test for all M0 dependencies | `unit`, `dependencies` |
| `engine_version_json` | CLI JSON smoke test | `unit`, `cli` |

`engine_version_json` runs `engine --version --json`, parses stdout as JSON and
checks the `name`, `version` and `cxxStandard` fields plus the presence of build
metadata. `engine_dependencies_smoke` includes and links nlohmann/json, spdlog,
CLI11, `tl::expected` and Catch2, so missing include paths or link errors fail
on both Linux and Windows.

`engine_core_tests` covers:

- `Result<T>`/`Error` codes, JSON serialization and exit-code mapping
- UUID parsing, formatting, v4 bits and deterministic seeded generation
- filesystem sandbox escapes, symlinks and invalid roots
- JSON configuration loading, type errors and recursive merge
- JSONL logging, level filtering and required record fields

The `log-record@1` schema is in `schemas/log-record@1.schema.json`. M0.3 checks
the required fields structurally; full JSON Schema validation is added with the
M0.4 schema infrastructure.

## How to run

```bash
export VCPKG_ROOT="$HOME/.local/share/vcpkg"
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
- Anything that requires Vulkan device creation
- Full JSON Schema validation of `log-record@1`; it is added with the M0.4
  schema infrastructure
- clang-format and clang-tidy in CI; they run locally and are added to CI in
  M0.5
- ASan/UBSan as a Linux CI job; the local job runs, and the CI job is added in
  M0.5
