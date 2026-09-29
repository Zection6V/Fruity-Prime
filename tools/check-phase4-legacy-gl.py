#!/usr/bin/env python3
"""Reject legacy OpenGL submission paths forbidden by RHI Phase 4."""

from __future__ import annotations

import re
import sys
from pathlib import Path

SOURCE_ROOT = Path("src/MphRead.Native")
SOURCE_SUFFIXES = {".c", ".cc", ".cpp", ".cxx", ".h", ".hpp", ".hxx"}

RULES: tuple[tuple[str, re.Pattern[str]], ...] = (
    (
        "display-list API",
        re.compile(r"\\b(?:GenLists|NewList|EndList|CallList|DeleteLists)\\b"),
    ),
    (
        "immediate-mode begin/end",
        re.compile(r"(?:\\bGL::(?:Begin|End)|\\bgl(?:Begin|End))\\s*\\("),
    ),
    (
        "immediate-mode vertex submission",
        re.compile(r"(?:\\bGL::Vertex(?:[234])?|\\bglVertex[234][A-Za-z0-9_]*)\\s*\\("),
    ),
)


def main() -> int:
    if not SOURCE_ROOT.is_dir():
        print(f"Phase 4 legacy OpenGL audit failed: missing {SOURCE_ROOT}", file=sys.stderr)
        return 2

    violations: list[tuple[Path, int, str, str]] = []
    scanned = 0
    for path in sorted(SOURCE_ROOT.rglob("*")):
        if not path.is_file() or path.suffix.lower() not in SOURCE_SUFFIXES:
            continue
        scanned += 1
        text = path.read_text(encoding="utf-8", errors="replace")
        for line_number, line in enumerate(text.splitlines(), 1):
            for label, pattern in RULES:
                if pattern.search(line):
                    violations.append((path, line_number, label, line.strip()))

    if violations:
        print("Phase 4 legacy OpenGL audit failed:", file=sys.stderr)
        for path, line_number, label, line in violations:
            print(f"  {path}:{line_number}: {label}: {line}", file=sys.stderr)
        return 1

    print(
        "Phase 4 legacy OpenGL audit passed: "
        f"scanned {scanned} C/C++ source files; "
        "display-list APIs=0; immediate vertex submission=0."
    )
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
