# AI Agent Guide

**Status:** M3.7 implementation and verification. Offline authoring, asset
cooking/reimport/placement, shared PBR rendering, fixed-tick simulation,
offscreen screenshots and the authenticated local live API are implemented.
Current review and exact-source verification results are in STATUS.md.

## Status reports and independent review

Owner instruction, 2026-10-30: do not send interim or substep status reports
while independent review is pending. Deliver one consolidated A.5 report only
after review is complete, findings have been addressed, and required re-review
and CI verification have completed successfully. During ongoing work, provide
only brief progress updates, necessary questions or genuine blockers. This
instruction applies across sessions, including when a review runs asynchronously.
The end-of-session/substep reporting rules do not require a premature report.

## Independent reviewer

Owner clarification, 2026-10-30: Claude Opus5.5 high is the reviewer until
further owner instruction. Reviewer selection and changes belong to the owner.
The earlier owner message selected GPT-6 Astra high; the latest clarification
supersedes it. Preserve historical reviewer attribution rather than rewriting it.
The three M3.7 review corrections require independent re-review, especially the
queued-deadline correction, before M3.7 can be approved.

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

## Asset CLI (M3.4)

```bash
engine asset import <project> <source.glb> [--name NAME] [--dry-run] --json
engine asset inspect <project> <assetId> --json
engine asset validate <project> <assetId> --json
engine asset reimport <project> <assetId> [--dry-run] --json
engine asset list <project> --json
```

The source argument is relative to the caller's working directory or absolute;
it may intentionally be outside the project. The sidecar retains that original
source for reimport and falls back to its project copy if the source disappears.
Successful CLI data exposes project-relative copy/cache/dependency paths rather
than the original machine-specific source path. Results use `command-result@1`;
asset data validates against `asset-result-data@1.schema.json`. Input schemas
for the asset lifecycle and placement commands are included in `engine schema --json` and the
generated API reference. CLI and future live control use the same handlers.

`inspect` reports stable node/mesh/material/texture IDs, per-primitive bounds,
dependencies, animation-channel count and cooked-cache state. It still works
when the derived cache is missing or stale, returning a warning and
`cache.state = "unavailable"`. `validate` requires a usable current cache and
checks the portable source copy against its stored SHA-256. `reimport` rebuilds
missing/stale cache while preserving stable IDs. `list` is sorted by asset UUID.
Animation data is not imported in M3 and inspect warns when channels are present.

`--dry-run` parses, validates and cooks in memory but writes no files. Generated
IDs in an import preview are disposable and not reservations. Reimport preview
preserves the existing asset ID. Preview validates content and sandbox paths;
it cannot promise that a later filesystem write will succeed. Imports acquire
the normal project lock before writing. Asset imports/reimports are not undoable
filesystem transactions; registering them in a shared bus does not make them
eligible for a scene batch transaction. Such use fails with `E_UNSUPPORTED`
before mutation. Scene placement/undo is implemented in M3.6.

The official MikkTSpace implementation generates missing normal-map tangents
during cooking and splits vertices at tangent discontinuities. Source tangents
are preserved. `gltf-import@4` rejects old cache; use reimport to regenerate it.
The checked-in Blender fixtures verify tangent compatibility on the CPU.
The M3.5 golden compares the mirrored-UV Blender model without tangents with
a fixed lavapipe image and its Blender-tangent export. Renderer C++ APIs expose
explicit `upload_asset`, `retire_asset` and `collect_assets`; scenes reference
the uploaded ID through `AssetInstance`. CLI placement and automatic project
asset resolution are implemented in M3.6. Reimport also rebuilds corrupt derived buffers
when their stored cache key still matches.

## Place and render asset instances (M3.6)

```sh
engine asset import demo models/cube.glb --json
engine asset place demo scenes/main.scene.json --asset <assetId> --name Left --position -1.5 0 0 --json
engine asset place demo scenes/main.scene.json --asset <assetId> --name Right --position 1.5 0 0 --json
engine screenshot demo scenes/main.scene.json --out screenshots/instances.png --width 256 --height 192 --json
engine run demo scenes/main.scene.json --frames 3 --hidden --json
```

