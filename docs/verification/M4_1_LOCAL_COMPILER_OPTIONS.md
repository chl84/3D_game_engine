# M4.1 local compiler gate — binding decision required

**Current answer: no.** The adopted admission pin, unmodified sol2 release3.3.0
(`eba86625b707e3c8c99bbfc4624e51f42dc9e561`), does not compile with this machine's
default GCC16.2.1 or Clang23.1.3. Both reject the nonexistent `construct` call
inside `sol::optional<T&>::emplace` at line2194. This fails even though that
function is not used by the probe. This is a hard template error, not an engine
warning that should be suppressed. Normal full builds including admission remain
broken locally until the owner selects and validates a remedy.

The engine CLI currently has no production Lua host. Building that CLI alone
would not prove sol2 adoption works. The reproduced failure is in its default
admission target. Reference GCC14/Clang18/MSVC success remains valid for the old
pin, but does not satisfy the owner's new default-local-compiler requirement.

## Four concrete alternatives

| Alternative | Evidence obtained here | Cost/risk and disposition |
|---|---|---|
| **New immutable upstream pin, recommended** | Commit `c1f95a773c6f8f4fde8ca3efe872e7286afe4444` (headers3.5.0) compiled all110 engine build steps with GCC16 and Clang23, own warnings-as-errors. Lua admission/Jolt admission/version tests passed3/3 on each. Separate protected Lua diagnostic probes passed both compilers. | Uses upstream fixes without maintaining a fork. It is36 commits/166 repository files/33 headers beyond the approved pin, including exception/error changes. Requires owner approval and fresh GCC14/Clang18/MSVC plus local admission/sanitizer/VM evidence before adoption. Not adopted here. |
| Narrow overlay backport to3.3.0 | A five-line optional-reference fix in an isolated header copy compiled and passed protected Lua probes on both local compilers with the same -Werror policy. [Exact proposed patch](M4_1_SOL2_OPTIONAL_BACKPORT.patch). | Smallest behavioral delta, preserves the already reviewed3.3.0 body. We must own/test the patch and its future rebases; this is not an upstream release. Good fallback if the wider upstream pin fails admission. Full engine/reference-matrix tests were not run for this backport. Not installed in the overlay. |
| Locked local GCC14 toolchain | The original pin passed GCC14 reference CI. No local GCC14 executable is installed, so a local pinned-toolchain build was not performed. | Can reproduce the reference environment using an explicitly pinned compiler/container, but does not make unpatched3.3.0 work with the requested default GCC16/Clang23. A workaround/exception would need owner approval. Do not replace system compilers or modify desktop configuration. |
| Engine-owned bindings generated from reflection using Lua's C API | Architectural alternative only; no implementation or performance claim. Keep approved Lua5.4.9 built as C++ and protected calls. | Removes sol2/template compatibility risk and keeps the registry as source of truth, but the engine must own stack discipline, conversion, userdata lifetime, protected exception/OOM behavior and quota tests. It changes the binding decision, needs its own bounded plan/probe, and invalidates assumptions about binding memory/startup. Highest immediate implementation scope. |

Recommend the exact newer upstream pin above, subject to owner approval and a
fresh admission matrix. Both native full-engine compilations already work, so
the local path is demonstrated. The small backport is a concrete reserve option,
not a second simultaneously adopted backend. Avoid adding a custom binding layer
before there is evidence that the maintained library path cannot meet the gates.

## Isolation and verification limits

No manifest, adopted overlay, production source or standard preset was changed.
Scratch trials live under ignored `build/m4-local-alternatives/`. Full-engine
trials reused the existing pinned, GCC16-built vcpkg installation with manifest
installation disabled; they do not claim a fresh Clang23-built dependency set.
All engine translation units compiled with the selected local compiler. The
separate diagnostic Lua source labels the alternative explicitly; the unchanged
full-engine probe still contains its original3.3.0 source-label literal, which is
not used as candidate identity evidence. Generated include commands and source
archive digests identify the actual candidate.

The only new focused runtime checks were the protected Lua/Jolt admission and
version tests. No full native render/acceptance suite, candidate reference-
compiler matrix or fresh candidate sanitizer/fuzz result is claimed. Those
remain adoption gates after the owner chooses a remedy. The earlier3.3.0 CI and
Astra verdict must not be relabelled as approval of this newer source.

Upstream candidate archive SHA-512:

```text
c02779f48c826d0bce2fd6ba171943ad8be2f87763e4e8997f09354656c69ac51a7e79bb330ca998a51c0ae971d929d82f64b66bf56b9de4a21df4a193b2f0f0
```

[Structured local evidence](M4_1_LOCAL_COMPILER_EVIDENCE.json) retains probe
outputs, compiler commands and scope. Sanitized logs retain both original
failures and full candidate builds. [VM costs/capacity](M4_1_VM_COMPARISON.md)
separate the original reference measurement from the diagnostic extension.

## Accepted decisions and next gate

The owner accepted ADR-0019's concrete schemas, strict unknown-component rejection,
explicit-save-only migration and64MiB/depth32 bounds. M5 world data goes into
separate chunk files, not scene JSON. These are recorded; readers/writers and
world storage are not implemented by this documentation update.

ADR-0016 and VM topology remain unresolved. **Do not start M4.2.** After the
owner selects the binding remedy, implement that exact pin/patch, verify default
local builds and the reference matrix, refresh formal VM costs for the adopted
binding source, then return the remaining ADR/topology decision to the owner.
The adoption check must include clean/matching local dependency builds, not only
the cached-library feasibility trials above, and a focused optional-reference
emplace/rebinding regression. Keep existing compiler/sanitizer tests intact.

The separate hard-quota capacity stress worker exposed a Lua panic/SIGABRT in
unprotected setup at exhaustion. That trial uses the narrow3.3.0 backport, not
the proposed newer commit. It does not prove a production-safe OOM path; setup
and cleanup protection/fault injection remain a required production-host gate.

## Primary upstream references checked

- [Exact recommended commit and optional-reference correction](https://github.com/ThePhD/sol2/commit/c1f95a773c6f8f4fde8ca3efe872e7286afe4444).
- [Full source comparison against the approved pin](https://github.com/ThePhD/sol2/compare/eba86625b707e3c8c99bbfc4624e51f42dc9e561...c1f95a773c6f8f4fde8ca3efe872e7286afe4444).
- [Upstream report of the same modern-compiler failure](https://github.com/ThePhD/sol2/issues/1695).
- [Upstream release listing](https://github.com/ThePhD/sol2/releases): release3.3.0
  and newer tags/commits are distinct provenance; a header string is not a pin.
