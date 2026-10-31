# ADR-0017: Deterministic Jolt profile for M4

- **Status:** Accepted by the owner on 2026-10-31; implementation not yet complete
- **Date:** 2026-10-31
- **Scope:** [M4 proposal](../M4_PLAN.md); follows ADR-0002 and the existing
  double-position scene model.

## Context

The vision selects Jolt with deterministic configuration and headless replay.
A fixed timestep alone is insufficient: build flags, body/API ordering, queries,
events, input and game state also affect results. Engine positions are already
double precision. Jolt source/configuration must be identical across the
supported compiler matrix, and no GPU/window dependency may enter physics.

## Decision

Pin **Jolt 5.6.0**, MIT, to an immutable source/hash through vcpkg. Configure a
static CPU-only profile, kept private behind engine physics types. Record the
effective compile definitions and flags, not just intended CMake cache values.
Newer upstream development removes some settings; use the pinned version's
actual API and do not silently transplant settings from master.

| Setting | Proposed value |
|---|---|
| `JPH_BUILD_SHARED_LIBS` | `OFF` |
| `DOUBLE_PRECISION` | `ON` for positions; matching engine/Jolt boundary types |
| `CROSS_PLATFORM_DETERMINISTIC` | `ON` |
| `INTERPROCEDURAL_OPTIMIZATION` | `OFF` for the M4 reference profile |
| `USE_SSE4_1`, `USE_SSE4_2` | `ON` on the selected x64 targets |
| `USE_AVX`, `USE_AVX2`, `USE_AVX512`, `USE_F16C`, `USE_FMADD` | `OFF` |
| `USE_LZCNT`, `USE_TZCNT` | `OFF` for the initial common x64 profile |
| `JPH_USE_DX12`, `JPH_USE_VK`, `JPH_USE_MTL`, `JPH_USE_CPU_COMPUTE` | `OFF`; these optional compute interfaces are outside M4 rigid-body scope |
| `OVERRIDE_CXX_FLAGS` | `OFF`; audit that sanitizer/warning settings survive |
| Runtime `PhysicsSettings::mDeterministicSimulation` | Explicitly `true` in pinned 5.6.0 |
| Job system | `JobSystemSingleThreaded`, no engine worker pool in M4 |
| Tick | Exactly 60Hz fixed dt; collision steps and solver settings explicit/fingerprinted |

Proposed runtime defaults: gravity `(0,-9.81,0)`, one collision step per tick,
10 velocity / 2 position solver iterations, sleeping enabled, at most 4,096
bodies, 16,384 body pairs, 16,384 contact constraints and 64MiB temporary
allocator capacity. Validate these on admission, capture allocation/Update
capacity errors and fail the session rather than continuing with lost contacts.
Every value is recorded in the replay profile; changing one changes its hash.

Match the existing target's MSVC runtime linkage; do not let the Jolt default
silently introduce a different CRT. Preserve upstream deterministic floating
point controls. Engine simulation and script numeric boundaries must not use
fast-math or unchecked FMA contraction; use audited per-target compiler flags,
including `-ffp-contract=off` on GCC/Clang and an appropriate strict MSVC profile.
Do not weaken warnings or sanitizers to make a port compile. Configure upstream
examples/tests/extra tools off when embedding; use engine-owned admission tests.

Create/destroy/apply body state in UUID/recorded command order. Keep a stable
UUID-to-body mapping; transient Jolt IDs never become persistent game IDs.
Capture callbacks into bounded storage and deliver sorted engine events after
the physics update, never call Lua or perform structural edits in a listener.
Use narrow-phase queries and sort/tie-break hits by fraction/UUID/subshape.
Broadphase result arrival order is not a deterministic engine query contract.

Use primitive colliders and a virtual capsule character first. New dynamic
mesh cooking, terrain, constraint/vehicle/ragdoll systems and parallel jobs are
out of scope. Preserve double world positions without adding M5 origin shifting.
Body/contact/event capacity failures return stable terminal session errors,
not successful simulation with dropped contacts. Initial capacities and temp
allocation are bounded and validated in M4.1, then recorded in the profile.

## Determinism claims and evidence

- **Mandatory same-profile replay:** same initial authoring state, source hashes,
  seeds and input frames produce identical canonical state at every tick in
  fresh processes. Hash explicit normalized fields, not raw Jolt memory.
- **Mandatory cross-compiler physics suite:** compare deterministic primitive,
  stacking, contact, raycast and character fixtures across GCC14, Clang18 and
  MSVC. Each job exports canonical traces for a comparison job. A mismatch
  blocks acceptance; do not silently replace exact equality with tolerances.
- **Whole-game scope:** same-build replay must be exact. Passing the physics
  suite alone does not prove arbitrary Lua transcendental functions or custom
  C++ module math are cross-platform deterministic. The acceptance sample uses
  approved operations; broader claims require their own evidence.
- Store source/profile/compiler/architecture fingerprints in replay. Reject
  incompatible recordings before stepping. No across-Jolt-version, savegame
  or rollback-networking compatibility is promised.

Single-threaded execution limits throughput but makes the initial ordering and
failure model auditable. Measure it on the reference machine. Parallel physics
requires a later profiling-backed proposal; do not build the M5 job system early.

## Alternatives

- Default Jolt flags: simpler integration, insufficiently explicit for the
  engine's cross-compiler physics tests and replay provenance.
- Fast SIMD/FMA or parallel workers immediately: potential throughput benefit,
  extra configuration/ordering proof before a measured need.
- Fixed-point physics or a different library: new technology decision, not a
  fallback authorized by a failed Jolt admission test.

## Primary references checked for this proposal

- [Jolt 5.6.0 release](https://github.com/jrouwe/JoltPhysics/releases/tag/v5.6.0)
  and [license](https://github.com/jrouwe/JoltPhysics/blob/v5.6.0/LICENSE).
- [Pinned CMake options and FP controls](https://github.com/jrouwe/JoltPhysics/blob/v5.6.0/Build/CMakeLists.txt).
- [Pinned deterministic runtime setting](https://github.com/jrouwe/JoltPhysics/blob/v5.6.0/Jolt/Physics/PhysicsSettings.h).
- [Upstream determinism conditions and query/listener caveats](https://jrouwe.github.io/JoltPhysics/index.html#deterministic-simulation).

The profile is an engineering proposal, not a report of completed engine tests.
