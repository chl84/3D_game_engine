# AI Agent Guide

**Status:** M1.7. Version, doctor, the offline project/scene/entity/component
CLI and fixed-tick simulation exist. The live API is planned for M3.

## Principles

- Use the CLI or live API, never the editor, for automation.
- Use `--json` whenever output is parsed.
- Check the exit code before parsing JSON.
- Treat stdout as one JSON document; diagnostics and logs go to stderr.
- Use stable UUIDs, never memory addresses or internal indices.
- Do not edit `project.json`, scene files or asset sidecars directly. Use the
  commands, so validation, logging and undo are preserved.
- The command layer is the same for CLI, live API, Lua and the future MCP
  adapter. See `docs/ARCHITECTURE.md`.

## Environment commands

```bash
engine --version --json
engine doctor --json
engine doctor --require vulkan --json
engine doctor --require gpu --require validation-layers --json
engine schema --json
```

The version output validates against `schemas/version@1.schema.json`; the
doctor output validates against `schemas/doctor@1.schema.json`. `engine schema
--json` returns draft-07 schemas for the built-in scene components.

## Offline project commands

```bash
engine project create ./my-game --name "My Game" --json
engine project validate ./my-game --json
engine project unlock ./my-game --force --json
```

A project contains `project.json`, `scenes/main.scene.json`, `assets/`,
`scripts/` and `.engine/`. The project lock prevents two writers at once.
`project unlock --force` removes a stale lock; do not use it while another
engine process is running.

## Offline scene commands

```bash
engine scene create ./my-game --name main --json
engine scene inspect ./my-game --json
engine scene validate ./my-game --json
```

`scene inspect` returns the canonical scene document. `scene validate` checks
the format, component types, duplicate UUIDs, orphaned parents and hierarchy
cycles.

## Offline entity and component commands

```bash
engine entity create ./my-game --name Root --json
engine entity create ./my-game --name Player --parent <root-uuid> --json
engine entity reparent ./my-game --id <entity-uuid> --parent <parent-uuid> --json
engine entity delete ./my-game --id <entity-uuid> --json

engine component set ./my-game --id <entity-uuid> \
  --component engine.scene.Transform \
  --value '{"position":[0,1.7,0],"rotation":[0,0,0,1],"scale":[1,1,1]}' --json
engine component add ./my-game --id <entity-uuid> --component engine.scene.Camera --json
engine component remove ./my-game --id <entity-uuid> --component engine.scene.Camera --json
```

Entity and component commands use the command bus, are validated against their
command schemas and return a `command-result@1` envelope. `entity delete`
destroys the whole subtree.

## Batch execution

```bash
engine batch ./my-game --file changes.json --undo 1 --json
```

`changes.json`:

```json
{
  "commands": [
    { "name": "scene.entity.create@1", "arguments": { "name": "Temporary" } },
    { "name": "scene.entity.create@1", "arguments": { "name": "Player" } }
  ]
}
```

A batch runs in one transaction and rolls back if a command fails. `--undo N`
undoes the last N commands after the transaction commits and before the scene
is saved. This is the offline equivalent of a short editor session.

## Simulation

```bash
engine simulate ./my-game --ticks 60 --json
```

The command loads the scene, runs `ticks` fixed steps at the project tick rate,
and returns a deterministic state dump with entity positions, rotations, scales
and velocities. It does not save the scene; simulation state is not
authoritative. Two identical runs of the same scene produce identical dumps.

## Response envelope

New commands print one document that validates against
`schemas/command-result@1.schema.json`:

```json
{
  "status": "ok",
  "data": {},
  "warnings": []
}
```

On failure, `status` is `error` and the document contains an `error` object
with a stable `code`, `message`, `details` and an optional `hint`. Use the
process exit code as the primary success signal.

## Planned commands

- `engine schema` and generated API reference in M1.8.
- `--dry-run` for state-changing commands in a later M1 substep or M2.
- Live API (JSON-RPC 2.0 over a local connection) in M3.
- MCP adapter in M7.

## Live API and MCP

Not implemented. The live API will expose the same commands plus inspection,
screenshots, statistics and "run N ticks". The MCP adapter will be a thin layer
over the same command bus.
