# Architecture

**Status:** Draft skeleton. The full architecture document is completed in M0.6.

## Module boundaries

The module structure follows the vision. Each module is a separate CMake
target with explicit dependencies; cycles are forbidden.

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

`render` is added to `world` only when `world` exists; M2 links `core`,
`platform`, `scene` and `asset`.

## Dependency rules

- Simulation modules (`scene`, `physics`, `script`, `world`) never depend on
  `render`.
- The renderer reads state and never mutates it.
- Authoritative state changes go through the command bus. Per-tick simulation
  state is mutated by deterministic systems. See ADR-0002.
- `command` does not know scene types. Scene modules register their handlers.
- `control` and `editor` are composition roots; game projects depend only on
  the public engine API.
- Static libraries are used in M0–M4.

## Current implementation

M0.1 contains only:

- `engine/core`: version metadata
- `tools/engine`: the `engine` CLI with `--version`
- `tests/smoke`: the version JSON smoke test

The remaining modules are planned, not implemented.
