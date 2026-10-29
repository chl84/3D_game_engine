#!/usr/bin/env python3
"""Scan tracked files for credentials and personal paths.

The scan is intentionally conservative: it reports the pattern name and
location, never the matched text, so CI logs do not leak a secret.
"""

from __future__ import annotations

import re
import subprocess
import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parents[2]

PATTERNS = [
    ("private-key", re.compile(r"-----BEGIN (?:RSA |EC |OPENSSH |DSA )?PRIVATE KEY-----")),
    ("github-token", re.compile(r"\bgh[pousr]_[A-Za-z0-9]{20,}")),
    ("github-pat", re.compile(r"\bgithub_pat_[A-Za-z0-9_]{20,}")),
    ("aws-access-key", re.compile(r"\bAKIA[0-9A-Z]{16}\b")),
    ("slack-token", re.compile(r"\bxox[baprs]-[A-Za-z0-9-]{10,}")),
    ("google-api-key", re.compile(r"\bAIza[0-9A-Za-z_-]{35}\b")),
    ("openai-key", re.compile(r"\bsk-[A-Za-z0-9]{32,}\b")),
    (
        "credential-assignment",
        re.compile(
            r"(?i)\b(?:password|passwd|secret|api[_-]?key|access[_-]?token|auth[_-]?token)\b"
            r"\s*[:=]\s*['\"]?[^\s'\"]{4,}"
        ),
    ),
    ("absolute-home-path", re.compile(r"(?<![\w.])/(?:home|Users)/[A-Za-z0-9._-]+/")),
    ("windows-user-path", re.compile(r"[A-Za-z]:\\Users\\[^\\\s]+\\")),
    (
        "email-address",
        re.compile(r"[A-Za-z0-9._%+-]+@[A-Za-z0-9.-]+\.[A-Za-z]{2,}"),
    ),
    ("ssh-public-key", re.compile(r"\bssh-(?:rsa|ed25519) AAAA[A-Za-z0-9+/=]{20,}")),
]

EMAIL_ALLOWLIST = re.compile(
    r"@(?:users\.noreply\.github\.com|example\.(?:com|org|net)|.*\.schema\.json)$"
)


def tracked_files() -> list[Path]:
    try:
        output = subprocess.run(
            ["git", "-C", str(ROOT), "ls-files", "-z"],
            check=True,
            capture_output=True,
        ).stdout
    except (OSError, subprocess.CalledProcessError):
        ignored = {".git", "build", ".venv", "reviews", "vcpkg", "vcpkg_installed"}
        return [
            path
            for path in ROOT.rglob("*")
            if path.is_file() and not any(part in ignored for part in path.parts)
        ]

    files: list[Path] = []
    for entry in output.decode("utf-8", errors="replace").split("\0"):
        if entry:
            files.append(ROOT / entry)
    return files


def main() -> int:
    errors: list[str] = []
    for path in tracked_files():
        try:
            text = path.read_text(encoding="utf-8", errors="replace")
        except OSError as error:
            errors.append(f"{path.relative_to(ROOT)}: cannot read ({error})")
            continue
        for line_number, line in enumerate(text.splitlines(), start=1):
            for name, pattern in PATTERNS:
                match = pattern.search(line)
                if not match:
                    continue
                if name == "email-address" and EMAIL_ALLOWLIST.search(match.group(0)):
                    continue
                errors.append(f"{path.relative_to(ROOT)}:{line_number}: {name}")

    if errors:
        for error in errors:
            print(f"error: {error}", file=sys.stderr)
        return 1
    print(f"sensitive-data scan OK ({len(tracked_files())} tracked files)")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
