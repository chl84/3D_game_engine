# M4.1 VM topology comparison — decision requested, not adopted

Source measured: `036b41b`, workflow
[41189740843](https://github.com/chl84/3D_game_engine/actions/runs/41189740843),
artifact `m4-vm-reference-release` (13038850660), GCC14.2.0 Release. Exact binary
SHA-256, archive digest, all ten samples and machine conditions are in
[M4_1_VM_MEASUREMENTS.json](M4_1_VM_MEASUREMENTS.json).
The subsequent clang-tidy correction changes integer expression types and an
unused parameter comment, not the benchmark payload, limits or algorithms.

## Method and actual results

Run on the reference i7-1265U, AC connected, fixed balanced platform profile;
the current machine reports about32GiB installed RAM, differing from the original
16GB project reference specification. No concurrent build. Linux x64, warm OS
file cache, five fresh processes per model in alternating order. Each process
retains exactly1,000 independent instances, then positively validates each one's
state. Both use the same allowlist, copied library tables, bound type and script.

```bash
.venv/bin/python3 scripts/measure_m4_vms.py \
  build/m4-evidence-036b41b/release/engine_m4_admission --runs 5
```

| Model, 1,000 instances | Median startup | Retained Lua allocator bytes | Max peak Lua bytes | Whole-child peak RSS, median |
|---|---:|---:|---:|---:|
| VM per instance | 49.66ms | 23,298,000 (22.22MiB) | 23,298,603 | 31,512KiB (30.77MiB) |
| Session VM, private environments | 19.51ms | 2,965,088 (2.83MiB) | 2,965,176 | 18,304KiB (17.88MiB) |

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

Recommend **one VM per instance for M4**, subject to owner approval. This measured
payload fits the approved aggregate budget; roughly50ms to initialize1,000 small
instances is a bounded loading cost. The main reason is enforceable isolation:
each counting allocator can enforce its own8MiB cap while a shared aggregate
counter enforces64MiB. VM teardown also gives a clear instance/reload lifetime.
It remains necessary to test real gameplay payloads in M4.6–M4.10.

The shared model uses roughly7.9times less retained Lua allocation and starts
roughly2.5times faster here. It is a reasonable later scaling option, but separate
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
CI checks remain separate gates before the consolidated M4.1 report.
