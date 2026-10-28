# ADR-0002: Authoritative state versus simulation state

- **Status:** Accepted
- **Date:** 2026-10-29

## Context

The vision says that all changes to project, scene and asset state go through
serializable commands. A literal reading would also apply to every transform
change during a simulation tick. That would require schema validation, command
logging and undo bookkeeping at 60 Hz for potentially thousands of entities,
which conflicts with the 16.6 ms frame budget and the 4 GB memory budget.

The engine also needs deterministic, replayable simulation and a renderer that
never owns game state.

## Decision

Separate two kinds of state:

- **Authoritative state:** project, scene, entities, component values, assets,
  prefabs and structural changes. All changes go through the command bus, are
  validated, logged and undoable.
- **Simulation state:** transforms, velocities, physics, animation and other
  per-tick values. Deterministic systems mutate this state directly during a
  fixed tick. It is reproduced from recorded input, explicit RNG seeds and
  snapshots.

Gameplay code may request structural changes (spawn, despawn, component
changes) through the command bus. Continuous per-tick movement uses the runtime
API. The renderer receives a read-only view and never mutates state.

## Consequences

- The command log stays small enough for AI traces and replay.
- The fixed-tick simulation can meet the frame budget.
- The command bus is the only authoring path for CLI, editor, live API and MCP.
- Lua and C++ gameplay APIs need a clear split between structural commands and
  runtime mutation.
- Determinism tests must cover both save/load ordering and the seeded UUID
  generator.

## VISION.md B.2 wording

The English `docs/VISION.md` uses this formulation for B.2:

> All authoritative changes to project, scene, asset and prefab state go
> through serializable commands. Per-tick simulation state is mutated by
> deterministic systems and is reproduced from recorded input, explicit RNG
> seeds and snapshots. Structural changes initiated by gameplay — spawning,
> despawning and component changes — also go through the command bus. The
> renderer reads state and never mutates it.

## Alternatives considered

- **Command per tick:** rejected for performance, log size and validation cost.
- **No command bus for authoring:** rejected because it breaks AI-first
  observability, undo and the single API surface.
