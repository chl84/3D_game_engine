# ADR-0004: Pinned build tools and CI quality gates

- **Status:** Accepted
- **Date:** 2026-10-29

## Context

M0 targets Linux x64 and Windows x64 and must be reproducible from a clean
clone. The Ubuntu and Windows runners ship different CMake, Ninja and compiler
versions, and those versions change over time. During M0.5 the quality job also
showed that clang-tidy did not analyse project headers because the header
filter did not match absolute paths.

The milestone plan requires CMake 4.4.5 and Ninja 1.13.2, GCC 14, Clang 18,
clang-format, clang-tidy, ASan/UBSan and branch protection on `main`.

## Decision

- CI pins CMake 4.4.5 and Ninja 1.13.2 by downloading the official release
  archives and prepending them to `PATH`. Every job fails if `cmake --version`
  or `ninja --version` does not match.
- Linux uses GCC 14 (`g++-14`) and Clang 18. Windows uses MSVC from Visual
  Studio 2022.
- The quality job runs clang-format 18 with `--dry-run --Werror` and
  clang-tidy 18 with `--warnings-as-errors='*'`.
- `.clang-tidy` uses `HeaderFilterRegex: '.*/(engine|tools|tests)/.*'` so
  project headers are treated as user code.
- A Linux job runs ASan and UBSan with `detect_leaks=1` and
  `halt_on_error=1`.
- Linux jobs install lavapipe and verify that `engine doctor --require vulkan
  --json` exits 0, reports `llvmpipe`, and exits 8 when the ICD is missing.
- Failed jobs upload CTest output and doctor JSON as artifacts.
- Branch protection on `main` requires the five CI jobs. Force pushes and
  deletions are disabled; `enforce_admins` is off during M0.

## Consequences

- Builds are reproducible across runner image updates.
- A CMake, Ninja, compiler or clang-tool update is an explicit change with a
  visible diff, not an implicit runner change.
- CI depends on the availability of the Kitware and Ninja release archives.
- Local versions may be newer. Differences are documented in `BUILD.md`; CI is
  the source of truth.
- The header filter must be kept in sync with the repository layout.

## Alternatives considered

- **Runner default tools:** simple, but not reproducible and already caused
  clang-tidy to miss project headers.
- **Distribution packages only:** versions differ between Omarchy and Ubuntu.
- **A third-party setup action:** adds another external action and hides the
  downloaded versions.
