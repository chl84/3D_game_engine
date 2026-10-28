# Build

**Status:** M0.2. The build uses a pinned vcpkg manifest, the `engine` version
command and Catch2 tests. Vulkan (`volk`) and `engine doctor` are added in M0.4.

## Local development environment

| Component | Version |
|---|---|
| OS | Omarchy 4.0.6 (Arch-based), Linux 7.2.8-9-omarchy, x86_64 |
| CPU | Intel Core i7-1265U, 10 cores / 12 threads |
| GPU | Intel Iris Xe Graphics (ADL GT2), Mesa 26.2.5, Vulkan 1.4.354 |
| RAM | 30 GiB local |
| GCC | 16.2.1 |
| Clang | 23.1.3 |
| CMake | 4.4.4 |
| Ninja | 1.13.2 |
| Python | 3.14.8 |
| vcpkg | bootstrapped at `~/.local/share/vcpkg`, commit `0e5eb43501dab17aa055dd54bd7a8c018f330fe2` |

The CI reference environment is Ubuntu 24.04 with GCC 14 and Clang 18, plus
Windows Server 2022 with MSVC. CI is the source of truth for reproducible
builds; local Omarchy versions may be newer and are reported here.

## Prerequisites

- CMake 3.28 or newer
- Ninja
- A C++20 compiler: GCC 14+, Clang 18+, or MSVC 19.4+
- Python 3.10+ (test scripts)

Vulkan development packages are needed from M2 onward. They are already
installed on the development machine:

```bash
sudo pacman -S --needed vulkan-headers vulkan-validation-layers vulkan-tools vulkan-swrast ccache
```

`vulkan-swrast` provides lavapipe, the software Vulkan device used for
headless tests and golden images.

## vcpkg

The manifest is `vcpkg.json`, pinned to baseline
`0e5eb43501dab17aa055dd54bd7a8c018f330fe2`. Bootstrap it once:

```bash
git clone https://github.com/microsoft/vcpkg "$HOME/.local/share/vcpkg"
"$HOME/.local/share/vcpkg/bootstrap-vcpkg.sh" -disableMetrics
export VCPKG_ROOT="$HOME/.local/share/vcpkg"
```

The CMake presets use `$env{VCPKG_ROOT}/scripts/buildsystems/vcpkg.cmake`.
The first configure installs the pinned dependencies into
`build/<preset>/vcpkg_installed/`.

## Configure and build

```bash
export VCPKG_ROOT="$HOME/.local/share/vcpkg"
cmake --preset linux-debug
cmake --build --preset linux-debug
ctest --preset linux-debug --output-on-failure
```

The binary is at:

```text
build/linux-debug/tools/engine/engine
```

To use Clang:

```bash
cmake --preset ci-linux-clang
cmake --build --preset ci-linux-clang
ctest --preset ci-linux-clang --output-on-failure
```

To use GCC explicitly:

```bash
cmake --preset ci-linux-gcc
cmake --build --preset ci-linux-gcc
ctest --preset ci-linux-gcc --output-on-failure
```

The CI presets set `ENGINE_WARNINGS_AS_ERRORS=ON`, so warnings fail the build.

## Sanitizers

AddressSanitizer + UndefinedBehaviorSanitizer:

```bash
cmake --preset linux-asan
cmake --build --preset linux-asan
ctest --preset linux-asan --output-on-failure
```

ThreadSanitizer:

```bash
cmake --preset linux-tsan
cmake --build --preset linux-tsan
ctest --preset linux-tsan --output-on-failure
```

ASan/UBSan and TSan cannot be combined. Sanitizers are GCC/Clang only.

## Formatting and static analysis

```bash
clang-format --dry-run --Werror $(find engine tools tests -type f \( -name '*.cpp' -o -name '*.hpp' \))
clang-tidy -p build/ci-linux-clang engine/core/src/version.cpp tools/engine/src/main.cpp
```

CI uses clang-format and clang-tidy 18; the development machine has version 23.
The CI version is authoritative for formatting.

## Version command

```bash
./build/linux-debug/tools/engine/engine --version --json
```

Expected shape:

```json
{"name":"engine","version":"0.1.0","gitCommit":"...","buildType":"Debug","compiler":"GNU 16.2.1","platform":"Linux-x86_64","cxxStandard":"20"}
```

## Windows

Windows is **not verified** yet. The `windows-debug`, `windows-release` and
`ci-windows` presets use MSVC and vcpkg. The Windows CI job compiles and runs
headless tests from M0.2 onward; the runner has no GPU, so graphics is not
tested there. Graphics on Windows is not part of the M2/M3 approval and will be
reported as **not verified** until the owner tests it manually on a Windows
machine.

The CI workflow sets up MSVC with `ilammy/msvc-dev-cmd` and uses the
`x64-windows` vcpkg triplet. The first configure installs the dependencies in
`build/ci-windows/vcpkg_installed/`.

## Toolchain versions

M0.2 uses the CI runner's CMake and Ninja (CMake 3.28+). M0.5 pins CMake 4.4.5
and Ninja 1.13.2 in CI. The development machine has CMake 4.4.4 and Ninja
1.13.2; the difference is recorded here and CI remains the source of truth.

## Memory

The development machine has 30 GiB of RAM. The project reference requirement
remains 16 GB, and the provisional memory budget remains 4 GB for the game
process in the reference scene. The local 30 GiB is a development convenience,
not a change to the target.

## Vulkan verification

```bash
vulkaninfo --summary
```

The development machine reports:

- Intel Iris Xe Graphics (ADL GT2), Vulkan 1.4.354, Mesa 26.2.5
- llvmpipe (lavapipe), Vulkan 1.4.354, Mesa 26.2.5
- `VK_LAYER_KHRONOS_validation` 1.4.363

To force lavapipe:

```bash
VK_ICD_FILENAMES=/usr/share/vulkan/icd.d/lvp_icd.json vulkaninfo --summary
```
