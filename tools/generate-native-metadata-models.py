#!/usr/bin/env python3
"""Generate the complete ModelMetadata tables from Metadata.cs.

The managed table is deliberately kept as the source of truth.  This parser
only understands the constructor forms used by that table and emits the
corresponding native ModelMetadata constructor/factory calls, including the
explicit recolor and replacement records.
"""

from __future__ import annotations

import re
import sys
from pathlib import Path


def matching(text: str, opening: int, left: str, right: str) -> int:
    depth = 0
    quoted = False
    verbatim = False
    escaped = False
    index = opening
    while index < len(text):
        char = text[index]
        if quoted:
            if verbatim:
                if char == '"':
                    if index + 1 < len(text) and text[index + 1] == '"':
                        index += 2
                        continue
                    quoted = False
            elif escaped:
                escaped = False
            elif char == "\\":
                escaped = True
            elif char == '"':
                quoted = False
            index += 1
            continue
        if char == '"':
            quoted = True
            verbatim = index > 0 and text[index - 1] == '@'
        elif char == left:
            depth += 1
        elif char == right:
            depth -= 1
            if depth == 0:
                return index
        index += 1
    raise ValueError(f"unclosed {left}{right} starting at {opening}")


def split_top_level(text: str) -> list[str]:
    parts: list[str] = []
    start = 0
    depth = 0
    quoted = False
    verbatim = False
    escaped = False
    index = 0
    while index < len(text):
        char = text[index]
        if quoted:
            if verbatim:
                if char == '"':
                    if index + 1 < len(text) and text[index + 1] == '"':
                        index += 2
                        continue
                    quoted = False
            elif escaped:
                escaped = False
            elif char == "\\":
                escaped = True
            elif char == '"':
                quoted = False
            index += 1
            continue
        if char == '"':
            quoted = True
            verbatim = index > 0 and text[index - 1] == '@'
        elif char in "({[<":
            depth += 1
        elif char in ")}]>":
            depth -= 1
        elif char == "," and depth == 0:
            if text[start:index].strip():
                parts.append(text[start:index].strip())
            start = index + 1
        index += 1
    if text[start:].strip():
        parts.append(text[start:].strip())
    return parts


def named_arguments(parts: list[str]) -> tuple[list[str], dict[str, str]]:
    positional: list[str] = []
    named: dict[str, str] = {}
    for part in parts:
        match = re.match(r"^([A-Za-z_]\w*)\s*:\s*(.*)$", part, re.S)
        if match:
            named[match.group(1)] = match.group(2).strip()
        else:
            positional.append(part)
    return positional, named


def string_value(expression: str) -> str:
    expression = expression.strip()
    first_quote = expression.find('"')
    if first_quote < 0:
        raise ValueError(f"expected string literal: {expression!r}")
    verbatim = '@' in expression[:first_quote]
    chars: list[str] = []
    index = first_quote + 1
    while index < len(expression):
        char = expression[index]
        if verbatim:
            if char == '"':
                if index + 1 < len(expression) and expression[index + 1] == '"':
                    chars.append('"')
                    index += 2
                    continue
                return "".join(chars)
        else:
            if char == '"':
                return "".join(chars)
            if char == "\\" and index + 1 < len(expression):
                chars.append(expression[index + 1])
                index += 2
                continue
        chars.append(char)
        index += 1
    raise ValueError(f"unclosed string literal: {expression!r}")


def is_null(expression: str | None) -> bool:
    return expression is None or expression.strip() == "null"


def cpp_string(expression: str) -> str:
    value = string_value(expression)
    if ")\"" not in value:
        return f'R"({value})"'
    escaped = value.replace("\\", "\\\\").replace('"', '\\"')
    return f'"{escaped}"'


def cpp_optional(expression: str | None) -> str:
    if is_null(expression):
        return "std::nullopt"
    return f"std::optional<std::string>{{{cpp_string(expression or '')}}}"


def cpp_bool(expression: str | None, default: bool = False) -> str:
    if expression is None:
        return "true" if default else "false"
    value = expression.strip()
    if value not in ("true", "false"):
        raise ValueError(f"expected bool, got {expression!r}")
    return value


def cpp_suffix(expression: str | None) -> str:
    if expression is None:
        return "MdlSuffix::None"
    match = re.fullmatch(r"MdlSuffix\.(None|All|Model)", expression.strip())
    if not match:
        raise ValueError(f"expected MdlSuffix, got {expression!r}")
    return f"MdlSuffix::{match.group(1)}"


