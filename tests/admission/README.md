# M4.1 admission probes

These are isolated test executables, not a Lua/gameplay runtime adoption.
No VM topology is selected. Root engine commands do not link the probe or open
a Lua/Jolt runtime. Existing engine tests remain registered and unchanged.

Pinned overlays:

- Lua 5.4.9 as C++, official archive SHA-256
  `2335b6c582a52654f94612bf10d2f4672805d05329aa6568b1d8cd9e5c6fb8e6`.
- sol2 v3.3.0 peeled commit `eba86625b707e3c8c99bbfc4624e51f42dc9e561`.
  That release's header still reports 3.2.3; retain/report the upstream metadata
  discrepancy instead of modifying it or claiming a different source version.
- Jolt v5.6.0 commit `e77f175595e64cb44218cc9d9d56fc365ad0e36a`, CPU/static,
  double positions and deterministic/common-SIMD profile. Probe preprocessor
  guards reject unexpected AVX/FMA/F16C/LZCNT/TZCNT configuration.

Each archive SHA-512 is checked by vcpkg. No baseline/unrelated dependency
version changes. Jolt/Lua ports build their libraries without applying the
engine warning policy to third-party source; **our** probe targets always use
the complete engine warning set with `-Werror` or `/WX`, even in local builds.

```bash
cmake --preset ci-linux-gcc14
cmake --build --preset ci-linux-gcc14 --target engine_m4_admission engine_m4_jolt_admission
ctest --test-dir build/ci-linux-gcc14 -R '^engine_m4_(lua|jolt)_admission$' --output-on-failure -V
```

GCC14, Clang18 and MSVC reference builds are the admission gate. Local GCC16
and Clang23 currently reject an upstream `sol::optional<T&>::emplace` body that
calls nonexistent `construct`. Do not suppress the diagnostic, patch the
library or substitute a new release to claim the approved gate passed. The
reference CI determines whether the owner-selected matrix accepts this source.
Any required library/profile change is an owner decision.

The Lua probe checks protected errors, C++ destruction on Lua-originated errors,
bound types, syntax/argument failures, instruction exhaustion, allocator failure,
per-environment library/global isolation and allocation cleanup. This is an
admission prototype, not proof of a complete production sandbox, path loader,
stale-ECS-handle policy or per-script allocation attribution in a shared VM.
Those runtime contracts are future M4 work. The Jolt executable runs independently
so a Lua compiler blocker does not prevent profile/rigid-body smoke evidence.

Admission requires the libraries themselves to match each host compiler, not
only the probe translation units. Linux CI sets explicit CC/CXX for GCC14 and
Clang18; Windows uses the selected MSVC toolchain. Both overlay packages retain
generated `compile_commands.json`, CMake's actual compiler path/ID/version and
the installed archive SHA-256 for Debug/Release under
`share/<port>/build-provenance/`. This evidence travels with binary-cache hits.
The capture script reads installed-package provenance, checks archive hashes,
requires library/host compiler identity/version equality, and audits every
library command for C++/exceptions/FP and Jolt numeric settings. Missing evidence
or a mixed compiler fails admission. Lua explicitly uses C++20, exceptions and
`-ffp-contract=off` (MSVC `/EHsc /fp:strict`). Jolt retains upstream deterministic
FP controls and explicit RTTI/exception settings. Do not infer archive compilers
from host macros or an unavailable source-build directory.

The first reference run passed GCC14/MSVC, but Clang18 rejected sol2's direct
member-variable trampoline. The candidate now uses typed `sol::property`
accessors without changing script field semantics, argument checks or pinned
headers. This is an explicit bridge limitation: direct member-pointer binding
is not admitted. All original error/isolation/field assertions remain required.
Jolt RTTI/exceptions now explicitly match the C++ host: upstream's private
no-RTTI default caused missing typeinfo when UBSan instrumented host-side virtual
operations. Keep vptr checks enabled; fix the library/consumer mismatch instead.
The configuration string is the consumer header view; generated library flags
must independently show `-frtti -fexceptions -ffp-contract=off` (or MSVC equivalents).

## Controlled 1,000-instance comparison

Both models initialize the same libraries, bound type, independent allowlisted
environment and script payload. Library/constructor tables are copied for each
environment; scripts do not share mutable API tables. Each script's private
state is positively checked after initialization. The shared model registers
types/libraries once per session; the per-instance model does so in every VM.

Build `engine_m4_admission` optimized, without sanitizers, then on the reference
machine (no concurrent build, AC and fixed power profile):

```bash
.venv/bin/python3 scripts/measure_m4_vms.py /path/to/optimized/engine_m4_admission --runs 5
```

The CI GCC14 job retains a Release executable so it can be measured locally
without silently changing the agreed compiler to work around local newer-
compiler incompatibilities. Verify its source/run and SHA before using it.
Each sample uses a fresh process, exactly 1,000 instances, and alternating order.
Record allocator-retained/peak requested Lua bytes and whole-child peak RSS
(Linux), plus startup time measured inside the program. RSS includes the entire
process; allocator bytes exclude native host allocations and malloc overhead.
The diagnostic does not enforce the aggregate64MiB runtime budget merely to
truncate the requested workload; it reports whether the model would exceed it.
Instance caps remain bounded. No runtime budget or VM decision is changed by
collecting these measurements.

Report cold processes with warm OS file cache honestly. A shared VM reduces
per-state duplication but separate environments are not separate heaps, and a
shared allocator does not itself enforce per-script8MiB memory attribution.
Evaluate those isolation/accounting tradeoffs with the numbers before asking
the owner to accept ADR-0016.
