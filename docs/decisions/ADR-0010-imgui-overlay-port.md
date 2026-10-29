# ADR-0010: Dear ImGui through a local vcpkg overlay port

- **Status:** Accepted
- **Date:** 2026-10-29

## Context

ADR-0008 and the M2 plan approve Dear ImGui 1.93.0-docking for the debug
overlay and later editor UI. The pinned vcpkg baseline provides an `imgui`
port with `sdl3-binding` and `vulkan-binding` features.

The stock port has two consequences that conflict with the approved dependency
set:

- `sdl3-binding` depends on `sdl3` with its default features. On Linux, SDL3's
  default `ibus` feature pulls dbus, libsystemd, libcap, libxcrypt and their
  build tools. The project deliberately builds SDL3 with `default-features:
  false` and only the `vulkan`, `x11` and `wayland` features. Adding the
  dbus/systemd chain would be a new, unapproved dependency expansion.
- `vulkan-binding` depends on the vcpkg `vulkan` stub, which installs a second
  Vulkan loader. The engine already uses volk, which loads the pinned system or
  LunarG loader dynamically (ADR-0001). A second loader is redundant and can
  interpose the wrong `vkGetDeviceProcAddr` symbol.

The stock ImGui Vulkan backend also expects Vulkan function prototypes unless
it is built with `IMGUI_IMPL_VULKAN_USE_VOLK`. Mixing the two loaders caused a
crash at renderer creation in the first integration attempt.

## Decision

Use a local vcpkg overlay port at `cmake/vcpkg-overlay/imgui`:

- It pins the exact commit `b916186e66aa3c02b72c6ca17455516512ed8049`
  (the annotated `v1.93.0-docking` tag) and the SHA512 of that commit's source
  archive. A moved tag or rewritten commit fails the build.
- Its `sdl3-binding` feature depends on `sdl3` with `default-features: false`.
- Its `vulkan-binding` feature depends on `volk`, defines
  `IMGUI_IMPL_VULKAN_USE_VOLK` and links `volk::volk_headers` instead of
  `Vulkan::Vulkan`.
- `vcpkg-configuration.json` registers the overlay port directory.

The overlay is enabled by default in debug builds and disabled by default in
release builds through `SwapchainOptions::debug_overlay`.

## Consequences

- The resolved dependency graph stays at the approved SDL3 feature set; no
  dbus, systemd or vcpkg Vulkan loader is introduced.
- ImGui uses the same volk-loaded function table as the renderer.
- The local port is a copy of the upstream build files. When the vcpkg
  baseline moves, the overlay port must be updated to the new port version and
  source hash in the same change.
- The debug overlay is compiled into the renderer library, but release builds
  do not enable it by default. It reads only `SceneView` and never mutates
  authoritative state.
- The renderer owns the ImGui context while the overlay is enabled. A future
  editor that also owns ImGui must take over context ownership explicitly.

## Alternatives considered

- **Use the stock port and accept dbus/systemd/libxcrypt.** Rejected: this is a
  new transitive dependency chain that the owner has not approved, and it
  contradicts the documented SDL3 configuration.
- **Use CMake FetchContent for ImGui.** Rejected: ADR-0001 selects vcpkg
  manifest mode as the single version source; FetchContent would add a second
  dependency mechanism.
- **Write custom SDL3 and Vulkan backends.** Rejected: unnecessary maintenance
  and a much larger surface for correctness bugs. The upstream backends are
  small and well tested.
- **Link the vcpkg Vulkan loader and drop volk for ImGui only.** Rejected: two
  loaders in one process cause symbol-interposition problems, as observed
  during integration.
