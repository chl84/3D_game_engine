# M4.1 reference admission report — 2026-10-31

Historical reference report. The owner subsequently approved schemas/parser
bounds and added a mandatory local compiler gate. See the
[local binding follow-up](M4_1_LOCAL_COMPILER_OPTIONS.md) and updated VM report;
M4.2 remains held. The newer candidate is not covered by this earlier verdict.

Delivered isolated pinned dependency/profile probes, a measured1,000-instance
comparison, concrete owner-review schemas and independent Astra high re-review.
Reviewed implementation: `30a130bb6ae5eca02477ea66dc475123e38b4264`.
M4.1 remains open for the owner decisions below; M4.2 has not started.

| Feature | Status | Evidence |
|---|---|---|
| sol2 release3.3.0 / Lua5.4.9 as C++, protected errors/unwinding/OOM and bridge warnings | Automatically tested | [CI](M4_1_REFERENCE_CI_EVIDENCE.txt), matching GCC14/Clang18/MSVC library/host identities |
| Jolt5.6.0 deterministic double/SSE profile and primitive smoke trace | Automatically tested | [Raw reports](M4_1_REFERENCE_PROBES.json), all240 values match; full command sets retained in gzip |
| Required regressions, sanitizers, quality and short fuzz | Automatically tested | Six green jobs on reviewed SHA; GCC/Clang30/30, ASan29/29, MSVC23/23 |
| Two VM models,1,000 instances each | Measured | [Report](M4_1_VM_COMPARISON.md), [raw samples](M4_1_VM_MEASUREMENTS.json), [chart](M4_1_VM_COMPARISON.html) |
| Concrete project2/scene4/profile/replay envelopes | Proposed, envelope checks tested | [Proposal](../proposals/m4/README.md); five schemas, seven examples,15 negative and12 compatibility controls |
| Independent review and initial P2 corrections | Technically approved | [Astra re-review](M4_1_ASTRA_REVIEW.md), exact30a130b; both P2 findings closed |

Not verified/implemented: production gameplay VM host, stale-ECS handles,
full sandbox/path/reload APIs, complete M4.4 physics determinism suite, migrations
or format readers/writers. Windows compilation/headless/offscreen passed;
physical Windows presentation and manual gameplay were not tested here.
VM/library/binding-setup OOM injection remains before production adoption.

Known local deviations: upstream sol2's optional template rejects local
GCC16/Clang23, and tidy18 cannot parse GCC16 standard headers. The approved
reference compiler matrix passed with unpatched pinned sources; direct sol2
member-pointer binding is excluded in favor of typed property accessors.
Current installed RAM is about32GiB rather than the original16GB reference
specification; this is not whole-engine/M5/M8 memory-budget acceptance.

No plan scope or resource quota was silently changed. Animation remains tracked
as a separate proposed M4A plan, and M4.8 retains its installed-SDK goal and
owner-approved packaging stop/proposal checkpoint.

Owner decisions needed:

1. Accept per-instance VMs for M4 (recommended: enforceable8MiB instance heaps
   and64MiB aggregate limit), or select another topology after reviewing measured
   costs and the shared-heap accounting limitation. ADR-0016 is not yet accepted.
2. Approve the concrete schemas and migration/replay contracts in ADR-0019.
3. Explicitly approve the64MiB document/depth32 parser compatibility exception:
   reject oversized/deep legacy documents without modifying/truncating them.

Next step: after these owner decisions and green publication-head checks on
draft PR6, record accepted decisions and start M4.2. This report grants no
M4 merge/tag authorization and does not adopt a VM topology or new formats.
