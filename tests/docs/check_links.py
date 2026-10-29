#!/usr/bin/env python3
"""Check local Markdown links and the documentation files required by B.10."""

from __future__ import annotations

import re
import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parents[2]

REQUIRED_DOCS = [
    "docs/VISION.md",
    "docs/STATUS.md",
    "docs/decisions/README.md",
    "docs/ARCHITECTURE.md",
    "docs/BUILD.md",
    "docs/TESTING.md",
    "docs/DEPENDENCIES.md",
    "docs/PROJECT_FORMAT.md",
    "docs/AI_AGENT_GUIDE.md",
    "docs/API_REFERENCE.md",
    "docs/ROADMAP.md",
    "README.md",
    "LICENSE",
    "THIRD_PARTY_NOTICES.md",
]

LINK_PATTERN = re.compile(r"!?\[[^\]]*\]\(([^)]+)\)")
REFERENCE_PATTERN = re.compile(r"^\s*\[[^\]]+\]:\s+(\S+)")
SKIP_PREFIXES = ("http://", "https://", "mailto:", "#")


def markdown_files() -> list[Path]:
    files = [ROOT / "README.md", ROOT / "THIRD_PARTY_NOTICES.md"]
    files.extend(sorted((ROOT / "docs").rglob("*.md")))
    return [path for path in files if path.is_file()]


def strip_target(target: str) -> str:
    target = target.strip()
    if target.startswith("<") and target.endswith(">"):
        target = target[1:-1]
    target = target.split(maxsplit=1)[0]
    return target.split("#", maxsplit=1)[0].split("?", maxsplit=1)[0]


def check_target(source: Path, target: str) -> str | None:
    if not target or target.startswith(SKIP_PREFIXES):
        return None
    cleaned = strip_target(target)
    if not cleaned:
        return None
    candidate = Path(cleaned)
    if candidate.is_absolute():
        resolved = ROOT / candidate.relative_to("/")
    else:
        resolved = source.parent / candidate
    try:
        resolved = resolved.resolve()
    except OSError:
        return f"{source.relative_to(ROOT)}: cannot resolve {target}"
    if not resolved.exists():
        return f"{source.relative_to(ROOT)}: missing target {target}"
    return None


def main() -> int:
    errors: list[str] = []
    for required in REQUIRED_DOCS:
        if not (ROOT / required).exists():
            errors.append(f"missing required documentation file: {required}")

    for source in markdown_files():
        in_fence = False
        for line in source.read_text(encoding="utf-8").splitlines():
            if line.lstrip().startswith("```"):
                in_fence = not in_fence
                continue
            if in_fence:
                continue
            for match in LINK_PATTERN.finditer(line):
                error = check_target(source, match.group(1))
                if error:
                    errors.append(error)
            match = REFERENCE_PATTERN.match(line)
            if match:
                error = check_target(source, match.group(1))
                if error:
                    errors.append(error)

    if errors:
        for error in errors:
            print(f"error: {error}", file=sys.stderr)
        return 1
    print(f"docs links OK ({len(markdown_files())} files)")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
