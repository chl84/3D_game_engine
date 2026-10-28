# Vision

**Status:** Partial M0 translation. B.1 and B.2 are translated below. B.2
includes the clarification accepted in ADR-0002 on 2026-10-29. B.3–B.13 are
pending and will be added in M0.6. Until the translation is complete and
approved, this file is a working draft; the decisions already accepted are
recorded in `docs/decisions/`.

## B.1 Vision

The goal is a standalone 3D game engine in which humans and AI agents together
create complete, playable 3D games.

The engine should be particularly suitable for:

- first-person games
- large, open 3D worlds
- sandbox and simulation games
- exploration and resource gathering
- building and interaction with the environment
- procedurally generated and handcrafted worlds
- physics, vehicles and dynamic objects

The engine must be general and must not be built around the mechanics of one
specific game.

Over time, a user should be able to give an AI agent an instruction such as:

> "Create a first-person game on an alien planet. Generate terrain with
> mountains and valleys, import my Blender models, place buildings in the
> world, add resource gathering and an inventory system, and build a runnable
> game prototype."

The agent should be able to do large parts of this through the engine's
existing APIs and tools, without changing the engine core. The engine must
also be fully usable without AI.

The end goal is an engine that can be extended over time without being rebuilt
for every new game idea.

## B.2 AI-first: core principles

AI-first is an architectural requirement from the first line of code, not a
feature added later. An agent must be able to complete the loop **Create → Run
→ Observe → Test → Correct → Verify** without the graphical editor.

**The command layer is the backbone.** All authoritative changes to project,
scene, asset and prefab state go through serializable commands:

- Each command has a unique, versioned name (for example
  `scene.entity.create@1`) and a JSON Schema.
- The command is validated before it is executed and can be undone where that
  makes sense.
- The result is structured: status, data, machine-readable error codes and
  warnings.
- Multiple commands can be grouped in a transaction that either completes
  entirely or rolls back.
- Every executed command is logged, so a session can be traced and replayed.

The command bus governs authoritative state. Per-tick simulation state is
mutated by deterministic systems and is reproduced from recorded input,
explicit RNG seeds and snapshots. Structural changes initiated by gameplay —
spawning, despawning and component changes — also go through the command bus.
The renderer reads state and never mutates it.

Editor, CLI, Lua, live API and a future MCP adapter use the same command bus.
There are no parallel implementations.

**Reflection is the single source of truth.** Components, fields, types, valid
values and descriptions are registered in one place. From this, serialization,
the object inspector, JSON Schema, Lua bindings and `API_REFERENCE.md` are
generated. The mechanism (for example `entt::meta` or code generation) is
chosen in an ADR.

**Two operating modes:**

- **Offline:** the CLI works directly on project files, without a window or
  GPU. It is used for validation, scene changes, asset import, tests and
  builds.
- **Live:** a running instance (game or editor) offers the same commands, plus
  observation: inspection, screenshots, statistics and "run N ticks". The
  transport is JSON-RPC 2.0 over a local connection. Only local connections
  are allowed.

**CLI requirements:**

- `--json` produces a machine-readable result that validates against a
  published schema.
- Exit codes are documented and stable.
- There are no interactive questions in automation mode.
- `--dry-run` exists for every command that changes state.

**Stable IDs:** entities, assets and prefabs have UUIDs. Memory addresses and
internal indices are never exposed externally.

**Determinism:** fixed simulation tick, RNG with an explicit seed, input
recording and playback, and a headless command that runs N ticks and dumps the
scene state as JSON. The same input, build and platform produce the same
result.

**Observability:** structured logs (JSON lines), error and warning reports,
inspection of scenes, entities and components, screenshots through offscreen
rendering, CPU/GPU statistics and memory and performance reports.

**Safe changes:** validation before execution, clear error messages,
undo/redo, transactions, file access restricted to the project directory,
versioned formats with migration, and recovery from failed operations.

**Model independence:** the engine is not locked to one AI vendor. Because
commands are described with JSON Schema, an MCP adapter becomes a thin layer.
It is built when the command API is stable (M7).

## Pending translation

The remaining sections — B.3 Multiplayer preparation, B.4 Technology stack,
B.5 Modules and dependency boundaries, B.6 Game logic and extensibility,
B.7 Large open worlds, B.8 Blender and the asset pipeline, B.9 Hot reload,
B.10 Documentation, B.11 Testing, B.12 Milestones and B.13 Future features —
are planned for M0.6. They are not included here yet.
