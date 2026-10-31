# M3 local LeakSanitizer diagnosis

Date: 2026-10-30. M3.6 has not started.

## Confirmed owner and mechanism

The two 56-byte allocations belong to Mesa 26.2.5 lavapipe's RTASM executable-
code arena, not the engine. This is established by exact loaded-module address
ranges, instruction/source comparison and an engine-independent reproducer.
The driver keeps its arena metadata for the shared library's lifetime. Its
code allocations are freed, but the arena's sentinel and final free block are
not destroyed on library unload. Their total is exactly 112 requested bytes.
The separate 10 MiB mmap arena is not counted by LSan's malloc leak total.

Affected local files:

- `libvulkan_lvp.so`, Build ID `75d1380ebcd48914e4248eb98cd64740c2f1330e`;
  SHA-256 `08d49503422af25fe5d9d8aee217501d12710645755a859c2e32711f68fbfc75`.
- Arch `vulkan-swrast` / Mesa `1:26.2.5-1`; LLVM `23.1.3-4`;
  GCC runtime `16.2.1+r23+gd564253eb6c8-1`.
- Mesa source archive `mesa-26.2.5.tar.xz`, SHA-256
  `794b3d1729075dae7ebd47c7283b7ca0cdb5dec668f7f11cc0a5d82f6138a0a5`,
  verified against the official release checksum.

The exact debug file was unavailable from Arch debuginfod (404). Function names
below are recovered by comparing the exact binary instructions with the exact
source; they are not claimed to be DWARF-symbolized frames. No unrelated nearby
exported C++ symbol was used as a substitute.

## Allocation stack and source correlation

Loader trace from a normal engine screenshot run:

```text
libvulkan_lvp.so base 0x7b3967200000, size 0xe55070
calloc
  0x7b39676d0b7b = libvulkan_lvp.so + 0x4d0b7b
  0x7b39674f8bcc = libvulkan_lvp.so + 0x2f8bcc
  ... remaining native driver worker frames ...
Indirect leak: 56 bytes

calloc
  0x7b39676d0b63 = libvulkan_lvp.so + 0x4d0b63
  0x7b39674f8bcc = libvulkan_lvp.so + 0x2f8bcc
  ... remaining native driver worker frames ...
Indirect leak: 56 bytes
SUMMARY: AddressSanitizer: 112 byte(s) leaked in 2 allocation(s).
```

The module's link map is destroyed before LSan prints these stacks; hence the
original reports said `<unknown module>`. Normalizing the PCs against the
loader's recorded base identifies the library unambiguously.

- `+0x4d0b30` is `u_mmInit` in `src/util/u_mm.c`. It performs two
  `calloc(1, 0x38)` calls (return PCs `+0x4d0b64` and `+0x4d0b7c`), then links
  a heap sentinel and free block exactly as the source and 56-byte structure do.
- `+0x2f8b20` is `rtasm_exec_malloc` in
  `src/gallium/auxiliary/rtasm/rtasm_execmem.c`. Its inlined `init_heap` calls
  `u_mmInit(0, 0xa00000)` at `+0x2f8bc7`, returning at `+0x2f8bcd`.
- `exec_heap` is the driver global at `+0xe50ef0`. The corresponding
  `rtasm_exec_free` entry is `+0x2f8ca0`; it returns code blocks to this arena
  rather than destroying its sentinel/final free block.

## Independent reproduction and application-leak control

[The standalone C diagnostic](../../tests/sanitizers/lavapipe_rtasm_probe.c)
links only ASan and libdl. It calls the driver's private allocator/free pair,
verifies all requested code memory is free, then closes the library. It neither
links nor runs engine, SDL, VMA or Vulkan rendering code. Because its entry
points are private binary offsets, it checks the exact ELF Build ID first and
returns 77 when a successfully loaded library has another Build ID. Its
hardcoded Arch path is not a portable driver locator: if that path cannot be
loaded it returns 2. It is a manual diagnostic, not an engine API or
an automatic platform acceptance test.

```sh
cc -g -O1 -Wall -Wextra -Werror -fsanitize=address -fno-omit-frame-pointer \
  tests/sanitizers/lavapipe_rtasm_probe.c -ldl -o /tmp/m35_lvp_rtasm_probe
ASAN_OPTIONS=detect_leaks=1:fast_unwind_on_malloc=0 \
  LSAN_OPTIONS=report_objects=1 /tmp/m35_lvp_rtasm_probe
```

Observed: sentinel and free block are 56+56 bytes, the free block covers all
10,485,760 bytes, and library close reproduces precisely the two leaks/112 bytes.
With `--keep-loaded`, the allocator globals remain reachable and the same
program exits 0. With `--keep-loaded --own-leak`, an intentional application
allocation still fails with a direct 73-byte leak at `own_leak_control`.
The control was also run with the repository suppression file loaded.
See [captured evidence](M3_SANITIZER_EVIDENCE.txt).

## Narrow local policy and its limits

The opt-in `linux-asan-lavapipe` test preset keeps all sanitizers active and uses
`VK_LOADER_DISABLE_DYNAMIC_LIBRARY_UNLOADING=1`. This is the official loader
switch for preserving native libraries and complete leak stacks. The older
attempt used `LOADER_DISABLE_DYNAMIC_LIBRARY_UNLOADING`, which was not the
runtime variable and had no effect.

The owner requested removal of the inactive `rtasm_exec_malloc` suppression
rule after source ownership was established. No suppression file or
LSAN_OPTIONS override is now loaded by this preset. On the installed stripped
driver, retention alone keeps this process-lifetime arena reachable; no
suppression produced the earlier successful run either. Historical commands
and captured output remain as evidence of the diagnosis.

```sh
VK_ICD_FILENAMES=/usr/share/vulkan/icd.d/lvp_icd.json \
  ctest --preset linux-asan-lavapipe --output-on-failure
```

The ordinary `linux-asan` preset and Ubuntu CI remain unsuppressed and unchanged.
ASan memory errors, UBSan and LeakSanitizer stay enabled. No rendering feature,
normal map, validation layer, test or shader is disabled by this local policy.
Retained driver/layer globals are LSan roots. An allocation referenced only
there can consequently remain reachable; the application-leak control does not
prove all possible driver-global references are cleaned up. Driver unload/reload
behavior itself is not exercised by the retention preset;
the standalone diagnostic documents that limitation explicitly.

Before the follow-up color golden was added, the complete retained local suite
passed 20/20; the ordinary run was 12/20 due these native reports. In the final
follow-up the local policy passed 21/21; the independent ordinary local run was
12/21, with the same 112 bytes in all 9 failures and no other leaks. The
unsuppressed Ubuntu reference CI passed 21/21. Permanent output is linked in
[the full M3.5 report](M3_5_STATUS_REPORT.md).

## Primary sources

- [Mesa 26.2.5 release and checksum](https://docs.mesa3d.org/relnotes/26.2.5.html)
- [Mesa exact source archive](https://archive.mesa3d.org/mesa-26.2.5.tar.xz)
- [RTASM allocator source](https://gitlab.freedesktop.org/mesa/mesa/-/blob/mesa-26.2.5/src/gallium/auxiliary/rtasm/rtasm_execmem.c)
- [u_mm source](https://gitlab.freedesktop.org/mesa/mesa/-/blob/mesa-26.2.5/src/util/u_mm.c)
- [Khronos loader runtime switches](https://github.com/KhronosGroup/Vulkan-Loader/blob/main/docs/LoaderInterfaceArchitecture.md)
- [LeakSanitizer suppression rules](https://clang.llvm.org/docs/LeakSanitizer.html)
