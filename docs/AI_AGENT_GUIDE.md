# AI Agent Guide

**Status:** M2.5. Version, doctor, the offline project/scene/entity/component
CLI, fixed-tick simulation, the Vulkan renderer and offscreen screenshots exist. The live API is
planned for M3.

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

## Dry run

State-changing offline commands accept `--dry-run`:

```bash
engine entity create ./my-game --name Preview --dry-run --json
engine component set ./my-game --id <uuid> --component engine.scene.Motion \
  --value '{"linearVelocity":[1,0,0]}' --dry-run --json
engine batch ./my-game --file changes.json --dry-run --json
```

A dry run returns the same result shape with `"dryRun": true` and writes no
project or scene files. Entity and component commands and batch execute in
memory; `project create` and `project unlock` only validate the target and
report what would happen. `scene create` validates the path and duplicate
registration. Read-only commands do not need it.

## Renderer

The render module is `engine::render`. `engine screenshot` and `engine run`
are the windowed/offscreen CLI entry points. The current API creates a Vulkan 1.3
device with Synchronization2, Dynamic Rendering and VMA, either headless or
from an SDL3 window. `render::Renderer` owns the swapchain and frame loop; it
clears, draws scene meshes and presents, and recreates the swapchain after
resize, minimize or out-of-date events. Scene world transforms are composed in
double precision and converted to camera-relative floats before rendering.
`SceneView` is the read-only boundary into the renderer. GLSL shaders are
compiled by `glslc` at build time; the shaderc library is not linked.
`engine.scene.Mesh` stores a `meshId` (`engine.builtin.cube`,
`engine.builtin.plane` or a future asset UUID), never mesh data. The windowed
renderer owns a Dear ImGui debug overlay that shows FPS, CPU/GPU frame time and
the entity list; it is enabled by default in debug builds and reads only
`SceneView`. See ADR-0010 for the ImGui integration. Device creation requires
descriptor indexing, update-after-bind, partially bound descriptors, variable
descriptor counts, runtime descriptor arrays and the descriptor limits in
ADR-0011. There is no fallback; `Device::create` returns `E_UNSUPPORTED` with
stage `descriptor-indexing` and machine-readable missing lists. `engine doctor
--require vulkan` applies the same gate and exits 8 when it is not met.

## Windowed run

Run a scene in a window with a fly camera:

```bash
engine run ./my-game scenes/main.scene.json
```

Controls: WASD moves, Q/E moves down/up, Shift moves faster, hold the right
mouse button to look, F1 toggles the debug overlay, and Escape closes the
window. `--frames N --hidden` runs a bounded hidden-window smoke test.

Input is presentation-only. The fly camera is passed to the renderer as a
`CameraPose`; it never mutates the scene or authoritative state (ADR-0002).

## Screenshots

Render a scene without a window:

```bash
engine screenshot ./my-game scenes/main.scene.json \
  --frames 1 --width 1280 --height 720 --out shot.png --json
```

The command creates a headless Vulkan device, renders the scene with the built-in
meshes, writes a PNG, and returns a `command-result@1` document. `--out` is
resolved inside the project sandbox; width and height are capped at 8192 and
frames at 1000. `--json` is supported. The screenshot test uses `tests/fixtures/golden/m2_scene_64.png` on
lavapipe. `render::Device::create` returns `E_UNSUPPORTED` when the
loader, Vulkan 1.3, Synchronization2, Dynamic Rendering or the validation
layers are missing; the error details name the failing stage.

Run the renderer tests with:

```bash
ctest --preset ci-linux-clang -R "engine_render_tests|engine_window_smoke|engine_swapchain_test" -V
```

`engine_window_smoke` and `engine_swapchain_test` report exit code 77 when no
display server is available. The renderer never mutates scene state; M2.4 adds
the read-only `SceneView`.

## Planned commands

- Live API (JSON-RPC 2.0 over a local connection) in M3.
- MCP adapter in M7.

## Live API and MCP

Not implemented. The live API will expose the same commands plus inspection,
screenshots, statistics and "run N ticks". The MCP adapter will be a thin layer
over the same command bus.
