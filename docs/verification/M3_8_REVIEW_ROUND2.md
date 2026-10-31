# M3.8 correction re-review — round 2

- Reviewer: Claude Opus 5.5, high effort, owner-selected.
- Source: `341dc3c239913c333cb71a46793765a299652c80` against `014ba20`, base `afea88a`.
- Verdict: **Technically approved, no required findings.** Not owner approval.
- Independent checkout was restored/clean and nothing was published.

## Required and optional corrections

F1 is fixed. A fresh TSan full build (102 steps, no target subset, warnings as
errors) completed; a second build had no work left. With a venv containing the
pinned requirements, the exact documented 13-test selection passed on lavapipe
and again on the default Intel environment. JUnit showed 13 tests, zero skipped,
and no ThreadSanitizer reports. Direct device tests passed 105 assertions in
13 cases without internal skips. The initial run using system Python failed
five contracts because jsonschema was missing; this was corrected as an
environment prerequisite, not a source change.

L1's ordinary-20/extended-55-minute timeout expression is consistent with the
per-target duration switch. L2's no-minimization and full-copy storage example
is accurate and explicitly not a cap. L3's JUnit gate rejects actual CTest
return-code and output-pattern skips and empty selections; passing reports pass.
A no-display repository run where CTest itself exited 0 with two window tests
skipped was correctly rejected by the checker, with both test names.

A full local file:// clone without a display stopped on the existing
asset-placement window test (No available video device); cleanup worked. That
is not a passing public clone, and the reviewer did not claim one.

## Minor non-blocking notes

- N1: align older STATUS TSan wording with the 13-test scope.
- N2: separate glued numbers/words in the new STATUS/review text.
- N3: CTest DISABLED cases have status="disabled" and no skipped child; the
  original checker would count them as executed. No current test is disabled.

The parent handles these small notes directly. N3 is strengthened to reject
skipped/disabled/notrun reports and checked against actual CTest outputs.

## Limits and later finding

The reviewer did not rerun GCC, formatter/tidy18, fuzzing, Windows or GitHub
cache operations. Six-job reference CI, manual extended cache save/restore and
public exact-SHA clone remain required gates. Scheduled activation requires
merge; physical Windows presentation and power-loss behavior remain unverified.

After this handback the parent's long fuzz run produced a MikkTSpace welding
input timeout under concurrent build load. It is captured separately, not
covered by this source approval. The resulting callback change/regression
requires its own technical review before publication. No claim is made that
the original long fuzz run was green.
