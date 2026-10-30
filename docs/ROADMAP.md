# Roadmap

The milestones follow the project vision. A milestone is complete only when its
acceptance criteria in the approved plan are met and the evidence is recorded in
`docs/STATUS.md`.

| Milestone | Scope | Status |
|---|---|---|
| M0 | Project foundation: repository, build, core, CLI skeleton, CI, documentation | Done — `v0.1.0` |
| M1 | Headless core: reflection, ECS, command bus, project/scene format, offline CLI, fixed tick | Done — `v0.2.1` |
| M2 | Vulkan renderer, SDL3 window, offscreen screenshots, ImGui debug overlay | Done — `v0.3.0`, owner approved |
| M3 | Asset pipeline (glTF/GLB), cooked cache, live API; basic PBR and `DirectionalLight`; replace ilammy/msvc-dev-cmd with native vcvars | In progress — M3.4/M3.5 owner-approved; M3.6 owner-approved; M3.7 durable journal and mandatory Claude Opus re-review in progress |
| M4 | Jolt physics, Lua 5.4, gameplay modules, input recording | Planned |
| M5 | Open world: chunks, streaming, terrain LOD, double precision, persistent edits; shadows and point lights with the reference scene | Planned |
| M6 | World editor with ImGui, reflection-driven inspector, gizmos | Planned |
| M7 | AI-assisted game development, MCP adapter, automation tools | Planned |
| M8 | Profiling, optimization, packaging, stable stress tests | Planned |

The detailed M2 plan is in [`docs/M2_PLAN.md`](M2_PLAN.md). It was approved by
the owner on 2026-10-29. M2.1 implements the render device and window; M2.2 adds the swapchain and
frame loop; M2.3 adds glslc-compiled shaders and the first triangle; M2.4 adds
scene format v2, camera-relative scene rendering, depth and culling; M2.5 adds
offscreen rendering and screenshots; M2.6 adds the ImGui debug overlay and the
screenshot sandbox/limits. M2.7 adds the mandatory descriptor-indexing gate,
doctor report and exact screenshot error codes. M2.8 records the reference-scene
budget and measurement plan in [PERFORMANCE.md](PERFORMANCE.md). M2.9 adds the
windowed fly-camera run command and routes input to ImGui without mutating
authoritative state.

### M3 follow-ups (non-blocking)

- Non-indexed GLB primitives are rejected in M3.3. A later extension can
  generate indices in a separate, bounds-checked importer step after validating
  the POSITION accessor, its count and the vertex limit. It is not part of M3.
- Permanent libFuzzer targets (`engine_gltf_fuzz`, `engine_image_fuzz`) run in
  a short CI job with the checked-in fixtures as the seed corpus. Crashes found
  by fuzzing are added as regression tests.

## M0 substeps

| Substep | Scope | Status |
|---|---|---|
| M0.0 | Environment prerequisites, vcpkg, Vulkan verification | Done |
| M0.1 | Repository bootstrap, build skeleton, version command, ADRs | Done |
| M0.2 | vcpkg manifest, dependencies, warnings, format/tidy, sanitizer presets, and the Windows MSVC + vcpkg CI job | Done |
| M0.3 | Core: logging, config, filesystem sandbox, UUID, error handling | Done |
| M0.4 | CLI skeleton and `engine doctor --json` | Done |
| M0.5 | clang-format and clang-tidy checks in CI, ASan/UBSan Linux job, pinned CMake 4.4.5 and Ninja 1.13.2, lavapipe/Vulkan detection tests, Linux CI matrix, caching, artifacts and branch protection on `main` | Done |
| M0.6 | Full documentation, ADRs and VISION translation | Done |
| M0.7 | Hardening and M0 acceptance | Done — owner approved |

### M0.5 follow-ups (non-blocking)

Recorded from the M0.5 review. None of these block M0.5 or M0.6.

- Make the missing-ICD test assert `requirements.failed == ["vulkan"]` and print the JSON.
- `ilammy/msvc-dev-cmd@v1` was replaced by native `vswhere` + `vcvarsall.bat x64` in M3.1.
- Trigger the failure-artifact upload once on a scratch branch and move it from
  "implemented" to "automatically tested".
- Consider SHA-256 verification for the downloaded CMake/Ninja archives and SHA
  pinning of GitHub actions.

### Security follow-ups

- History rewrite completed on 2026-10-29. The local absolute path is removed
  from all revisions, and `engine_sensitive_scan` remains in the test suite.

### M1 follow-ups

- Extract and run the code examples in the documentation as part of CI, so the
  examples cannot drift from the implemented commands. Target M2 or later.

## M1 substeps

| Substep | Scope | Status |
|---|---|---|
| M1.1 | Reflection registry and JSON Schema generation | Done |
| M1.2 | ECS, UUID and transform hierarchy | Done |
| M1.3 | Command bus with validation and command log | Done |
| M1.4 | Transactions and undo/redo | Done |
| M1.5 | Project and scene format | Done |
| M1.6 | Offline CLI | Done |
| M1.7 | Fixed tick and determinism | Done |
| M1.8 | API reference, AI guide and acceptance script | Done |
| M1.9 | Hardening and M1 acceptance | Done |
| M1.10 | `--dry-run`, generated `API_REFERENCE.md`, security and review fixes | Done — `v0.2.1` |

## Deferred modules

These are intentionally outside M0/M1 and do not block the foundation:

| Area | Tentative milestone | Direction |
|---|---|---|
| Audio | M4/M5 | Dedicated `audio` module. Start with the SDL3 audio API; add 3D spatialization and mixing on top. Evaluate miniaudio only if SDL3 is insufficient. |
| Runtime UI | M6/M7 | Separate `ui` module or library choice (for example RmlUi, MIT). Dear ImGui remains editor/debug UI, not the shipping game HUD. |
| Animation | M3/M4 | Report animation channels in M3; import and skeletal animation as a system in `scene`/`render` in M4. A separate `animation` module only if the responsibility grows. |

## M2 reference scene budget

The M2 renderer defines the reference scene used by later performance work:

| Parameter | Budget |
|---|---|
| Resolution | 1920×1080 |
| Frame time | ≤ 16.6 ms |
| Entities | ≤ 1 000 |
| Triangles per frame | ≤ 250 000 after frustum culling |
| Draw calls per frame | ≤ 200 including the shadow pass |
| Lights | 1 directional + 4 point lights; 1 shadow-casting directional light |
| Materials | ≤ 100 unique PBR materials |
| Texture memory | ≤ 1 GiB |
| CPU time | ≤ 8 ms |
| GPU time | ≤ 12 ms |
| Build | Release; validation layers off during measurement, on during correctness tests |
| Machine | i7-1265U, Iris Xe, AC power, performance profile |

This budget is a target, not a promise. It is revised only with measured data
and an ADR.
