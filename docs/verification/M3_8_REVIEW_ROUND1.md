# M3.8 independent technical review — round 1

- Reviewer: Claude Opus 5.5, high effort (owner-selected).
- Source: `014ba2045b378f896181d28e0f6a5eb4a0163226`; base `afea88a`.
- Verdict: **Changes required**, one Medium finding. Not owner approval.
- The reviewer restored all mutations and returned a clean isolated checkout.
  No source was published before this handback.

## Required F1: incomplete TSan selection

The initial documented nine-test selection dropped `engine_cli_contract` and
`engine_render_tests`, which earlier milestones ran, and omitted the production
socket deadline and durable journal tests. The evidence's description of that
selection as the existing complete CPU scope was incorrect. The reviewer built
TSan independently: the nine tests passed, and the four omitted tests passed
separately. The remaining rendering tests entered uninstrumented lavapipe races;
that does not justify dropping the four viable contracts/device tests.

Required correction: build **all** TSan targets, add `cli_contract`,
`render_tests`, `live_journal` and `local_socket_deadline` to the selection, run
13 tests and correct the documentation/evidence. A re-review is required.
The original nine-test evidence remains labeled historical, not complete
acceptance; the correction run is captured separately.

## Optional findings handled in the correction

- L1: restore the normal short fuzz job's 20-minute timeout; use 55 only for
  scheduled/manual extended mode.
- L2: explicitly state that cached corpora are not minimized, record a measured
  local size and storage planning example, and distinguish that from a bound.
- L3: report clean-clone CTest skips. The correction additionally rejects any
  skipped CTest test or empty report instead of declaring complete acceptance.

## Independent verification

- Fresh Clang Werror full suite: **27/27**, no skips; repeated after restoring
  mutations. Public Blender golden: max channel difference1, changed0.
- ASan/UBSan retention preset: **26/26**, `detect_leaks=1`, no `LSAN_OPTIONS`.
- Original TSan selection9/9 and four omitted tests4/4 separately.
- Direct invalid filesystem source, invalid supplied asset name and valid
  symlink alias to invalid source: CLI returns E_VALIDATION without changes.
  Live uses the same result and persists a started/completed error pair.
- Removing the import text preflight fails both unit and CLI regression tests.
- Reimport mutation assigning fresh IDs fails the new acceptance's stable-ID
  assertion. The existing golden/camera/light/tolerance are reused correctly.
- Fuzz helper rejects 0,601,0600,-1,1e3,abc,9999 (exit2). Missing binaries do not
  prevent later targets and yield exit1. An injected image-harness null write
  produces a crash artifact, preserves logs, lets RPC run and returns exit1.
  The harness was restored/rebuilt afterward.
- Workflow guard/concurrency/key/provenance and clean-clone SHA/cleanup checks
  hold. Registered counts27/26/20 match the platform/sanitizer conditions.
- Goldens, schemas, renderer, third-party sources and render tests are unchanged.

The reimport text preflight is defense in depth: public sidecar/GLB parsing
already rejects invalid text. Removing only that guard leaves27/27 passing;
no claim is made that a test specifically exercises it.

## Remaining gates at handback

F1 correction and re-review, six-job reference CI (including Windows20/20),
manual extended fuzz with actual cache save/restore, and public exact-SHA clean
clone. The reviewer did not rerun GCC, formatter/tidy18, short/600-second fuzzing
or GitHub cache operations. Nightly activation awaits default-branch merge;
physical Windows presentation and power-loss behavior remain unverified.
