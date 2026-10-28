# Testing

**Status:** M0.1. One smoke test exists.

## Current tests

| Test | Type | Command |
|---|---|---|
| `engine_version_json` | CLI smoke test | `ctest --preset linux-debug -R engine_version_json` |

The smoke test runs `engine --version --json`, parses stdout as JSON and checks
the `name`, `version` and `cxxStandard` fields plus the presence of build
metadata.

## How to run

```bash
cmake --preset linux-debug
cmake --build --preset linux-debug
ctest --preset linux-debug --output-on-failure
```

Clang:

```bash
cmake --preset ci-linux-clang
cmake --build --preset ci-linux-clang
ctest --preset ci-linux-clang --output-on-failure
```

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

- Windows compilation and headless tests
- Sanitizer builds
- Anything that requires Vulkan device creation
- CI runs
