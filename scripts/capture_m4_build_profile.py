#!/usr/bin/env python3
"""Retain effective admission compile flags from generated build files."""
from __future__ import annotations

import argparse
import hashlib
import json
import re
from pathlib import Path


def main() -> None:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("build", type=Path)
    parser.add_argument("vcpkg", type=Path)
    args = parser.parse_args()
    root = Path(__file__).resolve().parents[1]

    def sanitize(value):
        if isinstance(value, str):
            return value.replace(str(args.vcpkg.resolve()), "<vcpkg>").replace(str(root), "<workspace>")
        if isinstance(value, list):
            return [sanitize(item) for item in value]
        if isinstance(value, dict):
            return {key: sanitize(item) for key, item in value.items()}
        return value

    def compiler(path: Path) -> dict:
        data = path.read_text()

        def get(name):
            match = re.search(r'set\(' + re.escape(name) + r' "([^"]*)"\)', data)
            assert match, f"missing generated {name}: {path}"
            return match.group(1)

        return {"path": get("CMAKE_CXX_COMPILER"), "id": get("CMAKE_CXX_COMPILER_ID"),
                "version": get("CMAKE_CXX_COMPILER_VERSION"),
                "defaultStandard": get("CMAKE_CXX_STANDARD_COMPUTED_DEFAULT")}

    cache = (args.build / "CMakeCache.txt").read_text()

    def cached(name):
        match = re.search(r'^' + re.escape(name) + r':[^=\n]+=(.*)$', cache, re.MULTILINE)
        assert match, f"missing CMake cache entry {name}"
        return match.group(1).strip()

    cmake_version = ".".join(cached("CMAKE_CACHE_" + part + "_VERSION")
                             for part in ("MAJOR", "MINOR", "PATCH"))
    host = compiler(args.build / "CMakeFiles" / cmake_version / "CMakeCXXCompiler.cmake")
    prefix = Path(cached("VCPKG_INSTALLED_DIR")) / cached("VCPKG_TARGET_TRIPLET")

    commands = json.loads((args.build / "compile_commands.json").read_text())
    own = [entry for entry in commands if Path(entry["file"]).name in ("lua_probe.cpp", "jolt_probe.cpp")]
    assert len(own) == 2, "both admission translation units must be present"
    for entry in own:
        command = entry["command"]
        assert "-Werror" in command or "/WX" in command, "our bridge must use warnings as errors"
        assert "-ffp-contract=off" in command or "/fp:strict" in command
    third_party = {}
    for port in ("joltphysics", "engine-lua-cxx"):
        entries = []
        for config in ("dbg", "rel"):
            provenance = prefix / "share" / port / "build-provenance" / config
            identity = compiler(provenance / "compiler.txt")
            assert (identity["id"], identity["version"]) == (host["id"], host["version"]), (
                f"{port}/{config} archive compiler {identity} does not match host {host}")
            digest, relative = (provenance / "archive-sha256.txt").read_text().strip().split(" ", 1)
            archive = prefix / relative
            assert archive.is_relative_to(prefix) and ".." not in archive.parts
            assert hashlib.sha256(archive.read_bytes()).hexdigest() == digest, "archive/provenance mismatch"
            library_commands = json.loads((provenance / "compile_commands.json").read_text())
            assert library_commands, f"missing effective {port}/{config} compile commands"
            for entry in library_commands:
                command = entry["command"]
                assert not any(flag in command for flag in (
                    "-ffast-math", "/fp:fast", "-fp:fast", "-fno-exceptions")), command
                if identity["id"] == "MSVC":
                    assert "/EHsc" in command or "-EHsc" in command, command
                    assert any(flag in command for flag in (
                        "/fp:strict", "-fp:strict", "/fp:precise", "-fp:precise")), command
                else:
                    assert "-fexceptions" in command and "-ffp-contract=off" in command, command
                if port == "engine-lua-cxx":
                    assert any(flag in command for flag in ("-x c++", "/TP", "-TP")), command
                    assert (int(identity["defaultStandard"]) >= 20 or
                            any(flag in command for flag in (
                                "-std=c++20", "-std=gnu++20", "-std:c++20", "/std:c++20"))), command
                else:
                    assert re.search(r'(?:-D|/D)\s*JPH_DOUBLE_PRECISION\b', command), command
                    assert re.search(r'(?:-D|/D)\s*JPH_CROSS_PLATFORM_DETERMINISTIC\b', command), command
                    assert not any(flag in command for flag in (
                        "-mavx", "-mfma", "-mf16c", "-mlzcnt", "-mbmi",
                        "/arch:AVX", "-arch:AVX", "-fno-rtti")), command
                    for feature in ("SSE4_1", "SSE4_2"):
                        assert re.search(r'(?:-D|/D)\s*JPH_USE_' + feature + r'\b', command), command
                    for feature in ("AVX", "AVX2", "AVX512", "FMADD", "F16C", "LZCNT", "TZCNT"):
                        assert not re.search(r'(?:-D|/D)\s*JPH_USE_' + feature + r'\b', command), command
                    if identity["id"] == "MSVC":
                        assert ("/GR" in command or "-GR" in command) and not any(
                            flag in command for flag in ("/GR-", "-GR-")), command
                    else:
                        assert "-frtti" in command, command
            entries.append({"configuration": config, "compiler": identity,
                            "archive": relative, "archiveSha256": digest,
                            "compileCommands": library_commands})
        third_party[port] = {"cachedArchiveProvenanceVerified": True, "builds": entries}
    report = {"hostCompiler": host, "ownCompileCommands": own, "thirdParty": third_party,
              "note": "Actual CMake compiler identities and commands packaged with each library; "
                      "installed archive digests verified, including cache hits. "
                      "Both libraries must match the host compiler ID/version in Debug and Release."}
    print(json.dumps(sanitize(report), indent=2))


if __name__ == "__main__":
    main()
