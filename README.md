# Engine

An AI-first 3D game engine written in C++20 and Vulkan 1.3.

M0–M3 are released as `v0.4.0`: GLB import/cooking/reimport, stable asset
placement, PBR rendering and authenticated local live control are implemented
and verified. M4 physics/gameplay is planned in [docs/M4_PLAN.md](docs/M4_PLAN.md)
and is owner-approved with M4.1 compiler/VM/schema conditions; implementation
is starting with isolated admission probes. GPT-6 Astra high is the active independent reviewer
(see `REVIEWER.json`). The engine is not ready for shipping games.

## Requirements

- CMake 3.28 or newer
- Ninja
- A C++20 compiler: GCC 14+, Clang 18+, or MSVC 19.4+
- Python 3.10+ for the smoke tests
- vcpkg, bootstrapped at `~/.local/share/vcpkg` and exported as `VCPKG_ROOT`

## Build

```bash
export VCPKG_ROOT="$HOME/.local/share/vcpkg"
cmake --preset linux-debug
cmake --build --preset linux-debug
ctest --preset linux-debug --output-on-failure
```

The first configure downloads and builds the pinned vcpkg dependencies. Run the
version command:

```bash
./build/linux-debug/tools/engine/engine --version --json
./build/linux-debug/tools/engine/engine doctor --json
```

See `docs/BUILD.md` for platform notes, and `docs/STATUS.md` for the current state.

## Try the renderer

A minimal sample project is included:

```bash
engine run samples/m2-demo scenes/main.scene.json
```

Use `WASD`, `Q`/`E`, Shift, the right mouse button, `F1` for the debug overlay
and `Escape` to close. See `samples/m2-demo/README.md` for details.

## Documentation

- `docs/VISION.md` — vision, technology stack and milestones
- `docs/ARCHITECTURE.md` — modules, dependency rules and data flow
- `docs/ROADMAP.md` — milestone status
- `docs/M4_PLAN.md` — proposed physics/gameplay plan and approval decisions
- `docs/PROJECT_FORMAT.md` — planned project and scene format
- `docs/decisions/README.md` — architecture decision records index
- `docs/AI_AGENT_GUIDE.md` — CLI and live API usage
- `docs/TESTING.md` — test types and commands
- `docs/BUILD.md` — environment and build details
- `docs/PERFORMANCE.md` — reference-scene budget and measurement plan
- [Manual M3 Blender test](samples/m3-manual/README.md) — export, import, place, run and reimport your own model

## License

MIT. See `LICENSE` and `THIRD_PARTY_NOTICES.md`.

## Import a model

```bash
engine asset import samples/m2-demo model.glb --name Model --json
engine asset list samples/m2-demo --json
engine asset inspect samples/m2-demo <assetId> --json
engine asset validate samples/m2-demo <assetId> --json
engine asset reimport samples/m2-demo <assetId> --dry-run --json
```

Import accepts a self-contained indexed GLB with embedded PNG/JPEG textures.
Assets are cooked for placement and PBR rendering through `engine asset place`,
`engine run` and `engine screenshot`. See [the agent guide](docs/AI_AGENT_GUIDE.md).
