# AI Agent Guide

**Status:** M0.1. Only the version command exists. The command bus and full CLI
are implemented in M0.4/M1.

## Current commands

```bash
engine --version --json
```

Output:

```json
{"name":"engine","version":"0.1.0","gitCommit":"...","buildType":"Debug","compiler":"GNU 16.2.1","platform":"Linux-x86_64","cxxStandard":"20"}
```

- `--json` writes one JSON document to stdout. Diagnostics go to stderr.
- `--version` prints human-readable output.
- `--help` prints usage.
- An unknown argument exits with code 2.

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
