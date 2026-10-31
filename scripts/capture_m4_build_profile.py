#!/usr/bin/env python3
"""Retain effective admission compile flags from generated build files."""
from __future__ import annotations

import argparse
import json
from pathlib import Path


def main() -> None:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("build", type=Path)
    parser.add_argument("vcpkg", type=Path)
    args = parser.parse_args()
    root = Path(__file__).resolve().parents[1]

    def sanitize(value: str) -> str:
        return value.replace(str(args.vcpkg.resolve()), "<vcpkg>").replace(str(root), "<workspace>")

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
        for ninja in sorted((args.vcpkg / "buildtrees" / port).glob("*/build.ninja")):
            # Retain distinct generated define/flag lines, including Debug/Release.
            # Binary-cache hits may legitimately have no source build directory.
            lines = sorted({line.strip() for line in ninja.read_text().splitlines()
                            if line.strip().startswith(("DEFINES =", "FLAGS ="))})
            entries.append({"build": ninja.parent.name, "generatedFlags": lines})
        third_party[port] = {"sourceBuildAvailable": bool(entries), "builds": entries}
    report = {"ownCompileCommands": own, "thirdParty": third_party,
              "note": "Generated flags, not a consumer configuration-string inference. "
                      "Cache-hit absence is reported, never substituted with intended options."}
    print(sanitize(json.dumps(report, indent=2)))


if __name__ == "__main__":
    main()
