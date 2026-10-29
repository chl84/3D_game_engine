# ADR-0008: Renderer dependencies for M2

- **Status:** Accepted
- **Date:** 2026-10-29

## Context

M2 adds the Vulkan renderer. The engine needs a cross-platform window with a
Vulkan surface, GPU memory allocation and matrix/vector math. The dependency
policy requires owner approval and an ADR for new dependencies. The owner
approved the M2 plan, including SDL3, Vulkan Memory Allocator, GLM, shaderc,
Dear ImGui and `stb_image_write`, on 2026-10-29.

M2.1 introduces the first three. shaderc, Dear ImGui and `stb_image_write` are
added in the substeps that use them (M2.3, M2.5 and M2.6), so the build does not
carry unused dependencies.

## Decision

- **SDL3 3.4.18** owns the window, input, events and the Vulkan surface. It is
  built with the `vulkan`, `x11` and `wayland` features. `ibus`, audio backends
  and KMSDRM are disabled to avoid the dbus/libxcrypt build chain.
- **Vulkan Memory Allocator 3.5.0** is the GPU allocator. It is header-only,
  compiled exactly once with dynamic Vulkan function loading through volk.
- **GLM 1.0.3** is the renderer math library. It is used only by
  `engine/render`; authoritative scene data keeps `core::Vec3` and
  `core::Quat`.
- Versions are pinned through the vcpkg manifest with exact overrides.
- The Vulkan loader, headers and validation layers continue to come from the
  Vulkan SDK, not from a second vcpkg Vulkan installation.

## Consequences

- One windowing dependency covers window, input and surface creation.
- SDL3 adds X11 and Wayland development headers as Linux build prerequisites,
  which are documented in `docs/BUILD.md` and installed in CI.
- VMA hides Vulkan allocation details and keeps memory type selection in one
  place.
- GLM introduces a second math vocabulary next to `core::Vec3`/`Quat`. The
  boundary is explicit: GLM stays inside `render`.
- The render tests exercise the dependencies on Linux and skip only when the
  Vulkan loader is absent.

## Alternatives considered

- **GLFW instead of SDL3:** smaller, but SDL3 is already the planned input and
  window layer and covers audio and gamepad input for later milestones.
- **Hand-written Vulkan allocator:** avoids a dependency but duplicates a
  well-tested allocator and increases the risk of memory-type bugs.
- **DirectXMath or custom math:** DirectXMath is not portable; a custom math
  library would duplicate GLM without a demonstrated need.
- **Adding all M2 dependencies at once:** rejected because unused dependencies
  would enlarge the build and the license surface before the substeps need
  them.
