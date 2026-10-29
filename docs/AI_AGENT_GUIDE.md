# AI Agent Guide

**Status:** M0.4. Version and doctor commands exist. The command bus and the
project/scene CLI are implemented in M1.

## Current commands

```bash
engine --version --json
engine doctor --json
engine doctor --require vulkan --json
engine doctor --require gpu --require validation-layers --json
```

Output:

```json
{"name":"engine","version":"0.1.0","gitCommit":"...","buildType":"Debug","compiler":"GNU 16.2.1","platform":"Linux-x86_64","cxxStandard":"20"}
```

- `--json` writes one JSON document to stdout. Diagnostics go to stderr.
- `--version` prints human-readable output.
- `doctor` reports OS, compiler, Vulkan loader, physical devices, validation
  layers, lavapipe and the project root.
- `--require` accepts `vulkan`, `gpu`, `validation-layers` and `lavapipe`.
  A failed requirement exits with code 8; unknown values exit with code 2.
- `--help` prints usage.
- An unknown argument exits with code 2.

The version output validates against `schemas/version@1.schema.json`; the
doctor output validates against `schemas/doctor@1.schema.json`.

## Rules for automation

- Always use `--json` when parsing output.
- Do not parse human-readable output.
- Treat stdout as one JSON document; logs are on stderr.
- Check the exit code before parsing the document.
- Do not rely on memory addresses or internal indices; only stable IDs will be
  exposed from M1 onward.

## Planned commands (M1)

- `project create`
- `scene create`
- `entity create`
- `component set`
- `scene inspect`
- `simulate --ticks N`
- `batch`

The full command list and schemas will be generated in M1 and documented in
`docs/API_REFERENCE.md`.

## Live API

Not implemented. It is planned for M3 as JSON-RPC 2.0 over a local connection.
