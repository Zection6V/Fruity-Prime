#!/usr/bin/env python3
"""Report which managed symbols have no counterpart in the native tree.

Counting lines per matching filename over-states the gap badly: several
managed tables are ported into headers with different names, and a file that
looks empty can be fully covered.  This looks for the symbols themselves
instead -- the tables, types and methods a managed file declares -- and asks
whether each one appears anywhere under src/MphRead.Native.

    python tools/port-gap.py                 # the twenty widest gaps
    python tools/port-gap.py Metadata/Rooms  # one file, symbol by symbol

Names are compared case-insensitively with underscores removed, so
`MainHealthbars` matches `main_healthbars` and `DrawText2D` matches
`draw_text_2d`.
"""

from __future__ import annotations

import pathlib
import re
import sys

ROOT = pathlib.Path(__file__).resolve().parent.parent
MANAGED = ROOT / "src" / "MphRead"
NATIVE = ROOT / "src" / "MphRead.Native"

# Public tables, types and methods are the things worth accounting for.  Local
# variables and parameters are not.
DECLARATION = re.compile(
    r"\b(?:public|internal|protected|private)\s+"
    r"(?:static\s+|readonly\s+|partial\s+|abstract\s+|virtual\s+|override\s+|"
    r"sealed\s+|new\s+|async\s+)*"
    r"(?:class|struct|enum|interface)?\s*"
    r"[\w<>,\[\]?\.]+\s+"
    r"([A-Z][A-Za-z0-9_]*)\s*[\(\{=;]")

# Names too generic to mean anything on their own.
NOISE = {
    "get", "set", "value", "result", "count", "length", "size", "name",
    "type", "id", "index", "data", "item", "items", "list", "position",
    "rotation", "scale", "color", "colors", "flags", "state", "current",
    "main", "update", "draw", "process", "reset", "clear", "add", "remove",
    "contains", "equals", "gethashcode", "tostring", "dispose", "run",
}


RAW_STRING = re.compile(r'R"\((?:.|\n)*?\)"')
STRING = re.compile(r'"(?:[^"\\\n]|\\.)*"')
LINE_COMMENT = re.compile(r"//[^\n]*")


def normalise(name: str) -> str:
    return name.replace("_", "").lower()


def unported_symbols() -> set[str]:
    """Names the native tree declares but does not implement.

    A generated behaviour whose body says NOT PORTED is a name with
    nothing behind it.  Counting it would make this tool report progress
    that has not happened, so tools/gen-ai-methods.py writes the list and
    it is subtracted here.
    """
    path = pathlib.Path(__file__).resolve().parent / "ai_unported.txt"
    if not path.exists():
        return set()
    return {normalise(line.strip())
            for line in path.read_text(encoding="utf-8").splitlines()
            if line.strip() and not line.startswith("#")}


def native_symbols() -> set[str]:
    found: set[str] = set()
    for path in NATIVE.rglob("*"):
        if path.suffix not in (".cpp", ".hpp"):
            continue
        text = path.read_text(encoding="utf-8", errors="ignore")
        # String literals are stripped first: the generated AI name tables
        # spell out every Func... the managed side has, and counting those
        # would report a method as ported because its name is printable.
        text = RAW_STRING.sub(" ", text)
        text = STRING.sub(" ", text)
        text = LINE_COMMENT.sub(" ", text)
        for word in re.findall(r"[A-Za-z_][A-Za-z0-9_]*", text):
            found.add(normalise(word))
    return found - unported_symbols()


def managed_symbols(path: pathlib.Path) -> list[str]:
    text = path.read_text(encoding="utf-8", errors="ignore")
    names: list[str] = []
    seen: set[str] = set()
    for match in DECLARATION.finditer(text):
        name = match.group(1)
        key = normalise(name)
        if key in NOISE or key in seen:
            continue
        seen.add(key)
        names.append(name)
    return names


def main(argv: list[str]) -> int:
    native = native_symbols()
    if len(argv) > 1:
        target = MANAGED / (argv[1] + ".cs")
        if not target.exists():
            print("no such managed file: %s" % target)
            return 1
        missing = [name for name in managed_symbols(target)
                   if normalise(name) not in native]
        present = len(managed_symbols(target)) - len(missing)
        print("%s: %d of %d symbols present" % (argv[1], present,
                                                present + len(missing)))
        for name in missing:
            print("  missing %s" % name)
        return 0

    rows = []
    for path in sorted(MANAGED.rglob("*.cs")):
        names = managed_symbols(path)
        if not names:
            continue
        missing = [name for name in names if normalise(name) not in native]
        if missing:
            rows.append((len(missing), len(names),
                         str(path.relative_to(MANAGED)).replace("\\", "/")))
    rows.sort(reverse=True)
    print("%6s %6s  %s" % ("miss", "total", "file"))
    for missing, total, name in rows[:20]:
        print("%6d %6d  %s" % (missing, total, name))
    print("---")
    print("%d files with gaps, %d symbols missing"
          % (len(rows), sum(row[0] for row in rows)))
    return 0


if __name__ == "__main__":
    sys.exit(main(sys.argv))
