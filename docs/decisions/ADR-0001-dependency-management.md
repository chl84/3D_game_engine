# ADR-0001: Dependency management with vcpkg

- **Status:** Accepted
- **Date:** 2026-10-29

## Context

The engine targets Linux x64 and Windows x64. Dependencies must resolve to the
same versions on both platforms, be compatible with the MIT license, and be
reproducible in CI. The candidate mechanisms were a vcpkg manifest with a
pinned baseline and exact overrides, or CMake FetchContent/CPM with pinned
commit hashes.

## Decision

Use vcpkg in manifest mode with:

- `builtin-baseline`: `0e5eb43501dab17aa055dd54bd7a8c018f330fe2` (2026-10-28)
- Exact `overrides` for every direct dependency
- Triplets `x64-linux` and `x64-windows`
- A vcpkg checkout pinned to the same commit in CI
- Binary caching in CI

The Vulkan SDK (headers, loader, validation layers, `glslc` and `vulkaninfo`)
is installed separately from LunarG and pinned to version 1.4.363.0. vcpkg is
used for `volk` and Vulkan Memory Allocator, not for a second set of Vulkan
headers.

Dependencies are added at the start of the milestone that needs them, not all
at once. Every addition updates `docs/DEPENDENCIES.md` and passes the license
check.

## Consequences

- One version source for Linux and Windows.
- Binary caching keeps CI build times reasonable.
- vcpkg adds a bootstrap step and a baseline to maintain.
- A vcpkg port that lags upstream may require a local overlay port or a
  newer baseline; either change needs an ADR.
- The license allowlist is enforced in CI.

## Alternatives considered

- **CMake FetchContent/CPM with pinned commits:** fewer tools, but more CMake
  glue, weaker transitive dependency handling and no binary cache.
- **System packages only:** versions differ between Omarchy and Ubuntu, so this
  cannot be the reproducible source of truth.
