# Roadmap

The milestones follow the project vision. A milestone is complete only when its
acceptance criteria in the approved plan are met and the evidence is recorded in
`docs/STATUS.md`.

| Milestone | Scope | Status |
|---|---|---|
| M0 | Project foundation: repository, build, core, CLI skeleton, CI, documentation | In progress (M0.0–M0.6 done) |
| M1 | Headless core: reflection, ECS, command bus, project/scene format, offline CLI, fixed tick | Planned |
| M2 | Vulkan renderer, SDL3 window, offscreen screenshots, ImGui debug overlay | Planned |
| M3 | Asset pipeline (glTF/GLB), cooked cache, live API | Planned |
| M4 | Jolt physics, Lua 5.4, gameplay modules, input recording | Planned |
| M5 | Open world: chunks, streaming, terrain LOD, double precision, persistent edits | Planned |
| M6 | World editor with ImGui, reflection-driven inspector, gizmos | Planned |
| M7 | AI-assisted game development, MCP adapter, automation tools | Planned |
| M8 | Profiling, optimization, packaging, stable stress tests | Planned |

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
| M0.7 | Hardening and M0 acceptance | Next |

### M0.5 follow-ups (non-blocking)

Recorded from the M0.5 review. None of these block M0.5 or M0.6.

- Make the missing-ICD test assert `requirements.failed == ["vulkan"]` and print the JSON.
- Replace or upgrade `ilammy/msvc-dev-cmd@v1`, which still emits a Node 20 warning.
- Trigger the failure-artifact upload once on a scratch branch and move it from
  "implemented" to "automatically tested".
- Consider SHA-256 verification for the downloaded CMake/Ninja archives and SHA
  pinning of GitHub actions.

### M1 follow-ups

- Extract and run the code examples in the documentation as part of CI, so the
  examples cannot drift from the implemented commands. This lands together
  with API generation in M1.

## Deferred modules

These are intentionally outside M0/M1 and do not block the foundation:

| Area | Tentative milestone | Direction |
|---|---|---|
| Audio | M4/M5 | Dedicated `audio` module. Start with the SDL3 audio API; add 3D spatialization and mixing on top. Evaluate miniaudio only if SDL3 is insufficient. |
| Runtime UI | M6/M7 | Separate `ui` module or library choice (for example RmlUi, MIT). Dear ImGui remains editor/debug UI, not the shipping game HUD. |
| Animation | M3/M4 | Import glTF animations in M3; skeletal animation as a system in `scene`/`render` in M4. A separate `animation` module only if the responsibility grows. |

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
