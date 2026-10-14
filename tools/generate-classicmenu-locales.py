#!/usr/bin/env python3
"""Build-host locale compiler: validates the SSOT JSON and emits static C++ data."""
import argparse
from collections import Counter
import json
from pathlib import Path
import re
import sys

LOCALES = ("en", "ja", "fr")


def load(path):
    def parse_pairs(pairs):
        result = {}
        for key, value in pairs:
            if key in result:
                raise ValueError(f"{path}: duplicate ID: {key!r}")
            result[key] = value
        return result
    with path.open("r", encoding="utf-8") as file:
        entries = json.load(file, object_pairs_hook=parse_pairs)
    if not isinstance(entries, dict) or not entries:
        raise ValueError(f"{path}: expected nonempty JSON object")
    for key, value in entries.items():
        if not isinstance(key, str) or not key.startswith("menu.") or not isinstance(value, str) or not value:
            raise ValueError(f"{path}: invalid entry: {key!r}")
        if "\x00" in key or "\x00" in value:
            raise ValueError(f"{path}: NUL not permitted: {key!r}")
    return entries


def signature(value):
    return sorted(re.findall(r"%(?:[0-9]+\$)?[-+ #0]*[0-9]*(?:\.[0-9]+)?[diuoxXfFeEgGaAcsp%]|\{[a-zA-Z_][a-zA-Z_0-9]*\}", value))


def pool_for(values):
    pool, unique, refs = bytearray(), {}, []
    for value in values:
        data = value.encode("utf-8")
        if len(data) > 65535:
            raise ValueError("translation value is larger than uint16 length")
        if value not in unique:
            unique[value] = (len(pool), len(data))
            pool.extend(data)
            pool.append(0)
        refs.append(unique[value])
    return pool, refs


def literal(data):
    fragments, part = [], ""
    for byte in data:
        if byte == 0:
            token = r"\000"
        elif byte == 34:
            token = r'\"'
        elif byte == 92:
            token = r"\\"
        elif 32 <= byte < 127:
            token = chr(byte)
        else:
            token = "\\" + format(byte, "03o")
        if len(part) + len(token) > 100:
            fragments.append('    "' + part + '"')
            part = ""
        part += token
    if part:
        fragments.append('    "' + part + '"')
    return "\n".join(fragments)


def generate(folder):
    catalog = {language: load(folder / (language + ".json")) for language in LOCALES}
    en = catalog["en"]
    keys = sorted(en)
    if len(keys) >= 65535:
        raise ValueError("too many IDs for uint16 index")
    for language in LOCALES[1:]:
        missing = sorted(set(keys) - catalog[language].keys())
        extra = sorted(catalog[language].keys() - set(keys))
        if missing or extra:
            raise ValueError(f"{language}: missing={missing!r}, extra={extra!r}")
    collisions = [value for value, count in Counter(en.values()).items() if count > 1]
    if collisions:
        raise ValueError(f"English lookup ambiguity: {collisions!r}")
    for key in keys:
        placeholders = [signature(catalog[language][key]) for language in LOCALES]
        if any(item != placeholders[0] for item in placeholders[1:]):
            raise ValueError(f"placeholder mismatch for {key!r}: {placeholders!r}")

    ids, idrefs = pool_for(keys)
    pools, refs = {}, {}
    for language in LOCALES:
        pools[language], refs[language] = pool_for([catalog[language][key] for key in keys])
    by_english = sorted(range(len(keys)), key=lambda i: en[keys[i]].encode("utf-8"))
    rows = []
    for i in range(len(keys)):
        offsets = [idrefs[i][0]] + [refs[language][i][0] for language in LOCALES]
        sizes = [idrefs[i][1]] + [refs[language][i][1] for language in LOCALES]
        rows.append("        {{" + ", ".join(map(str, offsets)) + "}, {"
                    + ", ".join(map(str, sizes)) + "}},")
    return (
        "// Generated at build time from Languages/*.json. Do not edit.\n"
        "#pragma once\n"
        "#include <array>\n#include <cstdint>\n\n"
        "namespace MphRead::Mods::ClassicMenu::Generated\n{\n"
        "    struct Record final\n    {\n"
        "        std::array<std::uint32_t, 4> Offsets;\n"
        "        std::array<std::uint16_t, 4> Lengths;\n    };\n"
        "    inline constexpr char kIds[] =\n" + literal(ids) + ";\n"
        + "".join("    inline constexpr char k" + language.capitalize() + "[] =\n"
                  + literal(pools[language]) + ";\n" for language in LOCALES)
        + "    inline constexpr std::array<Record, " + str(len(keys)) + "> kRecords{{\n"
        + "\n".join(rows) + "\n    }};\n"
        + "    inline constexpr std::array<std::uint16_t, " + str(len(keys)) + "> kByEnglish{{\n"
        + "        " + ", ".join(map(str, by_english)) + "\n    }};\n"
        + "}\n"
    )


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("--languages", type=Path, required=True)
    parser.add_argument("--output", type=Path, required=True)
    args = parser.parse_args()
    try:
        result = generate(args.languages)
        args.output.parent.mkdir(parents=True, exist_ok=True)
        if not args.output.exists() or args.output.read_text(encoding="utf-8") != result:
            args.output.write_text(result, encoding="utf-8", newline="\n")
        print("ClassicMenu: AOT locale catalog generated and validated")
        return 0
    except (OSError, ValueError, UnicodeError) as error:
        print(f"ClassicMenu locale validation failed: {error}", file=sys.stderr)
        return 1


if __name__ == "__main__":
    raise SystemExit(main())
