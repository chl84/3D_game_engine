# M4 Plan — Physics and game logic

**Status:** Owner-approved on 2026-10-31 with the conditions below. M4.1 is
authorized; VM topology and concrete schemas require further owner decisions.

## Owner approval conditions

- ADR-0017 (Jolt) and ADR-0018 (static modules) are accepted.
- sol2 3.3.0 and C++ Lua 5.4.9 are conditionally approved: M4.1 admission probes
  must be green under GCC14, Clang18 and MSVC with warnings-as-errors for our code.
- ADR-0016 is not accepted yet. Compare **1,000 instances** using one VM per
  instance versus one VM per session with separate sandbox environments. Measure
  memory/startup time, provide actual numbers and a recommendation, then await
  the owner's VM decision. Do not adopt per-instance VMs as a default meanwhile.
- The resource budgets and 60Hz rate are approved starting points. An admission
  benchmark may measure a topology beyond the aggregate runtime budget only if
  it is explicitly labelled diagnostic and reports that budget incompatibility.
- Project2/scene4/replay direction is approved, but concrete schemas must be
  approved in M4.1 before format-writing implementation.
- Skeletal-animation deferral is approved; track a separate animation plan in
  ROADMAP.md with proposed placement.
- Retain the installed SDK goal for M4.8. If dependency install/export becomes
  extensive, stop and propose full packaging in M8 with an external CMake/public-
  target boundary test in M4. Do not silently substitute or expand packaging work.

