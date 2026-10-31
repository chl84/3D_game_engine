# M4.1 initial independent review and correction tracking

Reviewer: GPT-6 Astra high, owner-selected configuration in REVIEWER.json.
Reviewed exact SHA `ee920649a64b4b77d7dbae2cd0ad0e65713eb5d8`, full delta
from released M3 baseline `88ccc85`. Verdict: two required P2 corrections.
This is not M4.1 or owner approval.

1. New name/path/count bounds contradicted the migration compatibility promise.
   Correction candidate preserves unrestricted legacy strings and scene counts,
   duplicate scene-list entries and existing stored transform values. Proposal
   validator adds positive legacy compatibility cases. Newly proposed document
   byte/depth limits are now an explicit, owner-pending compatibility exception:
   bounded structured rejection, no automatic rewriting/truncation.
2. Library build audit lacked compiler identity and cached Lua commands; equal
   host traces did not prove separate GCC/Clang library builds. Correction
   candidate packages actual generated compiler metadata/commands and archive
   digests for both configurations with each cached Lua/Jolt library. CI selects
   CC/CXX explicitly; capture verifies archive digests, compiler identity/version
   equality with the host and effective C++/exception/FP/numeric flags. Lua's FP
   contraction/exception policy is explicit. Corrected reference CI is required.

Reviewer independently checked proposal validation, invalid-name reproduction,
the reference binary's digest and protected Lua admission, all measurement
summaries and the submitted240-tick primitive traces. No further concrete blocker
was found in the examined RAII/unwinding/instruction/environment paths. Benchmark
method and bounded recommendation were considered sound. Optional follow-up:
inject OOM at VM creation/library/binding setup before production adoption.
Production runtime/stale-ECS-handle APIs and the full M4.4 physics suite remain
outside these isolated probes. No fresh full build/sanitizer/fuzz run was claimed.

Previous CI41189740843/[raw probe reports](M4_1_PRE_AUDIT_REFERENCE_PROBES.json) remain historical evidence. Their
missing/mixed library compiler provenance is not relabelled as complete. New
reference artifacts and review disposition must resolve the two items before
the consolidated report. No topology or concrete format is accepted yet.

Both findings were subsequently closed by the independent
[re-review of exact30a130b](M4_1_ASTRA_REVIEW.md).
