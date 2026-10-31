# ADR-0018: Static game modules and an installed game SDK

- **Status:** Accepted by the owner on 2026-10-31; implementation not yet complete
- **Date:** 2026-10-31
- **Scope:** [M4 proposal](../M4_PLAN.md); specializes the static-library direction
  already accepted in [ADR-0006](ADR-0006-static-libraries.md).

## Context

Game-specific C++ code must register components and systems through public
engine APIs without editing engine sources. Current component load/save paths
still contain built-in type switches, CLI/live orchestration lives in the engine
tool, and installed CMake exports do not yet exist. These are implementation
tasks, not capabilities already supplied by linking an archive.

## Decision

Game modules are **static CMake libraries** linked into explicit game hosts.
Lua is the default gameplay extension mechanism; C++ is available for custom
systems and reflected components. Keep all engine modules static for M4.
No platform DLL loader, stable binary plugin ABI or C++ hot reload is added.

1. Publish a small registration context in existing public engine modules for
   component field metadata, scene ECS storage adapters, ordered systems,
   commands and events. Registration happens before loading scenes or producing
   schemas. Reject duplicate names/IDs and invalid dependency/phase orders.
2. Each module exports an ordinary explicit registration function. A game
   launcher's generated/declared module list calls these functions in a stable
   order. Do not rely on static constructors, archive traversal or linker
   `--whole-archive` to make registration happen by accident.
3. Extend generic scene component factories/serialization/undo/schema paths
   so the same registered component works from load/save, CLI/live and Lua.
   Reject missing modules/unknown types with useful IDs; never drop their data.
   One component registration is the field-level source of truth.
4. Extract reusable host/CLI/live orchestration from `tools/engine` into the
   approved control composition boundary. Window/render operations are injected
   services; the headless game host uses the same command handlers without
   initializing a window or Vulkan. No second implementation of M3 commands.
5. Export an installable `engineConfig.cmake` and named public targets with
   transitive dependency discovery. An external CMake consumer uses
   `find_package(engine CONFIG REQUIRED)` and links its game module/host against
   the installed SDK. Build-tree access to private engine headers is forbidden.
6. All archives/hosts use the same supported compiler/CRT/profile. Module ID,
   version and content/build fingerprint form part of game profile and replay.
   This is source-level extensibility, not cross-compiler binary compatibility.

The example module registers `game.demo.Interactable` and a small system. Its
fields automatically participate in save/load, generated schema/docs and Lua
access. Test it from a separate source tree after installing the SDK. Merely
building a sample under `add_subdirectory` inside this repo is insufficient.

Proposed project2 lists stable required module IDs and the game profile, not
arbitrary dynamic library paths. Use a game-specific executable containing the
modules to open its scenes. The generic engine executable cannot load a static
module that was never linked; it reports a missing profile/module clearly.
Existing core-only/M3 projects continue to work with the generic executable.

## Consequences and alternatives

- Simple platform deployment and ordinary debugger behavior, consistent with
  ADR-0006. New C++ gameplay requires rebuilding/relinking its host.
- Explicit referenced registration symbols survive normal dead stripping;
  initialization order is testable rather than dependent on linker behavior.
- Generic storage adapters and package exports are real M4 work and must be
  tested before claiming the sample proves extensibility.
- A header-only module or example that edits engine type switches would not
  meet the user's game-module requirement.
- Dynamic plugins could reduce relink time, but require ABI/versioning,
  lifetime/unload and Windows export decisions. Defer until there is a concrete
  need and a separately approved ADR. Lua hot reload remains in M4 scope.

## Owner-approved M4.8 stop condition

The installed SDK remains the target. If dependency install/export proves
extensive in M4.8, stop and present a concrete proposal to move full packaging
to M8 while using a separate external CMake/public-target boundary test in M4.
Do not silently weaken the external-consumer test or expand packaging scope.
This stop condition was explicitly added by the owner on 2026-10-31.

## Acceptance

On Linux GCC/Clang and Windows MSVC, install the SDK, build the external sample,
register its module and load/save an example scene. Generated schemas and Lua
expose the new field without engine edits; entity deletion/undo restores it.
The sample performs deterministic headless replay. Missing/duplicate/wrong-
version module profiles fail clearly before simulation. Verify public include
paths/exported targets and retain dependency-direction checks.
