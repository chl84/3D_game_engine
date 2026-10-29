# Dependencies

**Status:** M2.6. The vcpkg manifest is active for nlohmann/json 3.12.0,
spdlog 1.17.0, CLI11 2.7.2, `tl::expected` 1.3.1, Catch2 3.16.1, volk
1.4.363.0, EnTT 4.0.0, json-schema-validator 2.4.0, SDL3 3.4.18, Vulkan Memory
Allocator 3.5.0, GLM 1.0.3, stb 2026-08-02 and Dear ImGui 1.93.0-docking. The
core library uses nlohmann/json, spdlog and `tl::expected`; the CLI uses CLI11
and volk; Catch2 is test-only. EnTT is the ECS; json-schema-validator validates
command and format payloads. Dear ImGui draws the debug overlay in the windowed
renderer. Python `jsonschema` is a development-only contract-test dependency.

## Policy

- The engine is MIT-licensed. Every dependency must be compatible with MIT and
  use a permissive license: MIT, BSD, Apache-2.0, Zlib, BSL-1.0 or CC0.
- Every dependency is recorded here with version, license, purpose and the
  milestone that introduces it.
- Versions are pinned through a vcpkg manifest with a fixed baseline and exact
  overrides. The Vulkan SDK is pinned separately.
- CI runs a license allowlist check. Unknown or copyleft licenses fail the
  build unless the owner explicitly approves them.
- Transitive dependencies are covered by the same policy.
- A new dependency or a version upgrade requires an ADR and owner approval.

## Approved dependencies

| Dependency | Version | License | Purpose | Milestone | Rationale |
|---|---|---|---|---|---|
| Catch2 | 3.16.1 | BSL-1.0 AND BSD-3-Clause | Unit and integration testing | M0 | Simple test syntax, sections and generators, good CMake/CTest integration, no runtime dependency in the engine. Both licenses are permissive and compatible with MIT. Approved by the owner on 2026-10-29. |
| tl::expected | 1.3.1 | CC0-1.0 | `Result<T>` for expected errors | M0 | Provides an `expected`-like type in C++20 without requiring C++23 `std::expected`. Mature, header-only, and easy to replace with `std::expected` later. CC0 is a public-domain dedication and compatible with MIT. Approved by the owner on 2026-10-29. |
| nlohmann/json | 3.12.0 | MIT | JSON parsing, canonical serialization and schemas | M0 | Mature header-only JSON library with deterministic object key ordering and good diagnostics. |
| spdlog | 1.17.0 | MIT | Logging backend | M0 | Fast compiled logger with console and file sinks; the JSONL formatter stays in `core`. |
| CLI11 | 2.7.2 | BSD-3-Clause | Command-line parsing | M0 | Header/compiled library with subcommands, global options and predictable error handling. |
| volk | 1.4.363.0 | MIT | Vulkan function loading for `engine doctor` | M0 | Loads the Vulkan loader dynamically and reports a missing loader gracefully on Windows CI. |
| EnTT | 4.0.0 | MIT | Entity-component system | M1 | Header-only, fast sparse-set ECS with stable entity handles. Reflection remains in `core` (ADR-0007), so the ECS dependency stays small. Approved in the M1 plan. |
| json-schema-validator (`pboettch`) | 2.4.0 | MIT | Runtime JSON Schema draft-07 validation | M1 | nlohmann/json-based validator with draft-07 support. Used for command and format validation; Python `jsonschema` remains the contract-test validator. Approved in the M1 plan. |
| SDL3 | 3.4.18 | Zlib AND MIT AND Apache-2.0 AND BSD-3-Clause | Window, input, events and Vulkan surface creation | M2 | Mature cross-platform windowing with an explicit Vulkan feature. Built with the `vulkan`, `x11` and `wayland` features; `ibus` and audio features are disabled. All included components are permissive. Approved in the M2 plan on 2026-10-29. |
| Vulkan Memory Allocator | 3.5.0 | MIT | GPU memory allocation | M2 | AMD's official Vulkan allocator. Header-only, integrates with volk, and is the allocator named in the architecture. Approved in the M2 plan on 2026-10-29. |
| GLM | 1.0.3 | MIT | Matrix and vector math for the renderer | M2 | Header-only C++ math library with Vulkan-compatible clip-space support. The engine keeps its own `Vec3`/`Quat` for scene data and uses GLM only inside `render`. Approved in the M2 plan on 2026-10-29. |
| stb_image / stb_image_write (stb) | 2026-08-02 | MIT OR Unlicense | PNG encoding and golden-image decoding | M2 | Single-header PNG writer/reader; no runtime library. Used by the screenshot command and golden-image test. The owner explicitly approved the dependency on 2026-10-29. |
| Dear ImGui (`docking-experimental`) | 1.93.0-docking | MIT | Debug overlay and later editor UI | M2 | Immediate-mode UI for the debug overlay. Built with the SDL3 and Vulkan backends and volk, consistent with the renderer's dynamic loader. The owner approved it in the M2 plan on 2026-10-29. |

