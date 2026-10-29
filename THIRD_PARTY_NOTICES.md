# Third-Party Notices

The engine is MIT-licensed. This file lists the third-party components used by
the engine, tools and CI, with their licenses. Full license texts are included
in the corresponding source distributions and in the vcpkg installed tree.

## Runtime and build dependencies

| Component | Version | License | Used by |
|---|---|---|---|
| nlohmann/json | 3.12.0 | MIT | Engine core |
| spdlog | 1.17.0 | MIT | Engine core |
| fmt | 12.2.0 | MIT | Transitive dependency of spdlog |
| CLI11 | 2.7.2 | BSD-3-Clause | `engine` CLI |
| tl::expected | 1.3.1 | CC0-1.0 | `Result<T>` |
| volk | 1.4.363.0 | MIT | Vulkan loading in `doctor` and `render` |
| EnTT | 4.0.0 | MIT | Entity-component system |
| json-schema-validator (`pboettch`) | 2.4.0 | MIT | JSON Schema validation |
| SDL3 | 3.4.18 | Zlib AND MIT AND Apache-2.0 AND BSD-3-Clause | Window, input and Vulkan surface (`platform`) |
| Vulkan Memory Allocator | 3.5.0 | MIT | GPU memory allocation (`render`) |
| GLM | 1.0.3 | MIT | Renderer math (`render`) |
| stb_image / stb_image_write (stb) | 2026-08-02 | MIT OR Unlicense | PNG encoding and golden-image decoding |

## Test and development dependencies

| Component | Version | License | Used by |
|---|---|---|---|
| Catch2 | 3.16.1 | BSL-1.0 AND BSD-3-Clause | Unit and integration tests |
| jsonschema (Python) | 4.26.0 | MIT | Contract tests in CI |
| vcpkg | baseline `0e5eb43501dab17aa055dd54bd7a8c018f330fe2` | MIT | Dependency management |
| CMake | 4.4.5 in CI | BSD-3-Clause | Build system |
| Ninja | 1.13.2 in CI | Apache-2.0 | Build tool |
| GCC | 14 in CI | GPL-3.0 with GCC Runtime Library Exception | Linux compiler |
| Clang | 18 in CI | Apache-2.0 with LLVM exceptions | Linux compiler |
| MSVC | Visual Studio 2022 17.14 | Proprietary | Windows compiler |
| Vulkan SDK | 1.4.363.0 | Apache-2.0 / MIT components | Headers, loader, validation layers, `glslc`, `vulkaninfo` |
| glslc (shaderc CLI) | distro package on Linux; Vulkan SDK 1.4.363 on Windows | Apache-2.0 | GLSL to SPIR-V build step, not linked |

Compilers, CMake, Ninja and vcpkg are build tools, not linked engine
dependencies. The GCC runtime library exception and the LLVM exception make
the compiler outputs redistributable under the engine's MIT license.

## MIT License

The following components are distributed under the MIT License: nlohmann/json,
spdlog, fmt, volk, EnTT, json-schema-validator, jsonschema, Vulkan Memory
Allocator and GLM.

```text
Permission is hereby granted, free of charge, to any person obtaining a copy
of this software and associated documentation files (the "Software"), to deal
in the Software without restriction, including without limitation the rights
to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
copies of the Software, and to permit persons to whom the Software is
furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in all
copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
SOFTWARE.
```

## BSD 3-Clause License

The following components are distributed under the BSD 3-Clause License:
CLI11, Catch2 (in addition to BSL-1.0), CMake and the Vulkan SDK components
that are BSD-licensed.

```text
Redistribution and use in source and binary forms, with or without
modification, are permitted provided that the following conditions are met:

1. Redistributions of source code must retain the above copyright notice,
   this list of conditions and the following disclaimer.

2. Redistributions in binary form must reproduce the above copyright notice,
   this list of conditions and the following disclaimer in the documentation
   and/or other materials provided with the distribution.

3. Neither the name of the copyright holder nor the names of its contributors
   may be used to endorse or promote products derived from this software
   without specific prior written permission.

THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE LIABLE
FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER
CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY,
OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
```

## Boost Software License 1.0

Catch2 is primarily distributed under the Boost Software License 1.0.

```text
Boost Software License - Version 1.0 - August 17th, 2003

Permission is hereby granted, free of charge, to any person or organization
obtaining a copy of the software and accompanying documentation covered by
this license (the "Software") to use, reproduce, display, distribute,
execute, and transmit the Software, and to prepare derivative works of the
Software, and to permit third-parties to whom the Software is furnished to do
so, all subject to the following:

The copyright notices in the Software and this entire statement, including
the above license grant, this restriction and the following disclaimer, must
be included in all copies of the Software, in whole or in part, and all
derivative works of the Software, unless such copies or derivative works are
solely in the form of machine-executable object code generated by a source
language processor.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
FITNESS FOR A PARTICULAR PURPOSE, TITLE AND NON-INFRINGEMENT. IN NO EVENT
SHALL THE COPYRIGHT HOLDERS OR ANYONE DISTRIBUTING THE SOFTWARE BE LIABLE FOR
ANY DAMAGES OR OTHER LIABILITY, WHETHER IN CONTRACT, TORT OR OTHERWISE,
ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER
DEALINGS IN THE SOFTWARE.
```

## CC0 1.0 Universal

tl::expected is dedicated to the public domain under CC0 1.0 Universal. The
full legal text is available at
<https://creativecommons.org/publicdomain/zero/1.0/legalcode>.

## Apache License 2.0

Ninja, Clang, parts of the Vulkan SDK and bundled SDL3 components are
distributed under the Apache License 2.0. The full text is available at
<https://www.apache.org/licenses/LICENSE-2.0>.

## Zlib License

SDL3's core is distributed under the zlib license. The full text is available
at <https://www.zlib.net/zlib_license.html>. SDL3 also bundles components under
the MIT, Apache-2.0 and BSD-3-Clause licenses; their texts are included in the
SDL3 source distribution and in the vcpkg installed tree.

## Updating this file

A new dependency or a version upgrade updates `docs/DEPENDENCIES.md` and this
file in the same change. A dependency with a license outside the allowlist
requires owner approval and an ADR.
