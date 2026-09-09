#!/usr/bin/env python3
"""Generate the native counterpart of Metadata/FrontendMeta.cs.

FrontendMeta.cs is intentionally a data file.  Keep its model dictionaries
generated from the managed source so a spelling or path change cannot leave a
second hand-maintained native table behind.
"""

from pathlib import Path
import re


ROOT = Path(__file__).resolve().parents[1]
SOURCE = ROOT / "src" / "MphRead" / "Metadata" / "FrontendMeta.cs"
HEADER = ROOT / "src" / "MphRead.Native" / "Metadata" / "FrontendMeta.hpp"
CPP = ROOT / "src" / "MphRead.Native" / "Metadata" / "FrontendMeta.generated.cpp"


def raw_literal(value: str) -> str:
    """Return a C++ literal preserving the managed path spelling."""
    if "\")" in value:
        raise ValueError(f"path needs a raw-string delimiter: {value!r}")
    return f'R"({value})"'


def model_expression(arguments: str) -> str:
    compact = " ".join(arguments.split())

    direct = re.fullmatch(
        r'"([^"]+)", @"([^"]+)", null, null', compact
    )
    if direct:
        name, model_path = direct.groups()
        return (
            f'ModelMetadata::with_paths("{name}", {raw_literal(model_path)}, '
            "std::nullopt, std::nullopt)"
        )

    texture = re.fullmatch(
        r'"([^"]+)", texturePath: @"([^"]+)", dir: MetaDir\.(\w+)',
        compact,
    )
    if texture:
        name, texture_path, directory = texture.groups()
        return (
            f'ModelMetadata("{name}", {raw_literal(texture_path)}, '
            f"MetaDir::{directory})"
        )

    directory = re.fullmatch(
        r'"([^"]+)", dir: MetaDir\.(\w+)(?:, anim: "([^"]+)")?',
        compact,
    )
    if directory:
        name, directory_name, animation = directory.groups()
        if animation is None:
            return f'ModelMetadata("{name}", MetaDir::{directory_name})'
        return (
            f'ModelMetadata("{name}", MetaDir::{directory_name}, '
            f'std::string{{"{animation}"}})'
        )

    raise ValueError(f"unrecognised FrontendMeta ModelMetadata call: {arguments}")


def parse_models(source: str) -> dict[str, list[tuple[str, str]]]:
    result: dict[str, list[tuple[str, str]]] = {}
    dictionary_pattern = re.compile(
        r"FrozenDictionary<string, ModelMetadata> (\w+).*?"
        r"=\s*Frozen\.Create.*?\[(.*?)\]\);",
        re.DOTALL,
    )
    entry_pattern = re.compile(
        r'new\s*\(\s*"([^"]+)"\s*,\s*new ModelMetadata\((.*?)\)\s*\)\s*,?',
        re.DOTALL,
    )
    for name, body in dictionary_pattern.findall(source):
        entries = entry_pattern.findall(body)
        if body.count("new ModelMetadata(") != len(entries):
            raise ValueError(f"not all {name} entries were parsed")
        result[name] = [(key, model_expression(arguments)) for key, arguments in entries]
    expected = {"HudModels", "TouchToStartModels", "MultiplayerModels", "LogoModels", "FrontendModels"}
    if set(result) != expected:
        raise ValueError(f"unexpected FrontendMeta dictionaries: {sorted(result)}")
    return result


def render_header(models: dict[str, list[tuple[str, str]]]) -> str:
    declarations = []
    for name, entries in models.items():
        declarations.append(
            f"inline constexpr std::size_t {name}Count = {len(entries)};\n"
            f"extern const std::array<FrontendModelEntry, {name}Count> {name};"
        )
    finders = []
    for name in models:
        stem = name.removesuffix("Models")
        finders.append(
            f"[[nodiscard]] const ModelMetadata* find_{stem.lower()}_model("
            "std::string_view name) noexcept;"
        )
    return """#pragma once

// Direct native counterpart of Metadata/FrontendMeta.cs.  The model records
// below are generated from that source; keep the generator and source in sync.

#include "MetadataClasses.hpp"
#include "Metadata/metadata_extra.hpp"
#include "Metadata/movie_files.hpp"

#include <array>
#include <cstddef>
#include <span>
#include <string_view>

namespace fruityprime::metadata {

struct FrontendModelEntry {
    std::string_view Key;
    ModelMetadata Value;
};

extern const ModelMetadata Ad2Dm2;

""" + "\n\n".join(declarations) + "\n\n" + "\n".join(finders) + """

} // namespace fruityprime::metadata
"""


def render_cpp(models: dict[str, list[tuple[str, str]]]) -> str:
    blocks = []
    for name, entries in models.items():
        rows = ",\n".join(f'    {{"{key}", {expression}}}' for key, expression in entries)
        blocks.append(
            f"const std::array<FrontendModelEntry, {name}Count> {name}{{{{\n"
            f"{rows}\n"
            "}};"
        )

    finders = []
    for name in models:
        stem = name.removesuffix("Models")
        finders.append(
            f"const ModelMetadata* find_{stem.lower()}_model(std::string_view name) noexcept {{\n"
            f"    for (const auto& entry : {name}) {{\n"
            "        if (entry.Key == name) {\n"
            "            return &entry.Value;\n"
            "        }\n"
            "    }\n"
            "    return nullptr;\n"
            "}"
        )

    return """// Generated from src/MphRead/Metadata/FrontendMeta.cs by
// tools/generate-native-frontend-meta.py. Do not hand-edit.
#include "FrontendMeta.hpp"

namespace fruityprime::metadata {

const ModelMetadata Ad2Dm2("ad2_dm2", MetaDir::Stage);

""" + "\n\n".join(blocks) + "\n\n" + "\n\n".join(finders) + """

} // namespace fruityprime::metadata
"""


def main() -> None:
    models = parse_models(SOURCE.read_text(encoding="utf-8"))
    HEADER.write_text(render_header(models), encoding="utf-8", newline="\n")
    CPP.write_text(render_cpp(models), encoding="utf-8", newline="\n")
    print("generated", sum(map(len, models.values())), "frontend model entries")


if __name__ == "__main__":
    main()