def initializer_body(expression: str) -> str:
    opening = expression.find("{")
    if opening < 0:
        raise ValueError(f"expected collection initializer: {expression!r}")
    closing = matching(expression, opening, "{", "}")
    return expression[opening + 1:closing]


def string_list(expression: str) -> list[str]:
    return [string_value(part) for part in split_top_level(initializer_body(expression))
            if '"' in part]


def replace_ids(expression: str | None) -> str:
    if is_null(expression):
        return "std::map<int, std::vector<int>>{}"
    body = initializer_body(expression)
    records: list[str] = []
    for item in split_top_level(body):
        item = item.strip()
        if not item.startswith("{"):
            continue
        close = matching(item, 0, "{", "}")
        key_and_value = split_top_level(item[1:close])
        if len(key_and_value) != 2:
            raise ValueError(f"unsupported replaceIds item: {item!r}")
        key = key_and_value[0].strip()
        values = [int(value) for value in re.findall(
            r"-?\d+", initializer_body(key_and_value[1]))]
        records.append(
            "{ " + key + ", std::vector<int>{" + ", ".join(map(str, values)) + "} }"
        )
    return "std::map<int, std::vector<int>>{" + ", ".join(records) + "}"


def recolor_call(expression: str) -> str:
    opening = expression.find("(")
    closing = matching(expression, opening, "(", ")")
    parts = split_top_level(expression[opening + 1:closing])
    positional, named = named_arguments(parts)
    if not positional:
        raise ValueError(f"recolor has no name: {expression!r}")
    name = positional[0]
    positional_index = 1

    def take(parameter: str) -> str:
        nonlocal positional_index
        if parameter in named:
            return named[parameter]
        if positional_index >= len(positional):
            raise ValueError(f"missing {parameter} in {expression!r}")
        value = positional[positional_index]
        positional_index += 1
        return value

    model = take("modelPath")
    if "texturePath" not in named and positional_index >= len(positional):
        return f"RecolorMetadata({cpp_string(name)}, {cpp_string(model)})"
    texture = take("texturePath")
    if "palettePath" not in named and positional_index >= len(positional):
        return (f"RecolorMetadata({cpp_string(name)}, {cpp_string(model)}, "
                f"{cpp_string(texture)})")
    palette = take("palettePath")
    ids = replace_ids(named.get("replaceIds"))
    separate = cpp_bool(named.get("separateReplace"))
    return (f"RecolorMetadata({cpp_string(name)}, {cpp_string(model)}, "
            f"{cpp_string(texture)}, {cpp_string(palette)}, {ids}, {separate})")


def recolor_list(expression: str) -> str:
    records: list[str] = []
    cursor = 0
    while True:
        start = expression.find("new RecolorMetadata(", cursor)
        if start < 0:
            break
        opening = expression.find("(", start)
        closing = matching(expression, opening, "(", ")")
        records.append(recolor_call(expression[start:closing + 1]))
        cursor = closing + 1
    if not records:
        raise ValueError(f"recolor list is empty or unsupported: {expression!r}")
    return "std::vector<RecolorMetadata>{" + ", ".join(records) + "}"


