# Engine

An AI-first 3D game engine written in C++20 and Vulkan 1.3.

The project is in the M0 foundation phase. The engine is not usable for games yet.

## Requirements

- CMake 3.28 or newer
- Ninja
- A C++20 compiler: GCC 14+, Clang 18+, or MSVC 19.4+
- Python 3.10+ for the smoke tests

## Build

```bash
cmake --preset linux-debug
cmake --build --preset linux-debug
ctest --preset linux-debug --output-on-failure
```

Run the version command:

```bash
./build/linux-debug/tools/engine/engine --version --json
```

See `docs/BUILD.md` for platform notes, and `docs/STATUS.md` for the current state.

## Documentation

- `docs/VISION.md` — vision and architecture
- `docs/ROADMAP.md` — milestones
- `docs/ARCHITECTURE.md` — module boundaries (planned)
- `docs/decisions/README.md` — architecture decision records index
- `docs/AI_AGENT_GUIDE.md` — CLI and live API usage (planned)

## License

MIT. See `LICENSE`.
