# M4.1 VM topology comparison — decision requested, not adopted

The latest owner request holds ADR-0016 until the default-local-compiler binding
decision is resolved. See the [local options report](M4_1_LOCAL_COMPILER_OPTIONS.md).
Concrete schemas/parser bounds are accepted, but M4.2 has not started.

Source measured: `982b3f8`, workflow
[41192173709](https://github.com/chl84/3D_game_engine/actions/runs/41192173709),
artifact `m4-vm-reference-release` (13040060145), GCC14.2.0 Release. Exact binary
SHA-256, archive digest, all ten samples and machine conditions are in
[M4_1_VM_MEASUREMENTS.json](M4_1_VM_MEASUREMENTS.json).
GCC14's installed Lua/Jolt archive provenance verifies the actual compiler,
C++/exception/FP settings and archive hashes in both configurations. Subsequent
Windows-only Jolt platform selection, compile-time SSE guard and CI exit handling
do not change this GCC14 Lua binary or benchmark semantics. No remeasurement is
required merely because Windows/Jolt admission evidence is corrected.
The final30a130b Release artifact13040019080 was independently hashed and is
byte-identical to the measured binary (SHA-256 in the raw measurement record).

## Method and actual results

Run on the reference i7-1265U, AC connected, fixed balanced platform profile;
Linux MemTotal records30.777GiB usable RAM, differing from the original16GB
project reference specification. No concurrent build. Linux x64, warm OS
file cache, five fresh processes per model in alternating order. Each process
retains exactly1,000 independent instances, then positively validates each one's
state. Both use the same allowlist, copied library tables, bound type and script.

```bash
.venv/bin/python3 scripts/measure_m4_vms.py \
  build/m4-evidence-982b3f8/release/engine_m4_admission --runs 5
```

| Model, 1,000 instances | Median startup | Retained Lua allocator bytes | Max peak Lua bytes | Whole-child peak RSS, median |
|---|---:|---:|---:|---:|
| VM per instance | 53.32ms | 23,298,000 (22.22MiB) | 23,298,603 | 31,440KiB (30.70MiB) |
| Session VM, private environments | 18.01ms | 2,965,088 (2.83MiB) | 2,965,176 | 18,096KiB (17.67MiB) |

Startup covers VM creation, library/type registration, environment creation and
script compilation/initialization. It excludes payload-update validation,
cleanup and output; this is a startup comparison, not per-tick throughput.
Allocator bytes exclude native C++ objects, malloc overhead and non-Lua memory.
RSS is the Linux launched child's high-water mark, including exec/validation/
cleanup; Python launch can impose a baseline. Do not interpret the RSS difference
as exact isolated VM overhead. Lua allocator bytes are the primary comparison.
The aggregate64MiB budget is diagnostic-only in this workload, not used to
truncate1,000 instances. Neither model exceeds it with this small payload.
This is not acceptance of1,000 arbitrarily large gameplay scripts under64MiB.

## Recommendation for owner decision

The owner has since held ADR-0016/topology pending default-local-compiler
compatibility. The [binding alternatives](M4_1_LOCAL_COMPILER_OPTIONS.md) must be
resolved before this topology recommendation can be accepted. No M4.2 work has
started. The original measurements below remain3.3.0 data, not a formal benchmark
of the proposed newer source; refresh that formal benchmark after a binding
remedy is approved and admitted.

Recommend **one VM per instance for M4**, subject to owner approval. This measured
payload fits the approved aggregate budget; roughly53ms to initialize1,000 small
instances is a bounded loading cost. The main reason is enforceable isolation:
each counting allocator can enforce its own8MiB cap while a shared aggregate
counter enforces64MiB. VM teardown also gives a clear instance/reload lifetime.
It remains necessary to test real gameplay payloads in M4.6–M4.10.

The shared model uses roughly7.9times less retained Lua allocation and starts
roughly3.0times faster here. It is a reasonable later scaling option, but separate
environments do not create separate heaps. This prototype does **not** provide
reliable per-script8MiB attribution for shared objects, interned values or GC.
Adopting it while retaining the approved hard per-instance limit needs a proven
accounting/ownership design; relaxing that limit is a separate owner decision.
No resource budget is silently weakened by this recommendation.

Neither topology is an OS security boundary. Both still require protected calls,
host operation bounds, instruction hooks, source sandboxing and stale-handle
validation. The binding prototype alone does not prove those production APIs.

ADR-0016 remains partially approved. No VM topology has been selected in engine
runtime code or made the schema default. Independent review and final Quality/
CI passed on the reviewed implementation30a130b; see
[Astra's disposition](M4_1_ASTRA_REVIEW.md) and
[reference CI evidence](M4_1_REFERENCE_CI_EVIDENCE.txt). Final publication checks
are attached to PR6. The owner topology decision remains outstanding.

## Per-instance summary, including setup and bindings

[Per-instance cost/capacity chart](M4_1_VM_PER_INSTANCE.html).

The original reference measurement retained1,000 instances of each model. Divide
those recorded totals by1,000; these are **amortized averages**, not an isolated
single-script latency. Shared-session numbers amortize the one VM/library/type
registration across1,000 private environments. Per-instance VMs repeat that setup
1,000 times. Both include type bindings, environment/library table setup, script
compilation and initialization. Native vector reservation occurs before timing.

| Model | Lua bytes per instance | Lua KiB per instance | Startup per instance |
|---|---:|---:|---:|
| One VM per instance | 23,298 | 22.752 | 53.317 microseconds |
| Session VM + private environments | 2,965.088 | 2.896 | 18.013 microseconds |

Lua bytes include Lua-side binding/userdata/registry state and script payload.
They exclude C++ binding metadata allocated outside Lua, native host structures,
malloc overhead and code pages. The approved64MiB limit is **aggregate requested
Lua allocator bytes**, not a total-process/RSS limit. Native memory remains an
additional cost; do not call the Lua column the entire VM memory footprint.

A supplemental fresh-process RSS snapshot includes the complete initialized
native/Lua/binding setup. It uses a GCC16 diagnostic copy with the unadopted narrow
3.3.0 backport, five processes/model, measured before Lua and while all1,000
instances remain live, rather than Python-launch-influenced child high-water RSS:

| Model | Median initialized process RSS | Median RSS increase from before Lua | Amortized RSS increase/instance |
|---|---:|---:|---:|
| One VM per instance | 32,473,088 bytes | 28,307,456 bytes | 28,307.456 bytes (27.644KiB) |
| Session VM + private environments | 8,036,352 bytes | 3,842,048 bytes | 3,842.048 bytes (3.752KiB) |

Shared-run GC timing also varies the outstanding Lua allocation in an individual
sample; the reference and supplementary medians must not be read as a fixed
per-script size guarantee.

RSS is page-granular and includes allocator/code-page effects, not ownership-
attributed heap bytes. RSS already includes Lua; never add the RSS and Lua
columns together. The rows are medians of separately observed quantities;
do not subtract different medians to recreate the median delta. These are local
diagnostic snapshots, not replacement reference timing samples. Linux MemTotal
in the original measurement is30.777GiB usable RAM; the original16GB project
specification is unchanged. [Raw capacity/RSS evidence](M4_1_VM_CAPACITY_DIAGNOSTIC.json).

## How many fit in64MiB of Lua allocations?

For this exact small payload, the observed initialization-plus-validation peaks
give the following conservative, peak-safe planning counts in a controlled
extension of the3.3.0 diagnostic. These are not absolute hard-quota maxima:

| Model | Largest observed normal-GC peak-safe count | Peak at fitting count | Normal-GC peak at next count |
|---|---:|---:|---:|
| One VM per instance | **2,880** | 67,098,843 bytes | 67,122,141 bytes at2,881 |
| Session VM + private environments | **21,726** | 67,105,478 bytes | 67,109,054 bytes at21,727 |

The budget is67,108,864 bytes. Fresh-process binary search checked both fitting
and next-over-budget neighbors, followed by two independent rechecks per boundary.
Every successful construction also positively validates each script's independent
state/update and allocation cleanup. The one-shot1,000-instance capacity baseline matches both original GCC14
reference medians exactly. This does not claim every supplemental RSS sample
has identical Lua bytes: one shared RSS sample records3,081,753 bytes, while
the other four record2,965,088; reported medians are unchanged.

The extension only widens the CLI count guard in a scratch copy. Its shared
diagnostic allocator ceiling is256MiB so it can observe an over-budget neighbor
without triggering OOM; this is **not** a production quota change. It classifies
against the unchanged64MiB criterion using actual allocator peaks, including
bindings and validation, not merely retained bytes. No diagnostic timing from
the capacity search is used. [Exact scratch-source delta](M4_1_CAPACITY_DIAGNOSTIC.patch)
and [unadopted compile fix](M4_1_SOL2_OPTIONAL_BACKPORT.patch) are retained for review.

The naive shared estimate `floor(64MiB / (2,965,088/1,000)) =22,633` is too high:
registry/table growth and GC make memory nonlinear. It must not replace the
observed normal-GC boundary. These capacities apply only to the measured payload/backend,
not arbitrary game scripts or a future newer binding source. If every separate
VM consumes its full approved8MiB cap, at most **eight** such heaps fit the64MiB
aggregate budget. Shared environments still lack proven8MiB per-script ownership
accounting, regardless of their better small-payload capacity.

### Hard allocator ceiling and setup failure limitation

A second isolated variant restores the original64MiB shared allocator ceiling;
[raw stress controls](M4_1_VM_HARD64_DIAGNOSTIC.json) and
[exact extra delta](M4_1_CAPACITY_HARD64_DIAGNOSTIC.patch) are retained separately.
Lua can run emergency GC after allocator denial, so a count whose ordinary
initialization peak crosses64MiB may still initialize successfully under the
hard ceiling. A21,727-instance shared run did succeed and remained within the
enforced limit. The retained controls also admit24,000 shared instances, with
26,000 failing. No stable exact hard-quota maximum is established; these bounded
stress controls must not become a production capacity guarantee.

At32,768 instances, quota exhaustion triggered Lua panic and a disposable worker
terminated with SIGABRT and a secondary panic message. That failure path was not
debugged into a production recovery mechanism.
Core dumps were disabled for these intentional stress workers. This is concrete
evidence that the prototype's unprotected VM/binding setup is not production-
hardened for OOM; it is not a graceful session-error or recovery result. Do not
infer that the newer proposed binding source was stress-tested by this backport
trial. Protect/fault-inject all setup/cleanup paths before production adoption.
Keep the conservative planning counts separate from emergency-GC capacity and
the unresolved production OOM handling.
