# Vision

**Status:** Complete English translation of Part B from the master prompt
v0.2, delivered in M0.6. B.2 includes the clarification accepted in ADR-0002
on 2026-10-29. The source is the owner-approved Norwegian master prompt; this
file is the English reference for the repository.

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

## B.3 Multiplayer preparation

Networking is not implemented now, but the architecture must not block it
later:

- Separate **simulation** (game state, fixed tick, can run headless) from
  **presentation** (rendering, interpolation, audio, UI).
- The renderer reads game state but never owns it.
- Changes to game state happen through commands and events that can be
  serialized.
- The simulation must be able to run without a window and GPU. That is also
  the basis for dedicated servers.
- Avoid global singletons in simulation state.

Replication, prediction and network code are not built until a separate
milestone is approved.

## B.4 Technology stack

| Area | Choice |
|---|---|
| Language | C++20 |
| Graphics | Vulkan 1.3 as the minimum, for broader driver support than 1.4. Newer features only through optional extensions with capability checks. Dynamic Rendering and Synchronization2. |
| Window and input | SDL3 |
| Vulkan loading and GPU memory | volk and Vulkan Memory Allocator |
| Mathematics | GLM |
| 3D format | glTF 2.0 / GLB via fastgltf |
| Shaders | GLSL compiled to SPIR-V at build time (glslc or glslang from the Vulkan SDK) |
| ECS | EnTT |
| Physics | Jolt Physics, configured for determinism |
| Scripting | Lua 5.4 |
| Editor UI | Dear ImGui |
| Build | CMake with presets, and Ninja |
| Compilers | GCC and Clang on Linux, MSVC on Windows |

Chosen in M0 (the agent proposes, the owner approves):

- dependency management: vcpkg manifest with a baseline, or CPM/FetchContent
  with locked commit hashes
- JSON library, test framework, image loading and logging
- error-handling strategy (note that `std::expected` only arrived in C++23)
- reflection mechanism and Lua binding library

All dependencies are documented in `docs/DEPENDENCIES.md` with version,
license and rationale. The engine is MIT-licensed (Part 0), so only permissive
licenses (MIT, zlib, BSD, Apache-2.0) are allowed without separate approval.
Verify APIs and compatibility before adopting a library.

## B.5 Modules and dependency boundaries

The structure is preliminary and is fixed in M0. Each module is a separate
CMake target with explicit dependencies. Circular dependencies are forbidden.

| Module | Responsibility | May depend on |
|---|---|---|
| core | Lifecycle, logging, time, memory, job system, configuration, filesystem, UUID, reflection | No other engine modules |
| platform | Window, input, OS integration. The only place for OS-specific code | core |
| command | Command bus, schemas, validation, transactions, undo/redo, command log | core |
| scene | ECS, transform hierarchy, serialization, prefabs | core, command |
| asset | Import, cooking, cache, dependency tracking, reimport | core, command, scene |
| physics | Jolt integration, collision, raycast, character controller | core, scene |
| script | Lua runtime, sandbox, bindings, events | core, command, scene, physics |
| world | Chunks, streaming, terrain, persistent edits | core, command, scene, asset, physics |
| render | Vulkan renderer that reads scene and world data | core, platform, scene, asset, world |
| control | CLI, live API (JSON-RPC), later MCP adapter | core, command and the modules it exposes |
| editor | ImGui editor that changes state only through commands | All of the above |
| game modules | Game-specific C++ and Lua (B.6) | The engine's public API |

The simulation modules (scene, physics, script, world) must never depend on
render. Modules are implemented gradually, not all at once.

## B.6 Game logic and extensibility

The engine has three layers:

1. **Engine core:** the modules in B.5.
2. **Reusable game systems:** for example inventory, interaction, building,
   crafting, quests, vehicles, NPCs and progression. Built only once the
   foundation is stable.
3. **Game-specific logic and content:** per game project.

Game-specific logic can live in two places. Neither requires changes to the
engine core:

- **Lua scripts** are the default for game logic.
- **C++ game modules** are separate CMake targets per game project. They
  register their own components and systems through the reflection system and
  the engine's public API. Such components automatically get serialization,
  the inspector, schemas and Lua bindings.

Whether game modules are linked statically or loaded dynamically is decided in
an ADR when the need arises.

Lua requirements:

- Lua 5.4 with a sandbox: no `io`, `os`, `debug` or free file loading by
  default.
- Limits on memory and instructions per script.
- Errors reported with file, line and stack trace.
- Bindings generated from the reflection system and documented automatically.
- Hot reload of scripts in development builds.

The goal is that AI agents can compose games from existing components,
prefabs, scripts and assets.

