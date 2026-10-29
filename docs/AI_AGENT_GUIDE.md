# AI Agent Guide

**Status:** M0.6. Version and doctor commands exist. The command bus and the
project/scene CLI are implemented in M1.

## Principles

- Use the CLI or live API, never the editor, for automation.
- Use `--json` whenever output is parsed.
- Check the exit code before parsing JSON.
- Treat stdout as one JSON document; diagnostics and logs go to stderr.
- Use stable UUIDs, never memory addresses or internal indices.
- Prefer `--dry-run` before a state-changing command.
- A command that changes authoritative state is validated, logged and
  undoable where it makes sense.

The full command contract is defined in `docs/API_REFERENCE.md` and the
published schemas. The command layer is the same for CLI, live API, Lua and the
future MCP adapter. See `docs/ARCHITECTURE.md`.

## Current commands

```bash
engine --version --json
engine doctor --json
engine doctor --require vulkan --json
engine doctor --require gpu --require validation-layers --json
```

Example version output:

```json
{"name":"engine","version":"0.1.0","gitCommit":"...","buildType":"Debug","compiler":"GNU 16.2.1","platform":"Linux-x86_64","cxxStandard":"20"}
```

- `--json` writes one JSON document to stdout. Diagnostics go to stderr.
- `--version` prints human-readable output.
- `doctor` reports OS, compiler, Vulkan loader, physical devices, validation
  layers, lavapipe and the project root.
- `--require` accepts `vulkan`, `gpu`, `validation-layers` and `lavapipe`.
  A failed requirement exits with code 8; an unknown value exits with code 2.
- `--help` prints usage.
- An unknown argument exits with code 2.

The version output validates against `schemas/version@1.schema.json`; the
doctor output validates against `schemas/doctor@1.schema.json`. The
`log-record@1` schema is in `schemas/log-record@1.schema.json`.

## Exit codes

| Code | Meaning |
|---|---|
| 0 | Success |
| 1 | Internal error |
| 2 | Usage error, unknown command or invalid argument |
| 3 | Validation error |
| 4 | Not found |
| 5 | Conflict or locked resource |
| 6 | I/O or filesystem error |
| 7 | Unsupported format or version |
| 8 | A required capability is missing |
| 130 | Cancelled by the user |

The full error-code list is part of the public contract and is documented in
`docs/API_REFERENCE.md`.

## Rules for automation

- Always use `--json` when parsing output.
- Do not parse human-readable output.
- Treat stdout as one JSON document; logs are on stderr.
- Check the exit code before parsing the document.
- Do not rely on memory addresses or internal indices; only stable IDs are
  exposed.
- Do not edit `project.json`, scene files or asset sidecars directly. Use the
  commands, so validation, logging and undo are preserved.
- Do not assume an operation exists because it is planned. Check the command
  registry and `docs/API_REFERENCE.md`.

## Planned commands (M1)

- `project create`
- `project validate`
- `project unlock --force`
- `scene create`
- `scene validate`
- `scene inspect`
- `entity create`
- `entity delete`
- `component set`
- `simulate --ticks N`
- `batch`

The full command list and schemas are generated in M1 and documented in
`docs/API_REFERENCE.md`. Every command that changes state gets `--dry-run`.

## Live API

Not implemented. It is planned for M3 as JSON-RPC 2.0 over a local connection.
It exposes the same commands as the CLI, plus inspection, screenshots,
statistics and "run N ticks". Only local connections are allowed.

## MCP

Not implemented. The adapter is planned for M7, after the command API is
stable. It is a thin layer over the command bus and does not add a second
implementation of any operation.
