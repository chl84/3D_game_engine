# Architecture

**Status:** M1.7. This document describes the approved module boundaries and
the current implementation. Planned modules are marked as planned.

## Principles

- The engine is AI-first: every authoritative operation is available through a
  serializable command, not only through the editor.
- Authoritative state and per-tick simulation state are separate. See
  ADR-0002.
- The renderer reads state and never mutates it.
- Modules have explicit dependencies and no cycles.
- Reflection is the single source of truth for components, fields, schemas and
  generated documentation.
- The engine is MIT-licensed and uses permissive dependencies.

## Module boundaries

Each module is a separate CMake target with explicit `PUBLIC`/`PRIVATE`
dependencies. Cycles are forbidden.

| Module | Responsibility | May depend on |
|---|---|---|
| `core` | Lifecycle, logging, time, memory, configuration, filesystem, UUID, errors, JSON, reflection, schemas | None |
| `platform` | Window, input, OS integration, file locking, atomic writes | `core` |
| `command` | Command bus, registry, validation, transactions, undo/redo, command log | `core` |
| `scene` | ECS, transform hierarchy, serialization, prefabs, scene commands | `core`, `command` |
| `asset` | Import, cooking, cache, dependency tracking, reimport | `core`, `command`, `scene` |
| `physics` | Jolt integration, collision, raycast, character controller | `core`, `scene` |
| `script` | Lua runtime, sandbox, bindings, events | `core`, `command`, `scene`, `physics` |
| `world` | Chunks, streaming, terrain, persistent edits | `core`, `command`, `scene`, `asset`, `physics` |
| `render` | Vulkan renderer reading scene and world data | `core`, `platform`, `scene`, `asset`, `world` |
| `control` | CLI, live API, later MCP adapter | `core`, `command`, exposed modules |
| `editor` | ImGui editor that changes state only through commands | All modules |
| Game modules | Game-specific C++ and Lua | Public engine API only |

`render` links only the modules that exist at its milestone: M2 uses `core`,
`platform` and `scene`; `asset` is added in M3 and `world` in M5.

The job system belongs in `core` but is deferred until `world` streaming needs
it in M5. M0 and M1 are single-threaded.

## Dependency rules

- Simulation modules (`scene`, `physics`, `script`, `world`) never depend on
  `render`.
- The renderer reads state and never mutates it.
- Authoritative state changes go through the command bus. Per-tick simulation
  state is mutated by deterministic systems. See ADR-0002.
- `command` does not know scene types. Scene modules register their handlers.
- `control` and `editor` are composition roots; game projects depend only on
  the public engine API.
- Static libraries are used in M0–M4. Shared libraries and dynamic game
  modules are decided in a later ADR.
- No global singletons. Contexts and services are passed explicitly.

## Data flow

Authoring path:

```text
CLI / Live API / Lua / Editor / MCP
                 │
                 ▼
          command::CommandBus
        validate → apply → undo/redo
                 │
                 ▼
   scene::SceneEditor / asset / world
                 │
      ┌──────────┴──────────┐
      ▼                     ▼
  project/scene files   runtime state
      │                     │
      └──────────┬──────────┘
                 ▼
         scene::SceneView (read-only)
                 │
                 ▼
          render::Renderer
```

Simulation path:

```text
simulation tick ──► systems (physics, script, animation)
                         │
                         ▼
                 runtime state (ECS)
                         │
                         ▼
                 SceneView → renderer
```

Structural changes initiated by gameplay — spawning, despawning and component
changes — go through the command bus. Continuous movement, physics and
animation mutate the runtime state directly during a fixed tick.

## Command bus

- Every command has a unique, versioned name such as `scene.entity.create@1`
  and a JSON Schema.
- Commands are validated before execution.
- Results contain status, data, machine-readable error codes and warnings.
- Commands can be grouped in a transaction that commits entirely or rolls
  back.
- Every executed command is logged, so a session can be traced and replayed.
- CLI, live API, Lua, editor and a future MCP adapter use the same bus. There
  are no parallel implementations.

The command module contains only the bus, schemas, transactions and log.
`scene`, `asset` and `world` register their own handlers.

## Reflection

Components, fields, types, valid values and descriptions are registered in one
place. Serialization, the object inspector, JSON Schema, Lua bindings and
`API_REFERENCE.md` are generated from that registry. The reflection mechanism
is chosen in M1 and recorded in an ADR.

## Threading

M0 and M1 are single-threaded. `core` owns a future job pool, implemented when
`world` streaming needs it in M5. TSan tests for the job system are added then.
A render thread and asynchronous asset loading are later milestones.

## Error handling

- Expected failures return `Result<T>` (based on `tl::expected`), not
  exceptions.
- Exceptions are used only for fatal initialization failures and programming
  errors, and must not cross module boundaries.
- `Error` has a stable `code`, `message`, `details` and an optional `hint`.
- The CLI and live API map `Error` to JSON and exit codes.

Stable error codes and exit codes are part of the public contract. See
`docs/API_REFERENCE.md`.

## Memory

- CPU: standard allocators and `std::pmr` where useful. No custom allocator
  before profiling shows a need.
- GPU: Vulkan Memory Allocator from M2.
- Explicit contexts and services instead of global state.

## Enforcement

1. Every module is a CMake target with explicit dependencies.
2. A CI check will read the CMake File API or `target_link_libraries` and
   compare the result with the allowed dependency matrix. A violation fails
   the build. This check is planned for M1.
3. `SceneView` is read-only. `SceneEditor` is mutable and is exposed only to
   command handlers and simulation systems.
4. `render` never receives `SceneEditor`.
5. New module boundaries require review and an ADR.

## Current implementation

M1.2 contains:

- `engine/core`: version metadata, `Result<T>`/`Error`, UUID v4 and the seeded
  `UuidGenerator`, `PathSandbox`, JSON `Config`, `Log` with console and JSONL
  sinks, the reflection registry with draft-07 component schemas, and the
  `Vec3`/`Quat` math types
- `engine/scene`: EnTT-backed ECS with stable UUIDs, `Transform`, `Camera`,
  parent/child links, world-transform composition, cycle rejection,
  recursive subtree destruction, and the project/scene JSON format with
  canonical output, atomic saves and project locks
- `engine/platform`: atomic file replacement with `fsync` on POSIX and
  `MoveFileExW` on Windows
- `engine/command`: versioned command registry, JSON Schema validation via
  json-schema-validator, structured success/error results with warnings, an
  in-memory command log with sequence numbers, undo/redo stacks, redo
  invalidation and transactions that commit or roll back as one undo step
- `engine/scene` also provides a fixed-tick simulation that advances `Motion`
  velocities into `Transform` positions and emits a deterministic state dump
- EnTT 4.0.0 and json-schema-validator 2.4.0 are active
- `tools/engine`: the CLI11-based `engine` CLI with `--version`, `doctor`, and
  volk-based Vulkan enumeration
- `tests/unit`: core unit tests
- `tests/smoke`, `tests/contract`: CLI JSON and schema tests
- `schemas/`: `version@1`, `doctor@1` and `log-record@1` schemas
- CI: GCC 14, Clang 18, Windows MSVC, clang-format/clang-tidy, ASan/UBSan and
  lavapipe-based Vulkan detection

The remaining modules are planned, not implemented. M1 adds reflection, ECS,
the command bus and the project/scene format.