def model_call(expression: str) -> tuple[str, str]:
    opening = expression.find("(")
    closing = matching(expression, opening, "(", ")")
    parts = split_top_level(expression[opening + 1:closing])
    positional, named = named_arguments(parts)
    if not positional:
        raise ValueError(f"model has no name: {expression!r}")
    name = cpp_string(positional[0])

    recolors_expression = named.get("recolors")
    if recolors_expression is None:
        recolors_expression = next(
            (part for part in positional[1:] if "List<RecolorMetadata>" in part),
            None,
        )

    if recolors_expression is not None:
        if "List<string>" in recolors_expression:
            values = string_list(recolors_expression)
            value_expression = "std::vector<std::string>{" + ", ".join(
                cpp_string(f'"{value}"') for value in values) + "}"
            args = [
                name,
                value_expression,
                cpp_optional(named.get("remove")),
                cpp_bool(named.get("animation")),
                cpp_optional(named.get("animationPath")),
                cpp_bool(named.get("texture")),
                cpp_suffix(named.get("mdlSuffix")),
                cpp_optional(named.get("archive")),
                cpp_optional(named.get("recolorName")),
                cpp_optional(named.get("animationShare")),
                cpp_bool(named.get("useLightSources")),
                cpp_bool(named.get("firstHunt")),
                cpp_bool(named.get("noUnderscore")),
            ]
            return string_value(positional[0]), (
                "ModelMetadata::from_recolors(" + ", ".join(args) + ")"
            )

        model_path = named.get("modelPath")
        if model_path is None:
            model_path = positional[1]
        animation_path = named.get("animationPath")
        collision_path = named.get("collisionPath")
        if animation_path is None and not named and len(positional) > 2:
            animation_path = positional[2]
        if collision_path is None and not named and len(positional) > 3:
            collision_path = positional[3]
        animation_share = named.get("animationShare")
        use_light_sources = cpp_bool(named.get("useLightSources"))
        return string_value(positional[0]), (
            "ModelMetadata(" + ", ".join([
                name,
                cpp_string(model_path),
                cpp_optional(animation_path),
                cpp_optional(collision_path),
                recolor_list(recolors_expression),
                cpp_optional(animation_share),
                use_light_sources,
            ]) + ")"
        )

    if "modelPath" in named or (not named and len(positional) >= 4):
        model_path = named.get("modelPath", positional[1])
        animation_path = named.get(
            "animationPath", positional[2] if not named and len(positional) > 2 else "null"
        )
        collision_path = named.get(
            "collisionPath", positional[3] if not named and len(positional) > 3 else "null"
        )
        return string_value(positional[0]), (
            "ModelMetadata::with_paths(" + ", ".join([
                name,
                cpp_string(model_path),
                cpp_optional(animation_path),
                cpp_optional(collision_path),
                cpp_bool(named.get("firstHunt")),
            ]) + ")"
        )

    if "remove" in named:
        return string_value(positional[0]), (
            "ModelMetadata::from_remove(" + ", ".join([
                name,
                cpp_string(named["remove"]),
                cpp_bool(named.get("animation"), True),
                cpp_optional(named.get("animationPath")),
                cpp_bool(named.get("collision")),
                cpp_bool(named.get("firstHunt")),
            ]) + ")"
        )

    return string_value(positional[0]), (
        "ModelMetadata(" + ", ".join([
            name,
            cpp_bool(named.get("animation"), True),
            cpp_bool(named.get("collision")),
            cpp_bool(named.get("texture")),
            cpp_optional(named.get("share")),
            cpp_suffix(named.get("mdlSuffix")),
            cpp_optional(named.get("archive")),
            cpp_optional(named.get("addToAnim")),
            cpp_bool(named.get("firstHunt")),
            cpp_optional(named.get("animationPath")),
            cpp_optional(named.get("extraCollision")),
        ]) + ")"
    )


def dictionary_entries(source: Path, dictionary: str) -> list[tuple[str, str]]:
    text = source.read_text(encoding="utf-8")
    marker = re.compile(
        r"public static readonly FrozenDictionary<string, ModelMetadata>\s+"
        + re.escape(dictionary) + r"\s*=\s*Frozen\.Create",
        re.MULTILINE,
    )
    match = marker.search(text)
    if match is None:
        raise ValueError(f"dictionary not found: {source}:{dictionary}")
    end = text.find("\n        public static readonly", match.end())
    if end < 0:
        end = len(text)
    block = text[match.end():end]
    entries: list[tuple[str, str]] = []
    cursor = 0
    while True:
        start = block.find("new ModelMetadata(", cursor)
        if start < 0:
            break
        opening = block.find("(", start)
        closing = matching(block, opening, "(", ")")
        key, value = model_call(block[start:closing + 1])
        entries.append((key, value))
        cursor = closing + 1
    if not entries:
        raise ValueError(f"no ModelMetadata entries: {source}:{dictionary}")
    return entries


