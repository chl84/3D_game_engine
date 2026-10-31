# ADR-0016: Lua bindings and VM boundary

- **Status:** Partially approved; final acceptance pending M4.1 compiler probes and owner VM-topology decision
- **Date:** 2026-10-31
- **Scope:** [M4 proposal](../M4_PLAN.md); extends ADR-0005/0007 without changing
  the engine's Result/error contract.

## Context

Lua 5.4 and sandboxed, reflection-generated game bindings are selected in the
vision. A binding library has not been adopted. Entity access must stay valid
across deletion/reload, and Lua errors must not bypass C++ resource cleanup or
escape engine module boundaries. Convenience bindings do not themselves provide
a sandbox, deterministic state, memory limits or a gameplay command policy.

## Proposed decision

Use **sol2 3.3.0** privately inside `engine/script`, with **Lua 5.4.9 compiled
as C++**, statically linked. Both use MIT licenses. Keep Lua 5.4 despite newer
Lua releases because that major/minor is the owner-selected technology stack.
Pin immutable source digests through the existing vcpkg manifest/overlay model;
do not update unrelated baseline dependencies. Adoption waits for approval and
GCC14/Clang18/MSVC compatibility/unwinding/OOM probes in M4.1.

Enable `SOL_ALL_SAFETIES_ON` and the documented `SOL_USING_CXX_LUA` configuration.
Use protected load/call APIs and install explicit error/trace handling. Confine
Lua's internal exception unwinding to the script implementation, translate
errors to existing `Result`/`Error` at public boundaries, and never throw an
arbitrary host exception through a raw Lua C callback. Expected host failures
return structured results. An unrecoverable VM panic is terminal for that
session; do not catch it and pretend the VM can safely continue. Verify concrete
macro behavior against the pinned headers rather than copying old tutorials.

VM topology is **not approved**. M4.1 must measure 1,000 script instances with
(a) a `lua_State` per instance and (b) one session state with separate sandbox
environments, then present memory/startup numbers and a recommendation to the
owner. No topology is adopted before that decision. The comparison must explain
which memory attribution/isolation guarantees each topology can enforce.
The resource quotas remain approved starting points, subject to this decision. Provide a counting
allocator and instruction hook before user code. Expose only an allowlist of
base/table/string/numeric facilities; remove free load/loadfile/dofile, package,
io, os, debug and raw coroutine creation. Host-controlled text-only module
loading is project-relative through PathSandbox; bytecode is rejected. Host
calls have their own bounded work contracts because Lua hooks do not budget
arbitrary native work. The VM is not an OS security boundary.
The library allowlist also excludes `string.dump`, unrestricted pattern matching
and native library operations whose work cannot be bounded under this profile;
an instruction quota alone is not a wall-time guarantee for C implementations.

Generate component field/schema/binding descriptions from the existing registry
and the proposed generic storage adapters. Expose UUID/generation-checked
handles, copies/validated setters and explicit runtime methods, not EnTT/Jolt
pointers. Authoring/structural edits use CommandBus; tick operations use the
runtime API under ADR-0002. No public header contains a `sol::` or Lua type.

Require declared JSON-compatible script state. Engine-owned seeded RNG replaces
uncontrolled random/time input. Table iteration order is not a gameplay ordering
contract; provide ordered queries. Restrict the acceptance sample to numeric
operations covered by the deterministic profile rather than claiming arbitrary
Lua math/library behavior is bit-identical across all platforms.

Development reload prepares a new VM using the same limits, validates and
migrates declared state, then swaps at a tick boundary. Preparation cannot
issue world commands. Failure keeps the old VM; recording/playback disables
reload. No Lua coroutine/closure/opaque-heap persistence is promised.

## Alternatives and tradeoffs

- **Raw Lua API:** no binding dependency and full low-level control, but more
  custom conversion, ownership and error-boundary code to maintain. Retain as
  an alternative requiring a new owner decision if sol2 fails admission.
- **LuaBridge3:** another focused binding option; no compatibility or safety
  superiority is claimed without an actual admission comparison. Not adopted.
- **sol2:** selected for protected-call/usertype tooling; adds template/build
  complexity and requires explicit safeguards. It does not replace reflection
  or make exceptions and allocation failures automatically safe.
- **LuaJIT/Lua 5.5:** would change the owner-selected runtime; not proposed.

## Owner conditions and acceptance before relying on this decision

The owner conditionally approved sol2 3.3.0 and C++ Lua 5.4.9, provided all
three compiler probes pass with warnings-as-errors for our code. The VM choice
remains pending a measured 1,000-instance comparison and owner approval.

All three compilers prove successful bindings, wrong-type errors, stale handles,
syntax/runtime failure, instruction exhaustion and allocator failure. Destruction
sentinels must run on Lua-originated errors across the supported call paths.
ASan/UBSan must stay clean; no exception, panic or raw VM pointer crosses the
public boundary. The reflected external game component must work without a
second hand-written field list. A failed admission is an approval checkpoint,
not permission to suppress checks or substitute another runtime.

## Primary references checked for this proposal

- [sol2 v3.3.0 release](https://github.com/ThePhD/sol2/releases/tag/v3.3.0)
  and [MIT license](https://github.com/ThePhD/sol2/blob/v3.3.0/LICENSE.txt).
- [sol2 safety configuration](https://sol2.readthedocs.io/en/latest/safety.html),
  [errors/C++ Lua configuration](https://sol2.readthedocs.io/en/latest/errors.html)
  and [exception boundaries](https://sol2.readthedocs.io/en/latest/exceptions.html).
- [Lua 5.4.9 source/checksum listing](https://www.lua.org/ftp/),
  [Lua 5.4 manual](https://www.lua.org/manual/5.4/manual.html) and
  [Lua license](https://www.lua.org/license.html).

These references inform a proposal, not a claim that the selected combination
has already compiled or passed the engine's tests.
