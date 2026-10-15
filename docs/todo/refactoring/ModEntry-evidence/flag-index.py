"""Index every flag literal the ModEntry dispatch reads, for before/after diffs.

    python -I flag-index.py <repo> [git-rev]

Prints a sorted multiset of (lookup, literal) pairs found in ModEntry.cpp and
in Mods/Entry/*.cpp. With a git revision it reads ModEntry.cpp as it was
there instead of the working tree (Mods/Entry did not exist before ME-01).
Comparing the two outputs proves no flag string was dropped, renamed or
duplicated while code moved between files.
"""
import collections
import pathlib
import re
import subprocess
import sys

CALL = re.compile(r"\b(HasFlag|ValueAfter|ValuesAfter|IndexOfFlag|homingCheck|roomFor)\(\s*(?:args\s*,\s*)?\"([^\"]+)\"")
ARRAY = re.compile(r"\{\"(net[a-z]+|uibench[a-z]*)\"")


def sources(repo: pathlib.Path, rev: str | None) -> dict[str, str]:
    native = "src/MphRead.Native/Mods"
    if rev:
        text = subprocess.run(["git", "-C", str(repo), "show", f"{rev}:{native}/ModEntry.cpp"],
                              capture_output=True, text=True, encoding="utf-8", check=True).stdout
        return {"ModEntry.cpp": text}
    found = {"ModEntry.cpp": (repo / native / "ModEntry.cpp").read_text(encoding="utf-8")}
    for path in sorted((repo / native / "Entry").glob("*.cpp")):
        found[f"Entry/{path.name}"] = path.read_text(encoding="utf-8")
    return found


def main() -> int:
    repo = pathlib.Path(sys.argv[1])
    rev = sys.argv[2] if len(sys.argv) > 2 else None
    counts: collections.Counter[tuple[str, str]] = collections.Counter()
    for text in sources(repo, rev).values():
        for match in CALL.finditer(text):
            counts[(match.group(1), match.group(2))] += 1
        for match in ARRAY.finditer(text):
            counts[("array", match.group(1))] += 1
    for (lookup, literal), n in sorted(counts.items(), key=lambda kv: (kv[0][1], kv[0][0])):
        print(f"{literal}\t{lookup}\t{n}")
    print(f"# {sum(counts.values())} lookups, {len({k[1] for k in counts})} distinct flags", file=sys.stderr)
    return 0


if __name__ == "__main__":
    sys.exit(main())
