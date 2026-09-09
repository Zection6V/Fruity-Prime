#!/usr/bin/env python3
"""Generate the small ModelMetadata view used by the native Menu port.

Menu.ReadModels does not need the complete asset metadata record.  It needs
the managed lookup set, the managed name, and Recolors.Count.  Keeping this
generator tied to the C# source avoids inventing a native recolor limit or
silently accepting names which Metadata.GetModelByName would reject.
"""

from __future__ import annotations

import re
import sys
from pathlib import Path


SETS = {
    ("Metadata.cs", "ModelMetadata"): "Models",
    ("Metadata.cs", "FirstHuntModels"): "FirstHunt",
    ("FrontendMeta.cs", "HudModels"): "Hud",
    ("FrontendMeta.cs", "TouchToStartModels"): "TouchToStart",
    ("FrontendMeta.cs", "MultiplayerModels"): "Multiplayer",
    ("FrontendMeta.cs", "LogoModels"): "Logo",
    ("FrontendMeta.cs", "FrontendModels"): "Frontend",
}


def matching(text: str, opening: int, left: str, right: str) -> int:
    depth = 0
    quoted = False
    verbatim = False
    escaped = False
    for index in range(opening, len(text)):
        char = text[index]
        if quoted:
            if verbatim:
                if char == '"':
                    if index + 1 < len(text) and text[index + 1] == '"':
                        continue
                    quoted = False
            elif escaped:
                escaped = False
            elif char == "\\":
                escaped = True
            elif char == '"':
                quoted = False
            continue
        if char == '"':
            quoted = True
            verbatim = index > 0 and text[index - 1] == '@'
            continue
        if char == left:
            depth += 1
        elif char == right:
            depth -= 1
            if depth == 0:
                return index
    raise ValueError(f"unclosed {left}{right} starting at {opening}")


def string_literal(text: str, start: int) -> str:
    match = re.match(r'\s*"((?:\\.|[^"\\])*)"', text[start:])
    if not match:
        raise ValueError(f"expected model name near {text[start:start + 80]!r}")
    return bytes(match.group(1), "utf-8").decode("unicode_escape")


def recolor_count(call: str) -> int:
    if "recolors:" not in call:
        return 1
    match = re.search(r"recolors:\s*new List<string>\s*\(\)\s*\{", call)
    if not match:
        raise ValueError(f"unsupported recolor expression: {call[:120]!r}")
    opening = call.find("{", match.start())
    closing = matching(call, opening, "{", "}")
    values = call[opening + 1:closing]
    return len(re.findall(r'"(?:\\.|[^"\\])*"', values))


def dictionary_entries(source: Path, dictionary: str, set_name: str):
    text = source.read_text(encoding="utf-8")
    marker = re.compile(
        r"public static readonly FrozenDictionary<string, ModelMetadata>\s+"
        + re.escape(dictionary)
        + r"\s*=\s*Frozen\.Create",
        re.MULTILINE,
    )
    match = marker.search(text)
    if match is None:
        raise ValueError(f"dictionary not found: {source}:{dictionary}")
    end = text.find("\n        public static readonly", match.end())
    if end < 0:
        end = len(text)
    block = text[match.end():end]
    entries = []
    cursor = 0
    while True:
        call_start = block.find("new ModelMetadata(", cursor)
        if call_start < 0:
            break
        opening = block.find("(", call_start)
        closing = matching(block, opening, "(", ")")
        call = block[call_start:closing + 1]
        name = string_literal(block, opening + 1)
        entries.append((name, set_name, recolor_count(call)))
        cursor = closing + 1
    if not entries:
        raise ValueError(f"no ModelMetadata entries: {source}:{dictionary}")
    return entries


def escape_cpp(value: str) -> str:
    return value.replace("\\", "\\\\").replace('"', '\\"')


def main(root: Path) -> None:
    entries = []
    for (filename, dictionary), set_name in SETS.items():
        entries.extend(dictionary_entries(
            root / "src" / "MphRead" / "Metadata" / filename,
            dictionary,
            set_name,
        ))
    entries.extend([
        ("doubleDamage_img", "Special", 1),
        ("ad2_dm2", "Special", 1),
    ])

    output = root / "src" / "MphRead.Native" / "MenuModelMetadata.cpp"
    lines = [
        "// Generated from src/MphRead/Metadata/Metadata.cs and FrontendMeta.cs",
        "// by tools/generate-native-menu-model-catalog.py. Do not hand-edit.",
        '#include "MenuModelMetadata.hpp"',
        "",
        "#include <array>",
        "#include <string_view>",
        "",
        "namespace fruityprime::menu {",
        "namespace {",
        "",
        f"constexpr std::array<MenuModelMetadata, {len(entries)}> Records{{{{",
    ]
    for name, set_name, count in entries:
        lines.append(
            f'    {{"{escape_cpp(name)}", ModelMetadataSet::{set_name}, '
            f"{count}}},"
        )
    lines += [
        "}};",
        "",
        "} // namespace",
        "",
        "std::span<const MenuModelMetadata> menu_model_metadata() noexcept {",
        "    return Records;",
        "}",
        "",
        "} // namespace fruityprime::menu",
        "",
    ]
    output.write_text("\n".join(lines), encoding="utf-8", newline="\n")
    print(f"generated {output} ({len(entries)} entries)")


if __name__ == "__main__":
    main(Path(sys.argv[1]) if len(sys.argv) > 1 else Path(__file__).resolve().parents[1])
