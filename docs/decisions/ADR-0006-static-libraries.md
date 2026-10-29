# ADR-0006: Static libraries and module boundaries for M0–M4

- **Status:** Accepted
- **Date:** 2026-10-29

## Context

The engine is split into modules with explicit dependencies. Building shared
libraries on Windows requires export macros, symbol visibility rules and DLL
packaging. M0–M4 do not need dynamic loading, and fast link times and simple
tests are more valuable than shared-library deployment.

## Decision

- All engine modules are static libraries in M0–M4.
- Each module is a separate CMake target with explicit `PUBLIC` and `PRIVATE`
  dependencies.
- `command` contains only the bus, schemas, transactions and log. It does not
  know scene types; `scene`, `asset` and `world` register their handlers.
- `render` reads state and never mutates it. It never receives `SceneEditor`.
- `control` and `editor` are composition roots. Game projects use
  `find_package(engine CONFIG)` and only the public API.
- A CI dependency check will compare target links with the allowed matrix in
  M1. A violation fails the build.
- Shared libraries and dynamically loaded game modules are decided in a later
  ADR when there is a concrete need.

## Consequences

- No Windows DLL export macros in the first milestones.
- Simpler debugging and test binaries.
- A game executable links the modules it uses; dead code elimination keeps
  binaries reasonable.
- Dynamic game modules and hot reload of C++ code are deferred.

## Alternatives considered

- **Shared libraries from M0:** more deployment flexibility, but extra
  complexity before it is needed.
- **One monolithic target:** simplest build, but hides dependency violations
  and slows incremental builds.
- **Header-only modules:** not practical for the renderer, physics or ECS.
