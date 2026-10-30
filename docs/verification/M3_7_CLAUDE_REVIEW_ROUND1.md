# Mandatory Claude Opus re-review — source76ecc7d

Reviewer: Claude Opus5.5 high, independently requested by the owner.
Technical verdict: changes required (minor); no owner approval.
Reviewed76ecc7d againstd1bb955 plus the three corrections from6e3fb73.

## Earlier corrections independently verified

All three hold: MSVC std::array const_iterator, clang-format18 compatibility,
and queued receive deadlines after a preceding slow handler. The reviewer
mutated each deadline check alone (the other backs it up) and both together
(the regression fails with expired pending frame dispatched). The clock and
accept/send deadline handling are correct. An already executed command can lose
its reply if another handler blocks beyond the send deadline; rollback is not
claimed. The reviewer independently confirmed the old formatter failure and
current first-party C++ formatting success with clang-format18.1.8.

## Journal findings

- F1/P2: size accounting calls JSON dump outside the append guard. An invalid
  UTF-8 filesystem name behind a valid UTF-8 symlink alias can produce an
  unserializable error result. The old route returned Invalid params and kept
  dispatching instead of E_IO and stopping. Mandatory correction and re-check.
- F2/P3: mode0400 fails at open, not the permission check. A0644 probe catches
  removal of the0600 guard; strengthen the permanent test accordingly.
- F3/P3: removing fsync or ignoring completed-append failure survived CTest,
  although the reviewer's manual fault harness caught both. Register Linux
  non-ASan fault injection with both pre/post-execution fsync failures.
- F4/P3: journal failures exited0 with no stderr, making notification failures
  invisible. Return E_IO exit6 and one credential-free diagnostic line.

The reviewer confirmed sync ordering, actual CommandLog completeness, POSIX
link/ownership checks, fail-closed ordinary append failures, session retention
and no automatic credential logging. Windows implementation was checked by
source inspection; Windows runtime evidence came from CI only.

## Independent verification

Fresh Clang warnings-as-errors25/25 on lavapipe; core560assertions/69cases.
ASan/UBSan25/25, leak checks on, no LSAN_OPTIONS or suppression file.
Deadline and journal mutations were restored; isolated worktree clean.
Manual fsync fault checks confirmed before-intent execution prevention and
after-execution E_IO with the scene side effect and journal retained.
CI41105632792 exactly76ecc7d: GCC/Clang/ASan25/25; Windows19/19 (including
journal/deadline); Fuzz success. Quality was still running at the reviewer's
single snapshot. No physical Windows/power-loss verification claimed.

## Parent corrections (new-source verification pending)

F1: guard all dispatch, size accounting, completion serialization and reply
construction; serialization failures return the journal E_IO and stop. Linux
regressions use an invalid UTF-8 directory behind a UTF-8 alias and an existing
directory scene target for both save/load, requiring exit6 and only started.
F2: mode0644 owner-writable fixture exercises the actual permission guard.
F3: permanent engine_live_journal_fault CTest interposes real fsync in Linux
non-sanitized builds. Started/completed faults, notification diagnostics and
a no-fault control are asserted; no existing sanitizer test is disabled.
F4: journal_failed tracks terminal state; after draining, stderr emits one
constant credential-free line and the process exits6.

The F1 correction and associated fixes require another Claude Opus re-review
and fresh exact-source CI before final reporting/owner assessment. M3.7 is
not approved; merge/tag/M3.8 remain unauthorized.