## Planned dependencies

These are approved in principle by the milestone plan but are not in the build
yet. The exact versions are pinned at the start of the milestone that needs
them.

| Dependency | Version | License | Purpose | Milestone |
|---|---|---|---|---|
| fastgltf | 0.9.1 | MIT | glTF 2.0 / GLB import | M3 |
| Jolt Physics | 5.6.0 | MIT | Physics | M4 |
| Lua | 5.4.8 | MIT | Scripting | M4 |
| sol2 | 3.5.0 | MIT | Lua bindings generated from reflection | M4 |

## Tooling and SDK

| Tool | Version | License | Purpose |
|---|---|---|---|
| vcpkg | baseline `0e5eb43501dab17aa055dd54bd7a8c018f330fe2` | MIT | Dependency management |
| LunarG Vulkan SDK | 1.4.363.0 | Apache-2.0 / MIT components | Vulkan headers, loader, validation layers, `glslc`, `vulkaninfo` |
| glslc (shaderc CLI) | distro package on Linux; Vulkan SDK 1.4.363 on Windows | Apache-2.0 | GLSL to SPIR-V build step; not linked into the engine |
| CMake | 4.4.5 pinned in CI; 4.4.4 on the development machine | BSD-3-Clause | Build system |
| Ninja | 1.13.2 pinned in CI and on the development machine | Apache-2.0 | Build tool |
| GCC | 14 in CI (`g++-14`); local 16.2.1 | GPL-3.0 with GCC Runtime Library Exception | Linux compiler |
| Clang | 18 in CI | Apache-2.0 with LLVM exceptions | Linux compiler |
| MSVC | Visual Studio 2022 17.14 | Proprietary | Windows compiler |
| Python | 3.10+ | PSF-2.0 | Test and automation scripts |
| jsonschema | 4.26.0 | MIT | Development-only JSON Schema validation in contract tests |

Compiler runtime exceptions and the Python license are compatible with
distributing an MIT-licensed engine. Compilers and SDKs are build tools, not
linked engine dependencies. `THIRD_PARTY_NOTICES.md` contains the notice table
and the license texts used for redistribution.

## Notes

- Lua 5.4.9 is the latest 5.4 patch, but the pinned vcpkg baseline contains
  5.4.8. The 5.4.9 update is deferred until M4 and will use a vcpkg update or a
  local overlay port.
- JSON Schema validation uses draft-07 because `json-schema-validator` 2.4.0
  documents draft-07 support, not draft 2020-12.
- The Vulkan SDK is not managed by vcpkg. Using the SDK for headers and tools
  avoids duplicate Vulkan headers and keeps validation layers and `glslc` on
  the same version.
- SDL3 is built with the `vulkan`, `x11` and `wayland` features only. `ibus`,
  audio backends and KMSDRM are disabled; this avoids the dbus and libxcrypt
  build chain. Linux builds need the X11 and Wayland development headers
  (`libx11-dev`, `libxft-dev`, `libxext-dev`, `libwayland-dev`,
  `libxkbcommon-dev`, `libegl1-mesa-dev` on Debian/Ubuntu).
- VMA is header-only. `VMA_IMPLEMENTATION` is compiled exactly once, in
  `engine/render/src/vma.cpp`; dynamic Vulkan function loading is used because
  volk defines `VK_NO_PROTOTYPES`.
- GLM is used only inside `engine/render`. Authoritative scene data keeps the
  `core::Vec3` and `core::Quat` types.
- The `stb` port is header-only. `stb_image_write` is used by the screenshot command and `stb_image`/`stb_image_write` by the golden-image test; neither is linked into the renderer library.
- Dear ImGui uses a local overlay port in `cmake/vcpkg-overlay/imgui`.
  The stock port's `sdl3-binding` enables SDL3's default `ibus` feature, which
  pulls dbus, systemd and libxcrypt, and its `vulkan-binding` pulls a second
  Vulkan loader from vcpkg. The overlay port keeps `sdl3` at
  `default-features: false` and builds the Vulkan backend against `volk`.
  ADR-0010 records the decision. The overlay port is pinned to the same source
  archive and version as the vcpkg baseline; update both together.
- `glslc` is executed as a build tool. The shaderc library is not linked and is
  not a runtime dependency. Linux CI installs the distro `glslc` package;
  Windows CI installs the LunarG Vulkan SDK 1.4.363 and adds its `Bin`
  directory to `PATH`.
- The generated SPIR-V is embedded in `engine_render` as C++ word arrays, so
  tests and tools do not need shader files at runtime.
