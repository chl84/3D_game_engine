# M4.1 local compiler/capacity report — independent review

Reviewer: **GPT-6 Astra high**, selected by the owner in REVIEWER.json.
Reviewed exact local commit `bcafd4087576d7a36609515687bb646991fc6f0a`.
Verdict: **APPROVE for REPORT only. No required findings.**

The reviewer confirmed the evidence supports the current compiler failures,
newer-source feasibility and stated dependency/matrix limitations. Memory units,
amortization, RSS medians and capacity-neighbor arithmetic are correct. Normal-GC
peak observations are separated from hard-quota emergency GC and unresolved
setup OOM behavior. ADR0019/20 preserve the owner's conditions without adopting
ADR0016 or authorizing M4.2.

Optional observations were addressed in this publication:

- STATUS's current-substep summary now reflects accepted schemas/parser bounds
  and unresolved local binding/topology decisions.
- The exact1,000-instance match now explicitly refers to the capacity baseline
  and reference medians. One supplemental shared RSS sample retains3,081,753 Lua
  bytes; the other four retain2,965,088. The reported medians remain unchanged.
- Retained baseline logs had trailing spaces; these were removed. Diagnostic
  diffs are regenerated without context to avoid whitespace-only warnings from
  unified-diff context lines. Their source transformations are unchanged and
  were reapplied to scratch copies and compared byte-for-byte. Apply these
  zero-context proposal/diagnostic diffs with `git apply --unidiff-zero` against
  their documented exact base copies, never directly to production sources.

Checks independently performed:

- Read project rules/configuration, relevant ADRs, changed documentation, logs,
  JSON evidence, HTML source and diagnostic diffs.
- Confirmed no implementation changes outside documentation relative to30a130b.
- Verified both full candidate logs contain all110 build steps and3/3 focused
  passes; inspected actual candidate compile commands.
- Matched the candidate archive SHA-512 and both capacity-binary SHA-256 hashes.
- Applied all three original diffs in memory and matched retained scratch source.
- Recalculated RSS medians, unit conversions, capacity neighbors and repeats.
- Ran proposal validation: five schemas, seven examples,15 invalid controls and
  12 compatibility controls passed. No substantive whitespace defect identified.

No fresh builds, benchmarks, stress workers, remote checks or publications were
performed by the reviewer. The post-review edits only clarify scope/presentation
and record this verdict. Publication-head CI remains a separate gate.

This verdict does not approve a dependency upgrade or topology, accept ADR0016,
close M4.1 or authorize M4.2. The current3.3.0 pin remains locally incompatible;
the owner still selects the remedy before adoption and fresh admission.
