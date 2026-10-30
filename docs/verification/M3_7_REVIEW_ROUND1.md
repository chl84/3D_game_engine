# M3.7 independent review round1 and correction record

Reviewer: GPT-6 Astra, reasoningEffort=high, as selected by the owner.
Reviewed source:6e3fb7343944637e054212d946f1ef7d7fa4d80e against16c5805.
Technical verdict on that source: **Changes required**. No owner approval,
merge, tag or M3.8 authorization is implied.

The independent reviewer reported three confirmed findings:

1. **P1: MSVC checked iterator.** live_call declared the result of ranges::find_if
   as a pointer. MSVC's debug std::array iterator is a class; Windows compilation
   failed. Parent correction uses the explicit std::array const_iterator type,
   which is portable and also satisfies local qualified-auto linting.
2. **P2: queued receive deadline after a slow handler.** The transport expired
   clients only at the outer-loop boundary. An earlier synchronous handler could
   consume a later ready client's deadline before it was processed. The reviewer
   reproduced a150ms deadline and400ms handler dispatching an expired frame.
   Parent correction rechecks time before every client's receive/send processing
   and again immediately before dispatch. An already accepted handler may still
   finish; this does not extend another client's receive deadline.
3. **P2: reference formatter.** clang-format18 rejected two stat declarations
   accepted by local version23. Parent correction uses equivalent explicit empty
   initialization accepted by both versions. No formatter rule was relaxed.

The reviewer independently built Clang with warnings as errors and ran23/23 on
lavapipe; core546assertions/68cases and asset22,737/19. Raw/authenticated parser
fuzzing passed67,434executions in31s with ASan/UBSan/leak checks. Auth and parser
size/depth mutations were caught; source was restored. The reviewer confirmed
shared command handlers/rendering, loopback-only binding, generic auth denial,
sandboxed live paths, exclusive project locking, bounded logging and CI fuzz
integration. Windows runtime/private-file behavior was unverified because the
reviewed source did not compile. The reviewer did not independently run the full
engine sanitizer suite or physical Windows presentation.

CI inspected by the reviewer:
[41091300737](https://github.com/chl84/3D_game_engine/actions/runs/41091300737),
exact6e3fb73. Clang/GCC and Fuzz passed; Windows and Quality failed; ASan was
still running at that snapshot. These results are historical, not final proof.

## Parent closure verification

The new engine_local_socket_deadline regression uses the production transport
in a separate process. stdin gates arrange two ready clients in the same select
snapshot; an accepted slow handler blocks beyond the other client's absolute
receive deadline. The pending frame is then completed and must close without
dispatch, while the accepted handler completes and shutdown remains available.
This runs on both Linux and Windows, without process signals or server threads.
A mutation removing the new deadline checks must fail; restored code must pass.
Commands and captured results are in M3_7_LOCAL_EVIDENCE.txt.

Final correction CI and parent closure remain pending until recorded in the
status report. The original verdict is preserved; no second independent verdict
is claimed. Small findings are corrected by the parent with discriminating tests
and reference CI, without an unnecessary new delegated review.