**Baseline:** M3 merged through [PR5](https://github.com/chl84/3D_game_engine/pull/5)
at `88ccc85d30f03a8d2e6a8b345b1a2216a2d3d818`, released as annotated tag
`v0.4.0`. All six final CI jobs passed on release head `031bc43` before merge.
The owner reported the manual Blender test passed and authorized merge/tag.

## Goal and visible result

Deliver a small playable first-person sample: a capsule player walks and jumps
against static geometry, pushes dynamic objects and activates an object through
a Lua interaction script. An external C++ game module adds an `Interactable`
component without modifying the engine source. Record an input sequence and
replay it in a fresh headless process to obtain identical per-tick state hashes.

The renderer remains read-only. Authoring commands and runtime simulation retain
the split in [ADR-0002](decisions/ADR-0002-authoritative-state-vs-simulation.md).
Existing `engine run` remains the M2/M3 inspection fly camera. Add a distinct
`engine play` command for gameplay, so the owner's current workflows keep working.

## Decisions proposed for approval

1. [ADR-0016](decisions/ADR-0016-lua-bindings.md): **sol2 3.3.0 with Lua 5.4.9**,
   Lua built as C++ for unwinding, protected calls and safety checks; the VM
   topology remains undecided until the 1,000-instance comparison. No LuaJIT
   or Lua 5.5 migration. All packages remain private implementation details.
2. [ADR-0017](decisions/ADR-0017-deterministic-jolt.md): **Jolt 5.6.0**, CPU-only,
   double positions, cross-platform deterministic profile and a single-threaded
   job system in M4. Exact compiler/profile fingerprints are recorded in replay.
3. [ADR-0018](decisions/ADR-0018-game-module-linking.md): **static C++ game
   modules and explicit registration**, plus an installed CMake SDK and an
   independently built example. No DLL/plugin loader or C++ hot reload in M4.
4. Proposed **project format 2 / scene format 4**, with v1-project/v1–v3-scene
   migrations that preserve UUIDs and existing content. Add versioned replay and
   game-profile schemas. Exact schemas and a format ADR must be presented and
   accepted in M4.1 before format-writing code is implemented.
5. Default play/replay rate **60 fixed ticks per second**, per-instance Lua
   quota **8 MiB**, **64 MiB aggregate Lua allocation**, **100,000 instructions
   per callback**, **1,000,000 per tick**, **4,096 queued events/commands per
   tick**. Limits are checked and fingerprinted, not silently relaxed by tests.
6. Explicit scope decision: defer the skeletal-animation playback follow-up
   tentatively assigned to M4 in the M3 documents to a **separate animation plan**
   before it is implemented. This proposal covers the M4 physics/gameplay vision;
   animation import/skinning would require asset/render and cooked-format work.
   This deferral requires owner approval. If declined, expand this plan before
   implementation rather than quietly dropping the earlier follow-up.

The versions above are now pinned in admission-only overlays and `vcpkg.json`;
engine gameplay adoption remains conditional on the M4.1 gates. M4.1 records exact
commit/archive hashes, licenses and any transitive packages/overlay changes.
Keep the existing vcpkg baseline; use pinned overlays if necessary rather than
upgrading unrelated M3 packages. Lua's C++ build may require a local overlay.
Stop for a concrete owner decision if a proposed pin/profile is incompatible
with the approved GCC14/Clang18/MSVC matrix; do not silently change libraries.

## Existing foundation and work that is actually missing

- M1 supplies reflection, ECS, commands, transactions, seeded IDs and a basic
  fixed-tick `Motion` integrator. That integrator is not a physics/gameplay host.
- M3 supplies stable imported assets, placement, PBR, CLI/live authoring and
  durable command history. Its `run` fly camera is not a character controller.
- Component serialization/dispatch still names built-in component types in
  `scene/project.cpp`. Reflecting a C++ struct alone does not yet make arbitrary
  game components saveable or editable. M4.2 must remove that restriction using
  registered storage adapters while retaining strict unknown-type errors.
- `engine/control` currently contains JSON-RPC parsing; CLI/live orchestration
  is largely in `tools/engine`. Embedding must extract reusable handlers/host
  orchestration, not create a second implementation for custom game launchers.
- An installed `find_package(engine CONFIG)` SDK is an accepted architectural
  direction, but install/export support is not yet implemented. M4.8 delivers it.

## Boundaries and runtime contract

Implement the already-planned `physics` and `script` modules. Keep `physics`
dependent only on `core`/`scene`, and `script` on `core`/`command`/`scene`/`physics`.
Neither may depend on `render` or SDL input types. The control/launcher layer
composes services. Render and window services are optional for headless play;
the headless acceptance process must initialize neither a window nor Vulkan.
No new engine job-system module is introduced before M5.

Use independent runtime scene state for play, initialized through the same
registered component serialization contracts as authoring. Running/replaying
does not save authoring files. Explicit authoring save/commands retain existing
M3 semantics. Physics body IDs, pointers and VM internals never enter scene JSON.

Register components such as `engine.physics.RigidBody`, `BoxCollider`,
`SphereCollider`, `CapsuleCollider`, `CharacterController` and
`engine.script.Script` through reflection/storage adapters. Game types use a
separate stable namespace. Reject a dynamic body with a parent, conflicting
Motion/rigid-body transform writers, invalid shapes or unsupported scale with a
field-specific error. M4 supports primitive colliders attached to visual assets;
GLB triangle-mesh collision cooking is deferred, preserving `physics`'s boundary.

Proposed deterministic tick order:

1. Apply the validated tick's input frame and previously queued structural
   commands in recorded sequence, with deterministic ID generation.
2. Run ordered pre-physics C++/Lua systems to issue runtime intentions/forces.
3. Advance character/Jolt simulation by the fixed dt; never a render-frame dt.
4. Copy runtime transforms, collect and sort physics/game events, then deliver
   them to systems in explicit phase/order/name and entity-UUID order.
5. Queue structural effects for the next boundary; emit canonical state/hash.
   Rendering can interpolate read-only views after a completed tick.

Contact callbacks never execute Lua or mutate the world during Jolt's update.
Raycast ties and event ordering use stable UUID/subshape keys, not allocation
addresses or callback arrival order. Structural gameplay changes use CommandBus;
continuous runtime state uses the explicit simulation API, not 60Hz authoring
commands. Record both input and structural-command outcomes for replay auditing.

A script/resource error is fail-stop for the play session, with stable error
code, script/line/tick and stack details. Report the last completed tick and
failure phase; do not claim successful state for a partial tick. This does not
promise per-tick VM/Jolt rollback. Recover by starting a fresh session from the
initial scene and validated replay, not by resuming a damaged VM.

## Substeps and exit criteria

Each substep ends with a green relevant build/test selection, a commit and the
required independent review. Provide brief progress during review; deliver the
consolidated report after corrections/re-review and final CI. Reviewer remains
**GPT-6 Astra high** unless the owner explicitly changes it.

| Substep | Depends on | Deliverable and objective exit |
|---|---|---|
| M4.1 Decisions and dependency/profile admission | Plan approval | Accept the three proposed ADRs and the concrete format ADR; pin proposed packages/hashes/licenses; build minimal Jolt and protected Lua/C++ unwinding/OOM probes on GCC14, Clang18 and MSVC. Verify flags from generated compile commands. No dependency/format is adopted before approval. |
| M4.2 Generic components and shared host | M4.1 | Register ECS storage/serialization/schema adapters; implement approved project2/scene4 migration; extract reusable CLI/live host logic. An external test component round-trips, is editable/undoable and appears in schemas without a built-in switch case. Existing M0–M3 tests remain required. |
| M4.3 Session, input and replay spine | M4.2 | Fixed tick, ordered systems/events, seeded IDs/RNG, bounded `InputFrame` and versioned replay. Fresh headless processes repeat every tick hash. Existing `simulate` behavior remains compatible. Reject tampered headers, bad tick ordering and mismatched fingerprints before stepping. |
| M4.4 Rigid bodies and deterministic queries | M4.3 | Primitive static/dynamic/kinematic bodies, explicit lifetimes, gravity, contact events and narrow-phase raycasts. Falling/stacking/pushing/query fixtures and insertion-order controls pass; physics hashes match across the pinned compiler matrix. No Jolt type leaks through public headers. |
| M4.5 Character and first-person input | M4.4 | Capsule character, ground/jump/gravity, walls, stairs/slopes, moving-platform policy and interaction ray. SDL actions feed the same tick frames as playback. Focus loss releases inputs; cursor capture is restored on exit. Recorded walk/jump/push remains identical headless. |
| M4.6 Lua host and sandbox | M4.3 | Owner-selected VM topology after M4.1 comparison, allocator/instruction quotas, protected lifecycle, structured errors and text-only sandboxed loading. Test infinite loops, OOM, bytecode, path/symlink escape, absent io/os/debug/package and stale entity handles. C++ destructors run on Lua errors; no panic/exception crosses module boundaries. |
| M4.7 Generated bindings and gameplay | M4.4–M4.6 | Bind reflected fields/commands, explicit runtime operations, events and seeded RNG. A Lua interaction changes an example object's state. Tests prove authoring changes go through the bus, runtime mutation stays in play state and script state is explicit serializable data. Generate API docs from the same registry. |
| M4.8 Static game modules and installed SDK | M4.2/M4.7 | Build/install package exports; explicit registration and module/profile identity. A separate sample CMake project uses only the installed SDK, adds its own component/system, saves/loads it and exposes the same CLI/live interfaces in windowed and headless game hosts. No engine-source edits, DLL loading or static-initializer registration. |
| M4.9 Development Lua hot reload | M4.7/M4.8 | Explicit reload at a tick boundary; build candidate VM and migrate declared state before swap. Syntax/type/quota failure leaves old state active. Existing-world commands are disabled during preparation. Reload is disabled during deterministic recording/playback; a reload starts a new recording segment. |
| M4.10 Complete acceptance and release preparation | All above | Playable original sample, public CLI/live/headless acceptance, input/replay mutations, migration/installed-SDK tests, all reference CI, sanitizers, parser fuzz, clean clone, documentation and independent Astra review. Owner manual play test and explicit merge/tag approval follow technical acceptance. |

## Replay and file compatibility proposal

Project2 records the stable game-profile ID, module IDs/versions, script roots
and runtime settings; it does not name arbitrary DLLs. Scene4 carries the new
registered component data and script initial state. Existing M3 assets and
cooked1 data remain unchanged; M3 projects/scenes migrate in memory and only
write newer formats on explicit save. Old engines must fail clearly on newer
formats. A host missing a required module rejects load with the module/type
identified; it never drops unknown game components.

`replay@1` is bounded JSONL: a header with initial scene/asset/script/profile
hashes, fixed rate, RNG/UUID seeds, engine/Jolt/Lua/module versions and numeric
profile; followed by consecutive input/tick records and canonical hashes.
Proposed limits: 64KiB per record and 64MiB per replay. Load through PathSandbox
and validate size/depth/counts before allocations. Initial-state/script hashes
must match, so source edits cannot silently replay different code. No opaque
Lua heap or Jolt binary blob is a portable engine persistence format.

Canonical state includes UUID-sorted transforms, body velocities/activation,
character state, declared game/script fields, pending ordered events/commands,
and RNG state. Hash normalized explicit fields, never memory dumps, pointers,
padding or unordered container traversal. Same-build replay is bit-exact every
tick. Cross-compiler bit-exact physics fixtures are a separate mandatory Jolt
profile check; arbitrary Lua transcendental/libc behavior is not covered by
that claim. The sample uses approved deterministic operations and engine RNG.

## Final acceptance and evidence

1. Original sample demonstrates walking, collision, jump, pushing and Lua-driven
   interaction. CPU-authoritative play works with rendering entirely absent.
2. Record at least 3,600 input ticks; replay in two fresh headless processes and
   compare every tick, not merely approximate positions at the end. Changing a
   recorded input, seed or state field must be detected by the comparison.
3. Physics-only canonical traces match GCC14/Clang18/MSVC under ADR-0017's
   profile. Separate ordinary and sanitizer results; a sanitizer report is not
   silently substituted for the reference numeric profile.
4. Invalid scripts, quota overruns, broken module identity, corrupt replay and
   unsupported colliders fail in bounded time with structured errors; authoring
   files remain unchanged. Replay after a rejected operation remains valid.
5. The out-of-tree game module adds a component that survives save/load,
   reflection/schema generation and Lua access without editing the engine.
6. Valid reload preserves declared state; invalid reload keeps the prior VM;
   record/playback rejects reload. Stale Lua handles never expose freed objects.
7. Required GCC/Clang, ASan/UBSan, Windows and Quality jobs stay green; extend
   TSan's explicit CPU scope where meaningful without shrinking existing tests.
   Add bounded replay/game-profile parser fuzz targets. Keep required short
   fuzzing; long fuzz is diagnostic/optional, not a newly invented release gate.
8. Clean-clone acceptance includes an installed-SDK consumer and headless replay.
   Report CTest skips and internal device skips separately. Linux window play
   is verified; Windows offscreen/headless is required, physical Windows
   presentation is reported separately when the owner can test it.
9. Record CPU/memory costs of the sample and Lua VM choices on the reference
   machine with AC/profile/build conditions. This is not M5/M8 world-budget
   acceptance. No performance target is waived without owner decision.

## Risks and stop conditions

| Risk | Mitigation / decision boundary |
|---|---|
| Lua/C++ unwind or pinned-library incompatibility | M4.1 probes across all three compilers before game APIs depend on it; no silent binding-library fallback or relaxed warnings. |
| Determinism defeated by call/query/event order | Fixed profile and stable ordering; per-tick hashes and mutation controls; do not rename a tolerance comparison as bit-exact success. |
| Runtime component support is larger than field reflection | Deliver/test storage adapters and installed external consumer before claiming extensible game modules. |
| Replay or reload exposes hidden VM/global state | Explicit declared state, independent instances, stable RNG, source fingerprints and atomic candidate-VM swap. |
| Unsupported scale/hierarchy or collision cooking expands scope | Validate/reject with guidance; primitive colliders only in M4. New mesh cooking needs its own proposal. |
| Single-threaded baseline or per-instance VMs are too expensive | Measure first. Job-system integration or a different VM isolation model requires profiling evidence and owner approval. |
| Earlier skeletal-animation follow-up conflicts with this scope | Resolve the explicit deferral decision before approving/starting M4. Do not silently discard the commitment. |

No editor, terrain/streaming, networking, audio system, skeletal playback,
ragdolls/vehicles, GPU physics or C++ dynamic loading is implemented under this
proposal. Broader systems retain their roadmap positions, subject to the
explicit animation scope decision above.

## Remaining M4.1 approval checkpoints

The owner approved the plan with the conditions at the top. M4.1 now covers
admission/measurements and the concrete schemas proposed in
[ADR-0019](decisions/ADR-0019-m4-project-scene-replay-formats.md).
Topology acceptance and concrete schema approval remain separate checkpoints.