## B.7 Large open worlds

The engine must support very large game areas, with the target size from
Part 0.

- Global positions are stored in double precision. The renderer uses
  camera-relative float coordinates.
- The world is divided into chunks that are loaded and unloaded
  asynchronously based on the player's position.
- LOD for terrain and objects.
- The memory budgets from Part 0 are respected.
- Procedurally generated base terrain is combined with manually placed
  Blender models.
- Persistent changes are stored.

The original world definition is kept separate from the changes the player
makes. Example: an area is generated, the player builds a structure and
removes some objects. When the area is unloaded and loaded again, the changes
are preserved.

Simulation and storage of the world must not assume a single player (B.3).

## B.8 Blender and the asset pipeline

Blender is the primary tool for 3D models, and GLB/glTF 2.0 is the primary
import format.

The flow is `.blend` (editable source, kept) → exported GLB → import → cooked
runtime format in the cache, which can always be regenerated.

Support over time: meshes, materials, textures, PBR, multiple objects per
file, hierarchies, animations, asset references and reimport. Reimport must
preserve stable IDs and references.

An AI agent must be able to import a model, read the import result as JSON
(meshes, materials, bounding box, warnings) and place the model in a scene
without the editor.

File-format requirements:

- Project and scene files are text (JSON), deterministically sorted, and
  suitable for diff and merge in Git.
- Binary cooked formats are kept separate and can be regenerated.
- All formats are versioned and have migrations between versions.

## B.9 Hot reload

In development builds, shaders, Lua scripts and assets must be reloadable
without a restart. Fast iteration is a prerequisite for an effective AI loop.

## B.10 Documentation

Documentation is part of the product. These files must exist:

| File | Content |
|---|---|
| `docs/VISION.md` | Part B of this prompt, translated to English |
| `docs/STATUS.md` | Current status, updated every session |
| `docs/decisions/` | ADRs, one file per decision |
| `docs/ARCHITECTURE.md` | Modules, dependencies and data flow |
| `docs/BUILD.md` | Environment, dependencies and build commands |
| `docs/TESTING.md` | Test types and how to run them |
| `docs/DEPENDENCIES.md` | Versions, licenses and rationales |
| `docs/PROJECT_FORMAT.md` | Project and scene formats, with examples |
| `docs/AI_AGENT_GUIDE.md` | How an agent uses the CLI and live API |
| `docs/API_REFERENCE.md` | Generated from reflection and the command registry |
| `docs/ROADMAP.md` | Milestones and status |

Rules:

- Document only what exists. Clearly mark what is planned, implemented and
  verified.
- API reference and schemas are generated. They are not written by hand.
- Code examples in the documentation are run as tests in CI, so they do not
  become stale.
- An agent must be able to find out which operations exist, which parameters
  they require, which errors can occur, and how the result is verified.

## B.11 Testing

| Test type | What it ensures |
|---|---|
| Unit tests per module | Isolated logic |
| Integration tests | Interaction between modules |
| Headless simulation tests | Run N ticks and compare the state dump with a reference |
| CLI contract tests | JSON responses validate against published schemas |
| Round-trip tests | Save, load and save again produces byte-identical files |
| Golden image tests | Offscreen rendering is compared with reference images produced with lavapipe in CI. Local GPU runs are compared with a tolerance |
| Vulkan validation | The debug messenger counts validation errors. One error fails the test |
| Sanitizer builds | ASan, UBSan and TSan on Linux |

Every milestone must:

1. be configurable, buildable and testable from a clean clone on Linux, with
   documented commands
2. have relevant tests that have been run and passed
3. have updated documentation
4. not break previously working functionality
5. have green builds and headless tests in CI on Windows, and report Windows
   graphics as verified or not verified

## B.12 Milestones

The milestones build on each other, and each is divided into substeps in the
milestone plan (A.4). Numbers in the acceptance criteria come from Part 0.

### M0 — Project foundation

- Directory structure, CMake with presets, Ninja and dependency management
- Warning level, clang-format, clang-tidy and sanitizer presets
- Core: logging (structured and readable), configuration, filesystem
  restricted to the project directory, UUID and error handling
- Test infrastructure with the first tests
- CLI skeleton: `engine --version --json` and `engine doctor --json`, which
  reports compilers, Vulkan and GPU
- Documentation structure, `STATUS.md` and the first ADRs, and `VISION.md`
  translated to English from Part B
- CI in GitHub Actions for Linux (Ubuntu) and Windows, and a LICENSE file with
  the MIT license

**Approval:** A clean clone can be configured, built and tested on Linux with
the commands in `BUILD.md`. `engine doctor --json` produces valid JSON that
validates against its schema. Builds and tests are green in CI on both Linux
and Windows.