def main(root: Path) -> None:
    source = root / "src" / "MphRead" / "Metadata" / "Metadata.cs"
    models = dictionary_entries(source, "ModelMetadata")
    first_hunt = dictionary_entries(source, "FirstHuntModels")
    if len(models) != 253 or len(first_hunt) != 62:
        raise ValueError(
            f"unexpected table size: ModelMetadata={len(models)}, "
            f"FirstHuntModels={len(first_hunt)}"
        )

    header = root / "src" / "MphRead.Native" / "Metadata" / "MetadataModels.hpp"
    header.write_text(
        """#pragma once

// Complete native counterpart of Metadata.cs ModelMetadata and
// FirstHuntModels.  The records are generated from the managed constructors;
// do not replace them with a reduced name-only catalogue.

#include \"MetadataClasses.hpp\"

#include <array>
#include <cstddef>
#include <span>
#include <string_view>

namespace fruityprime::metadata {

struct MetadataModelEntry {
    std::string_view Key;
    ModelMetadata Value;
};

inline constexpr std::size_t ModelMetadataCount = 253;
inline constexpr std::size_t FirstHuntModelsCount = 62;

extern const ModelMetadata DoubleDamageImg;
extern const std::array<MetadataModelEntry, ModelMetadataCount> ModelMetadataTable;
extern const std::array<MetadataModelEntry, FirstHuntModelsCount> FirstHuntModels;

[[nodiscard]] const ModelMetadata* get_model_by_name(
    std::string_view name, MetaDir dir = MetaDir::Models) noexcept;
[[nodiscard]] const ModelMetadata* get_first_hunt_model_by_name(
    std::string_view name) noexcept;
[[nodiscard]] const ModelMetadata* get_entity_by_path(
    std::string_view path) noexcept;

} // namespace fruityprime::metadata
""",
        encoding="utf-8",
        newline="\n",
    )

    lines = [
        "// Generated from src/MphRead/Metadata/Metadata.cs",
        "// by tools/generate-native-metadata-models.py. Do not hand-edit.",
        '#include "MetadataModels.hpp"',
        '#include "FrontendMeta.hpp"',
        "",
        "#include <optional>",
        "#include <string>",
        "",
        "namespace fruityprime::metadata {",
        "namespace {",
        "",
        "template <std::size_t N>",
        "const ModelMetadata* find_in(",
        "    const std::array<MetadataModelEntry, N>& records,",
        "    std::string_view name) noexcept {",
        "    for (const auto& entry : records) {",
        "        if (entry.Key == name) {",
        "            return &entry.Value;",
        "        }",
        "    }",
        "    return nullptr;",
        "}",
        "",
        "} // namespace",
        "",
        'const ModelMetadata DoubleDamageImg("doubleDamage_img", false, false, false,',
        '                                    std::nullopt, MdlSuffix::None,',
        '                                    std::optional<std::string>{"common"},',
        '                                    std::nullopt, false, std::nullopt,',
        '                                    std::nullopt);',
        "",
        "const std::array<MetadataModelEntry, ModelMetadataCount> ModelMetadataTable{{",
    ]
    for key, value in models:
        lines.append(f"    {{{cpp_string('"' + key + '"')}, {value}}},")
    lines += [
        "}};",
        "",
        "const std::array<MetadataModelEntry, FirstHuntModelsCount> FirstHuntModels{{",
    ]
    for key, value in first_hunt:
        lines.append(f"    {{{cpp_string('"' + key + '"')}, {value}}},")
    lines += [
        "}};",
        "",
        "const ModelMetadata* get_model_by_name(std::string_view name,",
        "                                            MetaDir dir) noexcept {",
        '    if (name == "doubleDamage_img") {',
        "        return &DoubleDamageImg;",
        "    }",
        '    if (name == "ad2_dm2") {',
        "        return &Ad2Dm2;",
        "    }",
        "    if (dir == MetaDir::Logo) {",
        "        return find_logo_model(name);",
        "    }",
        "    if (dir == MetaDir::Multiplayer) {",
        "        return find_multiplayer_model(name);",
        "    }",
        "    if (dir == MetaDir::TouchToStart) {",
        "        return find_touchtostart_model(name);",
        "    }",
        "    if (dir == MetaDir::Hud) {",
        "        return find_hud_model(name);",
        "    }",
        "    if (dir != MetaDir::Models) {",
        "        return find_frontend_model(name);",
        "    }",
        "    return find_in(ModelMetadataTable, name);",
        "}",
        "",
        "const ModelMetadata* get_first_hunt_model_by_name(",
        "    std::string_view name) noexcept {",
        "    return find_in(FirstHuntModels, name);",
        "}",
        "",
        "const ModelMetadata* get_entity_by_path(std::string_view path) noexcept {",
        "    for (const auto& entry : ModelMetadataTable) {",
        "        if (entry.Value.ModelPath == path) {",
        "            return &entry.Value;",
        "        }",
        "    }",
        "    return nullptr;",
        "}",
        "",
        "} // namespace fruityprime::metadata",
        "",
    ]
    output = root / "src" / "MphRead.Native" / "Metadata" / "MetadataModels.generated.cpp"
    output.write_text("\n".join(lines), encoding="utf-8", newline="\n")
    print(f"generated {header} and {output} ({len(models)} + {len(first_hunt)} entries)")


if __name__ == "__main__":
    main(Path(sys.argv[1]) if len(sys.argv) > 1 else Path(__file__).resolve().parents[1])