A scene needs an enabled camera viewing the model. Placement uses the shared
`asset.place@1` handler and returns `data.result.id` plus assetId/name/position.
Its default name is the imported asset name and default position is [0,0,0].
The CLI treats an explicitly empty --name as omitted; direct asset.place@1 JSON
requires minLength1 if name is supplied. UUID lookup/deduplication uses canonical
identity, including uppercase references supplied through component updates.
`--dry-run` validates against the project and scene without saving; preview
entity IDs are disposable. Placement holds the project lock, supports scene
batch undo/rollback, and stores one AssetInstance rather than expanding GLB nodes.
Two instances share their asset UUID, meshes, materials and image descriptors.
Window and screenshot commands resolve each referenced asset once. Rendering
is read-only: repair a missing/stale cache with `engine asset reimport` explicitly.
Reimport keeps the asset UUID, so saved instance references continue to work.

BaseColor and emissive textures use sRGB; normal/MR/AO are linear. Importer@4
invalidates the former emissive-linear caches. Material factors remain linear.

## Local live API (M3.7)

Start a foreground project session, then use another terminal/client:

```sh
engine live serve demo scenes/main.scene.json --json
engine live call demo --method ping --json
engine live call demo --method command --params '{"name":"asset.list@1"}' --json
engine live call demo --method inspect --params '{"target":"log","offset":0,"limit":32}' --json
engine live call demo --method command --params '{"name":"scene.save@1","arguments":{"path":"scenes/main.scene.json"}}' --json
engine live call demo --method shutdown --json
```

The CLI reads .engine/live.json through PathSandbox; credentials are not command
line arguments or startup output. A Python client reads the private discovery
file, connects only to127.0.0.1:port and sends one UTF-8 JSON object plus LF:
jsonrpc2.0, protocolVersion1, token, id, method and named-object params. The
published wire schemas are jsonrpc-request@1/jsonrpc-response@1. RPC result
contains the same command-result@1 envelope as offline CLI. Authentication
errors contain only code/message and never echoed parameters.

Methods: command(name,arguments), inspect(target=scene|asset|project|schema|log),
screenshot(out optional,width,height,frames), ping and shutdown. Inspect log
supports offset/limit (1..64) and exposes the actual CommandBus records. Scene
save/load and history undo/redo are shared commands, also in CLI schema output.
Live scene changes remain in memory until explicit scene.save@1; shutdown does
not silently save. Loading clears undo actions from the prior scene.

See ADR-0013 for bounds:64KiB requests,4MiB replies, depth32,8clients, absolute
receive/send I/O deadlines (default2s), and command-log admission bounds. Clients
timing out after a complete request must not assume its command was rolled back.
All paths are project-relative and sandboxed; import sources must be inside the
project. An offline-imported external original is not read during live reimport;
the project copy is used instead. Screenshot outputs must be .png and cannot
replace session/cache/scene files. Offline writers, including screenshot and
force-unlock of an active process, conflict with the lifetime live project lock.
After a crash, use project unlock --force only once the owning process is dead.

Live command journals persist under .engine/live-commands-<session UUID>.jsonl.
The startup result identifies commandLogFile. Intent is synchronized before
execution; completed CommandBus records are synchronized before acknowledgement.
Restart preserves old session files, including after a crash. A started record
without completion has an uncertain outcome; do not replay it automatically.
Ignore an incomplete trailing JSONL line when reading earlier complete records.
Journal failure ends the live session with E_IO. This does not save scene state;
use scene.save@1. Journals contain command arguments/results and are owner-only.

A journal failure also ends the foreground server with the existing E_IO exit
status6 and one constant stderr diagnostic, even when a notification has no
reply. Malformed result serialization follows this same terminal path.