### M1 — Headless core: scene, command layer and project format

- Reflection system
- EnTT integration, transform hierarchy and stable UUIDs
- Command bus with schemas, validation, undo/redo, transactions and command
  log
- Versioned text formats for project and scene
- Offline CLI: create project, create and inspect scenes, create, delete and
  inspect entities, and read and set components. Everything goes through the
  command bus, with `--json` and `--dry-run`.
- Simulation loop with a fixed tick, and the command "run N ticks and dump
  state"
- Generated JSON schemas and the first version of `API_REFERENCE.md`

**Approval:** A script that uses only the CLI can create a project, build a
scene with a hierarchy of entities, change components, undo, save and load
again. Saving again produces byte-identical files. Two runs of the same scene
for N ticks produce an identical state dump. All CLI responses validate
against their schemas.

### M2 — Vulkan renderer

Substeps:

1. Instance, device and validation via volk
2. Swapchain and clearing the screen
3. First triangle
4. Depth buffer, mesh and camera-relative camera
5. Offscreen rendering and screenshot
6. Window resize, minimization and orderly cleanup
7. ImGui overlay for debugging: FPS, frame time and entity list

Other content: SDL3 window and input, VMA, Synchronization2, Dynamic
Rendering, frames in flight and a free camera. Descriptor strategy (for
example bindless) is decided in an ADR. The renderer draws scenes from M1 and
only reads scene state.

**Approval:** `engine screenshot <scene> --frames N --out <file>` works
without a window. A golden image test passes. The debug messenger counts zero
validation errors in the documented scenarios, including window resize and
minimization.

### M3 — Asset pipeline and live control

- GLB import via fastgltf: meshes, textures, basic PBR materials and
  hierarchies
- Cooked format, cache and dependency tracking. Reimport preserves IDs.
- CLI: import, inspect and validate assets, with a JSON result
- Commands for placing assets in scenes
- Live mode: JSON-RPC over a local connection with the same commands, plus
  inspection and screenshot

**Approval:** A script can, without the editor, import a model exported from
Blender, inspect it, place it in a scene, render, save and load the scene
again. After the model is changed and reimported, no references are broken. A
golden image test of the scene passes.

### M4 — Physics and game logic

- Jolt Physics with deterministic configuration: rigid bodies, collision and
  raycast
- Character controller and first-person controller
- Lua 5.4 with sandbox, generated bindings, lifecycle, events and hot reload
- Structure for C++ game modules, with an example module that registers its
  own component
- Event system
- Input recording and playback

**Approval:** A player can move, collide with the world and interact with
objects through Lua logic. The example module adds a component without changes
to the engine core. A recorded input sequence played back headless produces an
identical final state.

### M5 — Open world

- Terrain system and chunk structure
- Asynchronous streaming and terrain LOD
- Double precision in world coordinates
- Storage of persistent changes
- Memory budgets and streaming tests

**Approval:** A scripted camera or player crosses the entire target size from
Part 0. Golden image tests far from the origin show no precision errors.
Memory usage stays within budget. Changes survive unload and reload, verified
by an automated test. Frame time during streaming meets the performance target
on the reference hardware.

### M6 — World editor

- Dear ImGui interface, scene hierarchy and object inspector generated from
  reflection
- Transform gizmo, asset browser and object placement
- Saving scenes and projects

**Approval:** A user can build a simple world visually. Every editor operation
appears as a command in the command log and can be repeated through the CLI.

### M7 — AI-assisted game development

- Automated project creation and reusable project templates
- Scene automation and extended CLI
- MCP adapter over the command layer
- Automatic screenshots, reports and test tools
- Examples of AI-generated game content

**Approval:** An AI agent creates a small, playable prototype using only
documented interfaces (CLI, live API or MCP), with minimal manual intervention.
The session is logged and can be replayed.

### M8 — Optimization and distribution

- CPU/GPU profiling and resource budgets
- Performance improvements based on measurements
- Stabilization and reproducible stress tests
- Building and packaging games for Linux and Windows

**Approval:** A standalone game prototype can be built and distributed for
both platforms. Performance is measured and documented against the targets in
Part 0, on the reference hardware and under the measurement conditions stated
there.

## B.13 Future features

The architecture must accommodate these, but they are not built until the core
systems work:

- multiplayer and dedicated servers (prepared for, see B.3)
- NPC AI and vehicles
- advanced terrain editing and vegetation systems
- weather, day/night cycle and advanced lighting
- surface states such as dirt, dust, paint and wear
- modding and a plugin system
- more platforms
- automated game generation with multiple cooperating agents
