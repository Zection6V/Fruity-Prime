"""Generate the C++ AI func-name tables from PlayerAi.cs's switch expressions.

Run from the repository root:  python tools/gen-ai-func-names.py

The managed code names each AI subroutine after its cartridge address, which
is the only name any of them has; the tables exist so a dump of a bot's
execution path reads as something a person can follow.  They are generated
rather than typed because there are 331 arms across the four of them and a
single transposed digit would be invisible.
"""
import os
import re
import sys

SOURCE = "src/MphRead/Entities/Players/PlayerAi.cs"
OUT = "src/MphRead.Native/Entities/Players/ai_func_names.cpp"

text = open(SOURCE, encoding="utf-8").read()


def switch_body(name):
    match = re.search(
        r"static string " + name + r"\(int id\)\s*\{(.*?)\n            \}\n",
        text, re.S)
    if match is None:
        sys.exit("missing " + name)
    return match.group(1)


def parse(name):
    """Return [(pattern, value)] arms, plus the default arm's value.

    The body is flattened before it is split, because a long pattern list
    wraps onto a second line in the managed source and neither half is a
    complete arm on its own.  Values never contain a comma, so the flattened
    body splits cleanly on one.
    """
    body = switch_body(name)
    body = re.sub("//[^" + chr(10) + "]*", "", body)
    body = body[body.index("switch") + len("switch"):]
    body = body[body.index("{") + 1:]
    body = body[:body.rindex("}")]
    arms = []
    default = None
    for piece in body.split(","):
        piece = " ".join(piece.split())
        if not piece or "=>" not in piece:
            continue
        pattern, value = piece.split("=>", 1)
        pattern = pattern.strip()
        value = value.strip()
        if pattern == "_":
            default = value
            continue
        arms.append((pattern, value))
    return arms, default


def clean_value(value):
    value = value.strip().rstrip(",").strip()
    plus_star = value.endswith('+ "*"')
    if plus_star:
        value = value[: -len('+ "*"')].strip()
    match = re.fullmatch(r'nameof\(([A-Za-z0-9_]+)\)', value)
    if match is not None:
        return match.group(1) + ("*" if plus_star else "")
    match = re.fullmatch(r'"(.*)"', value)
    if match is not None:
        return match.group(1) + ("*" if plus_star else "")
    return None


def expand(pattern):
    """Turn a C# pattern into the list of ids it matches."""
    ids = []
    for part in re.split(r"\bor\b", pattern):
        part = part.strip().strip("()").strip()
        if not part:
            continue
        rng = re.fullmatch(r">=\s*(\d+)\s*and\s*<=\s*(\d+)", part)
        if rng is not None:
            ids.extend(range(int(rng.group(1)), int(rng.group(2)) + 1))
            continue
        if re.fullmatch(r"\d+", part):
            ids.append(int(part))
            continue
        sys.exit("unparsed pattern: " + repr(pattern))
    return ids


TABLES = [
    ("GetFuncs1Name", "funcs1_names", "funcs1_name",
     "// init (d3a) and process (d3b)"),
    ("GetFuncs2Name", "funcs2_names", "funcs2_name",
     "// proc (f*2*4)"),
    ("GetFuncs3Name", "funcs3_names", "funcs3_name",
     "// preconditions (d2->d4->d5) and path updates (d2->d5)"),
    ("GetFuncs4Name", "funcs4_names", "funcs4_name",
     "// init (f2*4*)"),
]

lines = [
    "// Generated from src/MphRead/Entities/Players/PlayerAi.cs by",
    "// tools/gen-ai-func-names.py -- do not edit by hand.",
    "//",
    "// Every AI subroutine is named after the cartridge address it sits at,",
    "// which is the only name any of them has.  A trailing '*' marks an id",
    "// that shares one implementation with a whole range of others, exactly as",
    "// the managed tables mark it.",
    '#include "ai_func_names.hpp"',
    "",
    "#include <array>",
    "#include <stdexcept>",
    "",
    "namespace fruityprime::players {",
    "",
    "namespace {",
    "",
]

accessors = []
for cs_name, table, accessor, comment in TABLES:
    arms, default = parse(cs_name)
    mapping = {}
    for pattern, value in arms:
        cleaned = clean_value(value)
        if cleaned is None:
            sys.exit("unparsed value in " + cs_name + ": " + repr(value))
        for entry_id in expand(pattern):
            mapping[entry_id] = cleaned
    highest = max(mapping)
    if sorted(mapping) != list(range(highest + 1)):
        missing = [i for i in range(highest + 1) if i not in mapping]
        sys.exit(cs_name + " has gaps: " + repr(missing[:10]))
    lines.append(comment)
    lines.append("constexpr std::array<const char*, %d> %s{{"
                 % (highest + 1, table))
    row = []
    for entry_id in range(highest + 1):
        row.append('"%s"' % mapping[entry_id])
    for start in range(0, len(row), 3):
        lines.append("    " + ", ".join(row[start:start + 3]) + ",")
    lines[-1] = lines[-1].rstrip(",")
    lines.append("}};")
    lines.append("")
    accessors.append((accessor, table, len(row)))

lines.append("} // namespace")
lines.append("")
for accessor, table, count in accessors:
    lines.append("std::string_view %s(int id) {" % accessor)
    lines.append("    if (id < 0 || id >= %d) {" % count)
    lines.append('        throw std::out_of_range("Invalid AI func id.");')
    lines.append("    }")
    lines.append("    return %s[static_cast<std::size_t>(id)];" % table)
    lines.append("}")
    lines.append("")

lines.append("std::vector<std::string_view> funcs1_name_list(")
lines.append("    std::span<const int> ids) {")
lines.append("    std::vector<std::string_view> names;")
lines.append("    names.reserve(ids.size());")
lines.append("    for (int id : ids) {")
lines.append("        names.push_back(funcs1_name(id));")
lines.append("    }")
lines.append("    return names;")
lines.append("}")
lines.append("")
lines.append("std::vector<std::string_view> funcs3_name_list(")
lines.append("    std::span<const int> ids) {")
lines.append("    std::vector<std::string_view> names;")
lines.append("    names.reserve(ids.size());")
lines.append("    for (int id : ids) {")
lines.append("        names.push_back(funcs3_name(id));")
lines.append("    }")
lines.append("    return names;")
lines.append("}")
lines.append("")
lines.append("} // namespace fruityprime::players")
lines.append("")

os.makedirs(os.path.dirname(OUT), exist_ok=True)
open(OUT, "w", encoding="utf-8", newline="\n").write("\n".join(lines))
print("wrote", OUT, "tables:", [(a, c) for a, _t, c in accessors])
