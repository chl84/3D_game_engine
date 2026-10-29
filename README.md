# Engine

An AI-first 3D game engine written in C++20 and Vulkan 1.3.

The project is in the M0 foundation phase. The engine is not usable for games
yet; the first playable milestone is M2.

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

## Documentation

- `docs/VISION.md` — vision, technology stack and milestones
- `docs/ARCHITECTURE.md` — modules, dependency rules and data flow
- `docs/ROADMAP.md` — milestone status
- `docs/PROJECT_FORMAT.md` — planned project and scene format
- `docs/decisions/README.md` — architecture decision records index
- `docs/AI_AGENT_GUIDE.md` — CLI and live API usage
- `docs/TESTING.md` — test types and commands
- `docs/BUILD.md` — environment and build details

## License

MIT. See `LICENSE` and `THIRD_PARTY_NOTICES.md`.
