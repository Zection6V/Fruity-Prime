"""Generate the native MemoryClasses mirror from the managed source.

MemoryClasses.cs is a cartridge-layout catalogue rather than ordinary game
logic: the names, widths, offsets, and embedded views are the specification.
Keeping this small generator beside the source makes regeneration mechanical
when the managed catalogue changes and leaves the hand-written native bits
limited to the few computed AI properties.
"""

from __future__ import annotations

import argparse
import re
from pathlib import Path


ROOT = Path(__file__).resolve().parents[1]
SOURCE = ROOT / "src" / "MphRead" / "MemoryClasses.cs"
HEADER = ROOT / "src" / "MphRead.Native" / "include" / "fruity_prime" / "memory_classes.hpp"
CPP = ROOT / "src" / "MphRead.Native" / "MemoryClasses.cpp"


SCALAR_CPP = {
    "sbyte": "std::int8_t",
    "byte": "std::uint8_t",
    "short": "std::int16_t",
    "ushort": "std::uint16_t",
    "int": "std::int32_t",
    "uint": "std::uint32_t",
    "bool": "bool",
    "IntPtr": "std::uint32_t",
    "Vector3": "formats::Vector3",
    "Vector4": "formats::Vector4",
    "Matrix4x3": "formats::Matrix4x3",
    "ColorRgb": "formats::ColorRgb",
    "EntityType": "formats::EntityType",
    "EnemyType": "formats::EnemyType",
    "BeamType": "formats::BeamType",
    "DoorType": "formats::DoorType",
    "ItemType": "formats::ItemType",
    "VolumeType": "formats::VolumeType",
    "FadeType": "formats::FadeType",
    "Hunter": "formats::Hunter",
    "Message": "formats::Message",
    "EquipFlags": "formats::EquipFlags",
    "PlatformFlags": "formats::PlatformFlags",
    "PlatStateFlags": "formats::PlatStateFlags",
    "PlatAnimFlags": "formats::PlatAnimFlags",
    "PlatformState": "formats::PlatformState",
    "SpawnerFlags": "formats::SpawnerFlags",
    "TriggerFlags": "formats::TriggerFlags",
    "BeamFlags": "formats::BeamFlags",
    "AiFlags2": "::fruityprime::players::AiFlags2",
    "AiFlags3": "::fruityprime::players::AiFlags3",
    "AiFlags4": "::fruityprime::players::AiFlags4",
    "GameMode": "::fruityprime::GameMode",
}

# These managed values are stored as their underlying integer width in the
# cartridge.  The generated accessors must restore the enum type on reads;
# C++ does not perform the implicit integer-to-enum conversion that C# does.
ENUM_TYPES = {
    "EntityType",
    "EnemyType",
    "BeamType",
    "DoorType",
    "ItemType",
    "VolumeType",
    "FadeType",
    "Hunter",
    "Message",
    "EquipFlags",
    "PlatformFlags",
    "PlatStateFlags",
    "PlatAnimFlags",
    "PlatformState",
    "SpawnerFlags",
    "TriggerFlags",
    "BeamFlags",
    "AiFlags2",
    "AiFlags3",
    "AiFlags4",
    "GameMode",
}

CPP_KEYWORDS = {
    "alignas", "alignof", "and", "and_eq", "asm", "auto", "bitand",
    "bitor", "bool", "break", "case", "catch", "char", "char8_t",
    "char16_t", "char32_t", "class", "compl", "concept", "const",
    "consteval", "constexpr", "constinit", "const_cast", "continue",
    "co_await", "co_return", "co_yield", "decltype", "default", "delete",
    "do", "double", "dynamic_cast", "else", "enum", "explicit", "export",
    "extern", "false", "float", "for", "friend", "goto", "if", "inline",
    "int", "long", "mutable", "namespace", "new", "noexcept", "not",
    "not_eq", "nullptr", "operator", "or", "or_eq", "private", "protected",
    "public", "reflexpr", "register", "reinterpret_cast", "requires",
    "return", "short", "signed", "sizeof", "static", "static_assert",
    "static_cast", "struct", "switch", "template", "this", "thread_local",
    "throw", "true", "try", "typedef", "typeid", "typename", "union",
    "unsigned", "using", "virtual", "void", "volatile", "wchar_t", "while",
    "xor", "xor_eq",
}


READ_METHODS = {
    "SByte": ("read_i8", "std::int8_t"),
    "Byte": ("read_u8", "std::uint8_t"),
    "Int16": ("read_i16", "std::int16_t"),
    "UInt16": ("read_u16", "std::uint16_t"),
    "Int32": ("read_i32", "std::int32_t"),
    "UInt32": ("read_u32", "std::uint32_t"),
    "Pointer": ("read_pointer", "std::uint32_t"),
    "Vec3": ("read_vec3", "formats::Vector3"),
    "Vec4": ("read_vec4", "formats::Vector4"),
    "Mtx43": ("read_mtx43", "formats::Matrix4x3"),
    "Color3": ("read_color3", "formats::ColorRgb"),
}

WRITE_METHODS = {
    "SByte": ("write_i8", "std::int8_t"),
    "Byte": ("write_u8", "std::uint8_t"),
    "Int16": ("write_i16", "std::int16_t"),
    "UInt16": ("write_u16", "std::uint16_t"),
    "Int32": ("write_i32", "std::int32_t"),
    "UInt32": ("write_u32", "std::uint32_t"),
    "Pointer": ("write_pointer", "std::uint32_t"),
    "Vec3": ("write_vec3", "formats::Vector3"),
    "Vec4": ("write_vec4", "formats::Vector4"),
    "Mtx43": ("write_mtx43", "formats::Matrix4x3"),
    "Color3": ("write_color3", "formats::ColorRgb"),
}


def matching_brace(text: str, opening: int) -> int:
    depth = 0
    for index in range(opening, len(text)):
        if text[index] == "{":
            depth += 1
        elif text[index] == "}":
            depth -= 1
            if depth == 0:
                return index
    raise ValueError("unbalanced brace")


def split_top_level(text: str) -> list[str]:
    parts: list[str] = []
    start = 0
    depth = 0
    for index, char in enumerate(text):
        # Constructor arguments contain C# lambdas (`=>`).  Treating the
        # arrow's `>` as a closing generic would corrupt the final factory
        # argument, so only track call/indexing delimiters here.
        if char in "([":
            depth += 1
        elif char in ")]":
            depth -= 1
        elif char == "," and depth == 0:
            parts.append(text[start:index].strip())
            start = index + 1
    parts.append(text[start:].strip())
    return parts if parts != [""] else []


def snake_case(name: str) -> str:
    result = re.sub(r"(?<!^)(?=[A-Z])", "_", name).lower()
    return f"{result}_" if result in CPP_KEYWORDS else result


def strip_nullable(type_name: str) -> tuple[str, bool]:
    type_name = type_name.strip()
    if type_name.endswith("?"):
        return type_name[:-1], True
    return type_name, False


def strip_generic(type_name: str) -> str:
    return re.sub(r"\s+", "", type_name)


def cpp_type(type_name: str, class_names: set[str]) -> str:
    clean, _ = strip_nullable(strip_generic(type_name))
    if clean in SCALAR_CPP:
        return SCALAR_CPP[clean]
    if clean == "ByteArray":
        return "ByteArray"
    if clean == "Int16Array":
        return "Int16Array"
    if clean == "UInt16Array":
        return "UInt16Array"
    if clean == "Int32Array":
        return "Int32Array"
    if clean == "UInt32Array":
        return "UInt32Array"
    if clean == "IntPtrArray":
        return "IntPtrArray"
    if clean == "U32EnumArray<Message>":
        return "U32EnumArray<formats::Message>"
    match = re.fullmatch(r"StructArray<(.+)>", clean)
    if match:
        return f"StructArray<{cpp_type(match.group(1), class_names)}>"
    if clean in class_names:
        # A generated property can have the same spelling as its class (for
        # example CObject::Model() or AIContext::AIData1()).  Fully qualify
        # embedded view types so those member names cannot hide the type in a
        # class declaration or out-of-line definition.
        return f"::fruityprime::memory::{clean}"
    raise ValueError(f"unmapped MemoryClasses type: {type_name}")


def class_records(source: str) -> list[dict]:
    records: list[dict] = []
    class_re = re.compile(
        r"^\s*public class (\w+)(?:\s*:\s*(\w+))?", re.MULTILINE
    )
    for match in class_re.finditer(source):
        opening = source.find("{", match.end())
        closing = matching_brace(source, opening)
        body = source[opening + 1 : closing]
        records.append(
            {
                "name": match.group(1),
                "base": match.group(2) or "MemoryClass",
                "body": body,
                "line": source.count("\n", 0, match.start()) + 1,
            }
        )
    return records


def parse_constants(body: str) -> dict[str, int]:
    constants: dict[str, int] = {}
    for match in re.finditer(
        r"private const int (\w+)\s*=\s*(0x[0-9A-Fa-f]+|\d+)\s*;", body
    ):
        constants[match.group(1)] = int(match.group(2), 0)
    return constants


def parse_properties(body: str) -> list[dict]:
    lines = body.splitlines(keepends=True)
    properties: list[dict] = []
    index = 0
    consumed = 0
    while index < len(lines):
        line = lines[index]
        match = re.match(
            r"\s*public\s+([\w<>?,]+)\s+(\w+)\s*(.*)$", line.rstrip("\r\n")
        )
        # A method has its parameter list before the opening brace.  Getter
        # expressions may themselves contain casts and calls, so do not reject
        # those parentheses.
        if not match or "(" in match.group(3).split("{", 1)[0]:
            consumed += len(line)
            index += 1
            continue
        type_name, name, tail = match.groups()
        if "{" in tail:
            opening = line.find("{", line.find(name))
            text = line[opening:]
            end_index = index
        elif index + 1 < len(lines) and "{" in lines[index + 1]:
            opening = len(line) + lines[index + 1].find("{")
            text = lines[index + 1][lines[index + 1].find("{") :]
            end_index = index + 1
        else:
            consumed += len(line)
            index += 1
            continue
        depth = text.count("{") - text.count("}")
        while depth > 0:
            end_index += 1
            if end_index >= len(lines):
                raise ValueError(f"unbalanced property {name}")
            text += lines[end_index]
            depth += lines[end_index].count("{") - lines[end_index].count("}")
        property_body = text[text.find("{") + 1 : text.rfind("}")]
        properties.append(
            {
                "type": type_name,
                "name": name,
                "body": property_body.strip(),
                "line": body.count("\n", 0, consumed) + 1,
            }
        )
        consumed += sum(len(lines[item]) for item in range(index, end_index + 1))
        index = end_index + 1
    return properties


def parse_constructor(body: str, class_name: str) -> str:
    match = re.search(
        rf"public\s+{class_name}\(Memory memory,\s*(?:int|IntPtr) address\)\s*:\s*base\(memory, address\)",
        body,
    )
    if not match:
        raise ValueError(f"constructor not found for {class_name}")
    opening = body.find("{", match.end())
    closing = matching_brace(body, opening)
    return body[opening + 1 : closing]


def parse_new_assignments(constructor: str) -> list[dict]:
    result: list[dict] = []
    assignment = re.compile(r"(\w+)\s*=\s*new\s+([\w]+(?:<[^;\n]+?>)?)\s*\(")
    for match in assignment.finditer(constructor):
        opening = constructor.find("(", match.start())
        closing = matching_paren(constructor, opening)
        result.append(
            {
                "property": match.group(1),
                "type": strip_generic(match.group(2)),
                "args": split_top_level(constructor[opening + 1 : closing]),
            }
        )
    return result


def matching_paren(text: str, opening: int) -> int:
    depth = 0
    for index in range(opening, len(text)):
        if text[index] == "(":
            depth += 1
        elif text[index] == ")":
            depth -= 1
            if depth == 0:
                return index
    raise ValueError("unbalanced parenthesis")


def read_write(prop: dict) -> tuple[str, str, str] | None:
    get_match = re.search(r"Read(\w+)\(([^)]*)\)", prop["body"])
    set_match = re.search(r"Write(\w+)\(([^,]+),", prop["body"])
    if not get_match or not set_match:
        return None
    if get_match.group(1) not in READ_METHODS or set_match.group(1) not in WRITE_METHODS:
        return None
    offset = get_match.group(2).strip()
    if offset != set_match.group(2).strip():
        # CPlayer.AggroCount addresses the AI sidecar through AiDataPtr rather
        # than through this object's fixed offset.  It is emitted as a
        # hand-written computed property below.
        return None
    return get_match.group(1), set_match.group(1), offset


def offset_value(offset: str, constants: dict[str, int]) -> int:
    if offset not in constants:
        raise ValueError(f"unknown MemoryClasses offset {offset}")
    return constants[offset]


def scalar_property(prop: dict, constants: dict[str, int]) -> dict | None:
    rw = read_write(prop)
    if rw is None:
        return None
    read_kind, write_kind, offset_ref = rw
    return {
        "read_kind": read_kind,
        "write_kind": write_kind,
        "offset": offset_value(offset_ref, constants),
    }


def array_properties(records: list[dict], class_names: set[str]) -> set[str]:
    result: set[str] = set()
    for record in records:
        for prop in record["properties"]:
            clean, _ = strip_nullable(prop["type"])
            if (
                clean.endswith("Array")
                or clean.startswith("StructArray<")
                or clean.startswith("U32EnumArray<")
            ):
                result.add(prop["name"])
    return result


def raw_type(kind: str) -> str:
    return READ_METHODS[kind][1]


def emit_scalar(prop: dict, info: dict, class_names: set[str]) -> list[str]:
    typ = cpp_type(prop["type"], class_names)
    name = prop["name"]
    snake = snake_case(name)
    offset = f"0x{info['offset']:X}"
    read_method = READ_METHODS[info["read_kind"]][0]
    write_method = WRITE_METHODS[info["write_kind"]][0]
    if prop["type"] == "bool":
        getter = f"return {read_method}({offset}) != 0;"
        setter = f"{write_method}({offset}, value ? 1 : 0);"
    elif info["read_kind"] in {"Vec3", "Vec4", "Mtx43", "Color3"}:
        getter = f"return {read_method}({offset});"
        setter = f"{write_method}({offset}, value);"
    elif prop["type"] in ENUM_TYPES:
        getter = f"return static_cast<{typ}>({read_method}({offset}));"
        setter = f"{write_method}({offset}, static_cast<{raw_type(info['write_kind'])}>(value));"
    else:
        getter = f"return {read_method}({offset});"
        setter = f"{write_method}({offset}, static_cast<{raw_type(info['write_kind'])}>(value));"
    return [
        f"    [[nodiscard]] {typ} {name}() const {{ {getter} }}",
        f"    void {name}({typ} value) {{ {setter} }}",
        f"    [[nodiscard]] {typ} {snake}() const {{ return {name}(); }}",
        f"    void {snake}({typ} value) {{ {name}(value); }}",
    ]


def is_array_type(type_name: str) -> bool:
    clean, _ = strip_nullable(type_name)
    return (
        clean.endswith("Array")
        or clean.startswith("StructArray<")
        or clean.startswith("U32EnumArray<")
    )


def emit_header(records: list[dict]) -> str:
    class_names = {record["name"] for record in records}
    lines = [
        "#pragma once",
        "",
        '#include "Formats/enum_tables.hpp"',
        '#include "Mods/Network/map_rotation.hpp"',
        '#include "Memory.hpp"',
        '#include "Formats/Types.hpp"',
        '#include "Entities/Players/PlayerAi.hpp"',
        "",
        "#include <array>",
        "#include <cstddef>",
        "#include <cstdint>",
        "#include <cmath>",
        "#include <limits>",
        "#include <memory>",
        "#include <stdexcept>",
        "#include <string>",
        "#include <string_view>",
        "#include <type_traits>",
        "#include <utility>",
        "#include <vector>",
        "",
        "namespace fruityprime::memory {",
        "",
        "struct FieldInfo {",
        "    std::string name;",
        "    std::size_t offset = 0;",
        "    std::size_t size = 0;",
        "};",
        "",
        "class Layout {",
        "public:",
        "    [[nodiscard]] bool add(std::string name, std::size_t offset,",
        "                          std::size_t size);",
        "    [[nodiscard]] const FieldInfo* find(std::string_view name) const noexcept;",
        "    [[nodiscard]] const std::vector<FieldInfo>& fields() const noexcept",
        "        { return fields_; }",
        "",
        "private:",
        "    std::vector<FieldInfo> fields_;",
        "};",
        "",
        "class Object {",
        "public:",
        "    Object(Buffer& buffer, const Layout& layout,",
        "           std::size_t base_offset = 0) noexcept",
        "        : buffer_(&buffer), layout_(&layout), base_offset_(base_offset) {}",
        "",
        "    [[nodiscard]] std::uint32_t read_u32(std::string_view field) const;",
        "    void write_u32(std::string_view field, std::uint32_t value);",
        "    [[nodiscard]] const Layout& layout() const noexcept { return *layout_; }",
        "    [[nodiscard]] std::size_t base_offset() const noexcept",
        "        { return base_offset_; }",
        "",
        "private:",
        "    [[nodiscard]] const FieldInfo& require_field(std::string_view field) const;",
        "    Buffer* buffer_ = nullptr;",
        "    const Layout* layout_ = nullptr;",
        "    std::size_t base_offset_ = 0;",
        "};",
        "",
        "// Native counterpart of MemoryClasses.cs::MemoryClass.  DS pointers",
        "// remain 32-bit values; base_offset is the checked Buffer coordinate.",
        "class MemoryClass {",
        "public:",
        "    MemoryClass(Buffer& buffer, std::size_t base_offset,",
        "                std::uint32_t address = 0) noexcept",
        "        : buffer_(&buffer), base_offset_(base_offset), address_(address) {}",
        "    MemoryClass(Buffer& buffer, std::uint32_t address) noexcept",
        "        : buffer_(&buffer), base_offset_(offset_for_address(buffer, address)),",
        "          address_(address) {}",
        "    virtual ~MemoryClass() = default;",
        "",
        "    [[nodiscard]] std::uint32_t address() const noexcept { return address_; }",
        "    [[nodiscard]] std::uint32_t Address() const noexcept { return address_; }",
        "    [[nodiscard]] std::size_t base_offset() const noexcept { return base_offset_; }",
        "",
        "    friend bool operator==(const MemoryClass& left,",
        "                           const MemoryClass& right) noexcept {",
        "        return left.address_ == right.address_;",
        "    }",
        "    friend bool operator!=(const MemoryClass& left,",
        "                           const MemoryClass& right) noexcept {",
        "        return !(left == right);",
        "    }",
        "",
        "    [[nodiscard]] std::int8_t read_i8(std::size_t offset = 0) const {",
        "        return static_cast<std::int8_t>(buffer().read_u8(at(offset)));",
        "    }",
        "    [[nodiscard]] std::uint8_t read_u8(std::size_t offset = 0) const {",
        "        return buffer().read_u8(at(offset));",
        "    }",
        "    [[nodiscard]] std::int16_t read_i16(std::size_t offset = 0) const {",
        "        return buffer().read_i16_le(at(offset));",
        "    }",
        "    [[nodiscard]] std::uint16_t read_u16(std::size_t offset = 0) const {",
        "        return buffer().read_u16_le(at(offset));",
        "    }",
        "    [[nodiscard]] std::int32_t read_i32(std::size_t offset = 0) const {",
        "        return buffer().read_i32_le(at(offset));",
        "    }",
        "    [[nodiscard]] std::uint32_t read_u32(std::size_t offset = 0) const {",
        "        return buffer().read_u32_le(at(offset));",
        "    }",
        "    [[nodiscard]] float read_f32(std::size_t offset = 0) const {",
        "        return buffer().read_f32_le(at(offset));",
        "    }",
        "",
        "    void write_i8(std::size_t offset, std::int8_t value) {",
        "        buffer().write_u8(at(offset), static_cast<std::uint8_t>(value));",
        "    }",
        "    void write_u8(std::size_t offset, std::uint8_t value) {",
        "        buffer().write_u8(at(offset), value);",
        "    }",
        "    void write_i16(std::size_t offset, std::int16_t value) {",
        "        buffer().write_u16_le(at(offset), static_cast<std::uint16_t>(value));",
        "    }",
        "    void write_u16(std::size_t offset, std::uint16_t value) {",
        "        buffer().write_u16_le(at(offset), value);",
        "    }",
        "    void write_i32(std::size_t offset, std::int32_t value) {",
        "        buffer().write_u32_le(at(offset), static_cast<std::uint32_t>(value));",
        "    }",
        "    void write_u32(std::size_t offset, std::uint32_t value) {",
        "        buffer().write_u32_le(at(offset), value);",
        "    }",
        "    void write_f32(std::size_t offset, float value) {",
        "        buffer().write_f32_le(at(offset), value);",
        "    }",
        "",
        "    [[nodiscard]] std::uint32_t read_pointer(std::size_t offset = 0) const {",
        "        return read_u32(offset);",
        "    }",
        "    void write_pointer(std::size_t offset, std::uint32_t value) {",
        "        write_u32(offset, value);",
        "    }",
        "    [[nodiscard]] formats::ColorRgb read_color3(std::size_t offset = 0) const {",
        "        return {read_u8(offset), read_u8(offset + 1), read_u8(offset + 2)};",
        "    }",
        "    void write_color3(std::size_t offset, formats::ColorRgb value) {",
        "        write_u8(offset, value.red); write_u8(offset + 1, value.green);",
        "        write_u8(offset + 2, value.blue);",
        "    }",
        "    [[nodiscard]] formats::Vector3 read_vec3(std::size_t offset = 0) const {",
        "        return {static_cast<float>(read_i32(offset)) / 4096.0F,",
        "                static_cast<float>(read_i32(offset + 4)) / 4096.0F,",
        "                static_cast<float>(read_i32(offset + 8)) / 4096.0F};",
        "    }",
        "    void write_vec3(std::size_t offset, formats::Vector3 value) {",
        "        write_i32(offset, fixed(value.x)); write_i32(offset + 4, fixed(value.y));",
        "        write_i32(offset + 8, fixed(value.z));",
        "    }",
        "    [[nodiscard]] formats::Vector4 read_vec4(std::size_t offset = 0) const {",
        "        return {static_cast<float>(read_i32(offset)) / 4096.0F,",
        "                static_cast<float>(read_i32(offset + 4)) / 4096.0F,",
        "                static_cast<float>(read_i32(offset + 8)) / 4096.0F,",
        "                static_cast<float>(read_i32(offset + 12)) / 4096.0F};",
        "    }",
        "    void write_vec4(std::size_t offset, formats::Vector4 value) {",
        "        write_i32(offset, fixed(value.x)); write_i32(offset + 4, fixed(value.y));",
        "        write_i32(offset + 8, fixed(value.z)); write_i32(offset + 12, fixed(value.w));",
        "    }",
        "    [[nodiscard]] formats::Matrix4x3 read_mtx43(std::size_t offset = 0) const {",
        "        const auto a = read_vec3(offset); const auto b = read_vec3(offset + 12);",
        "        const auto c = read_vec3(offset + 24); const auto d = read_vec3(offset + 36);",
        "        return {a.x, a.y, a.z, b.x, b.y, b.z, c.x, c.y, c.z, d.x, d.y, d.z};",
        "    }",
        "    void write_mtx43(std::size_t offset, formats::Matrix4x3 value) {",
        "        write_vec3(offset, {value.m11, value.m12, value.m13});",
        "        write_vec3(offset + 12, {value.m21, value.m22, value.m23});",
        "        write_vec3(offset + 24, {value.m31, value.m32, value.m33});",
        "        write_vec3(offset + 36, {value.m41, value.m42, value.m43});",
        "    }",
        "",
        "    static constexpr std::uint32_t AddressBase = 0x02000000U;",
        "    [[nodiscard]] static std::size_t offset_for_address(",
        "        const Buffer& buffer, std::uint32_t address) noexcept {",
        "        (void)buffer;",
        "        if (address >= AddressBase) return static_cast<std::size_t>(address - AddressBase);",
        "        return static_cast<std::size_t>(address);",
        "    }",
        "",
        "protected:",
        "    [[nodiscard]] Buffer& buffer() noexcept { return *buffer_; }",
        "    [[nodiscard]] const Buffer& buffer() const noexcept { return *buffer_; }",
        "    [[nodiscard]] std::size_t at(std::size_t offset) const noexcept {",
        "        return base_offset_ + offset;",
        "    }",
        "    [[nodiscard]] std::size_t offset_for_address(std::uint32_t address) const noexcept {",
        "        return offset_for_address(*buffer_, address);",
        "    }",
        "",
        "private:",
        "    [[nodiscard]] static std::int32_t fixed(float value) noexcept {",
        "        const double scaled = static_cast<double>(value) * 4096.0;",
        "        if (!std::isfinite(scaled)) return 0;",
        "        if (scaled <= static_cast<double>(std::numeric_limits<std::int32_t>::min()))",
        "            return std::numeric_limits<std::int32_t>::min();",
        "        if (scaled >= static_cast<double>(std::numeric_limits<std::int32_t>::max()))",
        "            return std::numeric_limits<std::int32_t>::max();",
        "        return static_cast<std::int32_t>(scaled);",
        "    }",
        "    Buffer* buffer_ = nullptr;",
        "    std::size_t base_offset_ = 0;",
        "    std::uint32_t address_ = 0;",
        "};",
        "",
        "// C# MemoryArray<T> equivalent.  Every element remains a live view of",
        "// the backing Buffer; arrays never copy cartridge memory.",
        "class MemoryArrayBase : public MemoryClass {",
        "public:",
        "    [[nodiscard]] std::size_t length() const noexcept { return length_; }",
        "    [[nodiscard]] std::size_t Length() const noexcept { return length_; }",
        "",
        "protected:",
        "    MemoryArrayBase(Buffer& buffer, std::size_t base_offset,",
        "                     std::size_t length, std::uint32_t address);",
        "    MemoryArrayBase(Buffer& buffer, std::uint32_t address,",
        "                     std::size_t length);",
        "    void require_index(std::size_t index) const;",
        "    [[nodiscard]] std::size_t element_offset(std::size_t index,",
        "                                             std::size_t width) const;",
        "",
        "private:",
        "    std::size_t length_ = 0;",
        "};",
        "",
        "template <typename T>",
        "class ScalarArray : public MemoryArrayBase {",
        "public:",
        "    ScalarArray(Buffer& buffer, std::size_t base_offset,",
        "                std::size_t length, std::uint32_t address = 0)",
        "        : MemoryArrayBase(buffer, base_offset, length, address) {}",
        "    ScalarArray(Buffer& buffer, std::uint32_t address,",
        "                std::size_t length)",
        "        : MemoryArrayBase(buffer, address, length) {}",
        "",
        "    [[nodiscard]] T get(std::size_t index) const {",
        "        require_index(index);",
        "        const auto offset = element_offset(index, sizeof(T));",
        "        if constexpr (std::is_same_v<T, std::int8_t>) return read_i8(offset);",
        "        else if constexpr (std::is_same_v<T, std::uint8_t>) return read_u8(offset);",
        "        else if constexpr (std::is_same_v<T, std::int16_t>) return read_i16(offset);",
        "        else if constexpr (std::is_same_v<T, std::uint16_t>) return read_u16(offset);",
        "        else if constexpr (std::is_same_v<T, std::int32_t>) return read_i32(offset);",
        "        else if constexpr (std::is_same_v<T, std::uint32_t>) return read_u32(offset);",
        "        else if constexpr (std::is_same_v<T, float>) return read_f32(offset);",
        "        else static_assert(std::is_same_v<T, void>, \"unsupported MemoryArray scalar\");",
        "    }",
        "",
        "    void set(std::size_t index, T value) {",
        "        require_index(index);",
        "        const auto offset = element_offset(index, sizeof(T));",
        "        if constexpr (std::is_same_v<T, std::int8_t>) write_i8(offset, value);",
        "        else if constexpr (std::is_same_v<T, std::uint8_t>) write_u8(offset, value);",
        "        else if constexpr (std::is_same_v<T, std::int16_t>) write_i16(offset, value);",
        "        else if constexpr (std::is_same_v<T, std::uint16_t>) write_u16(offset, value);",
        "        else if constexpr (std::is_same_v<T, std::int32_t>) write_i32(offset, value);",
        "        else if constexpr (std::is_same_v<T, std::uint32_t>) write_u32(offset, value);",
        "        else if constexpr (std::is_same_v<T, float>) write_f32(offset, value);",
        "        else static_assert(std::is_same_v<T, void>, \"unsupported MemoryArray scalar\");",
        "    }",
        "",
        "    [[nodiscard]] T operator[](std::size_t index) const { return get(index); }",
        "    void set_at(std::size_t index, T value) { set(index, value); }",
        "};",
        "",
        "using SByteArray = ScalarArray<std::int8_t>;",
        "using ByteArray = ScalarArray<std::uint8_t>;",
        "using Int16Array = ScalarArray<std::int16_t>;",
        "using UInt16Array = ScalarArray<std::uint16_t>;",
        "using Int32Array = ScalarArray<std::int32_t>;",
        "using UInt32Array = ScalarArray<std::uint32_t>;",
        "using IntPtrArray = ScalarArray<std::uint32_t>;",
        "",
        "template <typename Enum>",
        "class U32EnumArray : public MemoryArrayBase {",
        "public:",
        "    U32EnumArray(Buffer& buffer, std::size_t base_offset,",
        "                  std::size_t length, std::uint32_t address = 0)",
        "        : MemoryArrayBase(buffer, base_offset, length, address) {}",
        "    [[nodiscard]] Enum get(std::size_t index) const {",
        "        return static_cast<Enum>(read_u32(element_offset(index, 4)));",
        "    }",
        "    void set(std::size_t index, Enum value) {",
        "        write_u32(element_offset(index, 4), static_cast<std::uint32_t>(value));",
        "    }",
        "    [[nodiscard]] Enum operator[](std::size_t index) const { return get(index); }",
        "};",
        "",
        "template <typename T>",
        "class StructArray : public MemoryArrayBase {",
        "public:",
        "    StructArray(Buffer& buffer, std::size_t base_offset,",
        "                std::size_t length, std::size_t element_size,",
        "                std::uint32_t address = 0)",
        "        : MemoryArrayBase(buffer, base_offset, length, address) {",
        "        items_.reserve(length);",
        "        for (std::size_t i = 0; i < length; ++i) {",
        "            items_.push_back(std::make_unique<T>(",
        "                buffer, base_offset + i * element_size,",
        "                address + static_cast<std::uint32_t>(i * element_size)));",
        "        }",
        "    }",
        "    StructArray(Buffer& buffer, std::uint32_t address,",
        "                std::size_t length, std::size_t element_size)",
        "        : StructArray(buffer, MemoryClass::offset_for_address(buffer, address),",
        "                       length, element_size, address) {}",
        "",
        "    [[nodiscard]] T& get(std::size_t index) {",
        "        require_index(index);",
        "        return *items_[index];",
        "    }",
        "    [[nodiscard]] const T& get(std::size_t index) const {",
        "        require_index(index);",
        "        return *items_[index];",
        "    }",
        "    [[nodiscard]] T& operator[](std::size_t index) { return get(index); }",
        "    [[nodiscard]] const T& operator[](std::size_t index) const { return get(index); }",
        "    void set(std::size_t, const T&) {",
        "        throw std::logic_error(\"embedded MemoryClass writes are not supported\");",
        "    }",
        "",
        "private:",
        "    std::vector<std::unique_ptr<T>> items_;",
        "};",
        "",
        "// Forward declarations allow the managed source order to stay visible",
        "// even when a class contains an embedded view of a later class.",
    ]
    lines.extend(f"class {record['name']};" for record in records)
    lines.append("")

    for record in records:
        record["constants"] = parse_constants(record["body"])
        record["properties"] = parse_properties(record["body"])
        record["constructor"] = parse_constructor(record["body"], record["name"])
        record["assignments"] = parse_new_assignments(record["constructor"])

    for record in records:
        name = record["name"]
        base = record["base"]
        constants = record["constants"]
        properties = record["properties"]
        lines.extend([f"class {name} : public {base} {{", "public:"])
        lines.append(
            f"    {name}(Buffer& buffer, std::size_t base_offset, std::uint32_t address);"
        )
        lines.append(
            f"    {name}(Buffer& buffer, std::uint32_t address);"
        )
        lines.append(f"    ~{name}() override;")
        lines.append("")
        for prop in properties:
            type_name = prop["type"]
            clean_type, optional = strip_nullable(type_name)
            info = scalar_property(prop, constants)
            if info is not None:
                lines.extend(emit_scalar(prop, info, class_names))
                continue
            body = prop["body"]
            if prop["name"] in {"Slot1", "Slot2"} and "get; set;" in body:
                typ = cpp_type(type_name, class_names)
                snake = snake_case(prop["name"])
                lines.extend(
                    [
                        f"    [[nodiscard]] {typ} {prop['name']}() const noexcept {{ return {snake}_; }}",
                        f"    void {prop['name']}({typ} value) noexcept {{ {snake}_ = value; }}",
                        f"    [[nodiscard]] {typ} {snake}() const noexcept {{ return {prop['name']}(); }}",
                        f"    void {snake}({typ} value) noexcept {{ {prop['name']}(value); }}",
                    ]
                )
                continue
            if name == "AIAggro" and prop["name"].startswith("VarA"):
                ret_type = cpp_type(type_name, class_names)
                lines.append(f"    [[nodiscard]] {ret_type} {prop['name']}() const;")
                lines.append(f"    [[nodiscard]] {ret_type} {snake_case(prop['name'])}() const {{ return {prop['name']}(); }}")
                continue
            if name == "AIContext" and prop["name"] == "AIData1":
                lines.append("    [[nodiscard]] ::fruityprime::memory::AIData1* AIData1() noexcept;")
                lines.append("    [[nodiscard]] const ::fruityprime::memory::AIData1* AIData1() const noexcept;")
                lines.append("    [[nodiscard]] ::fruityprime::memory::AIData1* ai_data1() noexcept { return AIData1(); }")
                lines.append("    [[nodiscard]] const ::fruityprime::memory::AIData1* ai_data1() const noexcept { return AIData1(); }")
                continue
            if name == "CPlayer" and prop["name"] == "AggroCount":
                lines.append("    [[nodiscard]] std::uint32_t AggroCount() const;")
                lines.append("    void AggroCount(std::uint32_t value);")
                lines.append("    [[nodiscard]] std::uint32_t aggro_count() const { return AggroCount(); }")
                lines.append("    void aggro_count(std::uint32_t value) { AggroCount(value); }")
                continue
            if "get;" in body:
                typ = cpp_type(clean_type, class_names)
                if optional:
                    lines.append(f"    [[nodiscard]] {typ}* {prop['name']}() noexcept;")
                    lines.append(f"    [[nodiscard]] const {typ}* {prop['name']}() const noexcept;")
                else:
                    lines.append(f"    [[nodiscard]] {typ}& {prop['name']}() noexcept;")
                    lines.append(f"    [[nodiscard]] const {typ}& {prop['name']}() const noexcept;")
                if optional:
                    lines.append(f"    [[nodiscard]] {typ}* {snake_case(prop['name'])}() noexcept {{ return {prop['name']}(); }}")
                    lines.append(f"    [[nodiscard]] const {typ}* {snake_case(prop['name'])}() const noexcept {{ return {prop['name']}(); }}")
                else:
                    lines.append(f"    [[nodiscard]] {typ}& {snake_case(prop['name'])}() noexcept {{ return {prop['name']}(); }}")
                    lines.append(f"    [[nodiscard]] const {typ}& {snake_case(prop['name'])}() const noexcept {{ return {prop['name']}(); }}")
                continue
            raise ValueError(f"unhandled property {name}.{prop['name']}: {body}")

        if name == "AIAggro":
            lines.append("    void UpdateSlots(const std::array<::fruityprime::memory::CPlayer*, 4>& players);")
            lines.append("    void update_slots(const std::array<::fruityprime::memory::CPlayer*, 4>& players) { UpdateSlots(players); }")
        lines.append("")
        lines.append("private:")
        for constant, value in constants.items():
            lines.append(f"    static constexpr std::size_t {constant} = 0x{value:X};")
        for prop in properties:
            type_name = prop["type"]
            clean_type, optional = strip_nullable(type_name)
            if scalar_property(prop, constants) is not None:
                continue
            if name == "AIAggro" and prop["name"].startswith("VarA"):
                continue
            if name == "AIContext" and prop["name"] == "AIData1":
                continue
            if name == "CPlayer" and prop["name"] == "AggroCount":
                continue
            if prop["name"] in {"Slot1", "Slot2"} and "get; set;" in prop["body"]:
                lines.append(f"    std::int32_t {snake_case(prop['name'])}_ = -1;")
                continue
            if "get;" in prop["body"]:
                typ = cpp_type(clean_type, class_names)
                lines.append(f"    std::unique_ptr<{typ}> {snake_case(prop['name'])}_;")
        if name == "AIContext":
            lines.append("    std::uint32_t last_data1_ptr_ = 0;")
            lines.append("    std::unique_ptr<::fruityprime::memory::AIData1> data1_cache_; ")
        lines.append("};")
        lines.append("")

    lines.extend(["} // namespace fruityprime::memory", ""])
    return "\n".join(lines)


def parse_address(expr: str, constants: dict[str, int]) -> tuple[str, int | None]:
    expr = expr.strip()
    match = re.fullmatch(r"address\s*\+\s*(\w+)", expr)
    if match:
        return "relative", offset_value(match.group(1), constants)
    if expr == "address":
        return "relative", 0
    if expr == "AiDataPtr":
        return "pointer", None
    if re.fullmatch(r"\w+(?:Ptr|Reference)", expr):
        return "member_pointer", None
    if expr == "offset":
        return "offset_variable", None
    match = re.fullmatch(r"AiDataPtr\s*\+\s*new IntPtr\((0x[0-9A-Fa-f]+|\d+)\)", expr)
    if match:
        return "pointer_plus", int(match.group(1), 0)
    raise ValueError(f"unhandled constructor address expression: {expr}")


def emit_nested_init(record: dict, assignment: dict, class_names: set[str]) -> list[str]:
    prop = assignment["property"]
    args = assignment["args"]
    if not args or args[0] != "memory":
        raise ValueError(f"unexpected constructor args for {record['name']}.{prop}: {args}")
    address_expr = args[1]
    kind, value = parse_address(address_expr, record["constants"])
    type_expr = assignment["type"]
    clean_type, optional = strip_nullable(type_expr)
    target_type = cpp_type(clean_type, class_names)
    property_names = {property_info["name"] for property_info in record["properties"]}

    def value_expression(expression: str) -> str:
        expression = expression.strip()
        if expression in property_names:
            return f"{expression}()"
        return expression

    if target_type.startswith("StructArray<"):
        if len(args) < 4:
            raise ValueError(f"StructArray initializer is incomplete: {args}")
        length = value_expression(args[2])
        element_size = value_expression(args[3])
        constructor = f"std::make_unique<{target_type}>(buffer, {{base}}, {length}, {element_size}, {{address}})"
    elif target_type.endswith("Array") or target_type.startswith("U32EnumArray<"):
        length = value_expression(args[2])
        constructor = f"std::make_unique<{target_type}>(buffer, {{base}}, {length}, {{address}})"
    else:
        constructor = f"std::make_unique<{target_type}>(buffer, {{base}}, {{address}})"

    if kind == "relative":
        base = f"base_offset + 0x{value:X}"
        address = f"address + 0x{value:X}"
    elif kind == "pointer":
        base = "offset_for_address(AiData())"
        address = "AiData()"
    elif kind == "pointer_plus":
        base = f"offset_for_address(AiData() + 0x{value:X})"
        address = f"AiData() + 0x{value:X}"
    elif kind == "member_pointer":
        base = f"offset_for_address({address_expr}())"
        address = f"{address_expr}()"
    elif kind == "offset_variable":
        base = "dynamic_offset"
        address = "dynamic_address"
    else:
        raise AssertionError(kind)
    return [f"    {snake_case(prop)}_ = {constructor.format(base=base, address=address)};"]


def emit_cpp(records: list[dict]) -> str:
    class_names = {record["name"] for record in records}
    lines = [
        '#include "memory_classes.hpp"',
        "",
        "#include <algorithm>",
        "#include <limits>",
        "#include <stdexcept>",
        "",
        "namespace fruityprime::memory {",
        "",
        "bool Layout::add(std::string name, std::size_t offset, std::size_t size) {",
        "    if (name.empty() || size == 0 || find(name) != nullptr) return false;",
        "    fields_.push_back({std::move(name), offset, size});",
        "    return true;",
        "}",
        "",
        "const FieldInfo* Layout::find(std::string_view name) const noexcept {",
        "    const auto it = std::find_if(fields_.begin(), fields_.end(),",
        "        [name](const FieldInfo& field) { return field.name == name; });",
        "    return it == fields_.end() ? nullptr : &*it;",
        "}",
        "",
        "const FieldInfo& Object::require_field(std::string_view field) const {",
        "    const FieldInfo* info = layout_->find(field);",
        "    if (info == nullptr) throw std::out_of_range(\"unknown memory layout field\");",
        "    if (info->size < sizeof(std::uint32_t)",
        "        || !buffer_->contains(base_offset_ + info->offset, sizeof(std::uint32_t)))",
        "        throw std::out_of_range(\"memory layout field is outside the buffer\");",
        "    return *info;",
        "}",
        "",
        "std::uint32_t Object::read_u32(std::string_view field) const {",
        "    const auto& info = require_field(field);",
        "    return buffer_->read_u32_le(base_offset_ + info.offset);",
        "}",
        "",
        "void Object::write_u32(std::string_view field, std::uint32_t value) {",
        "    const auto& info = require_field(field);",
        "    buffer_->write_u32_le(base_offset_ + info.offset, value);",
        "}",
        "",
        "MemoryArrayBase::MemoryArrayBase(Buffer& buffer, std::size_t base_offset,",
        "                                   std::size_t length, std::uint32_t address)",
        "    : MemoryClass(buffer, base_offset, address), length_(length) {}",
        "",
        "MemoryArrayBase::MemoryArrayBase(Buffer& buffer, std::uint32_t address,",
        "                                   std::size_t length)",
        "    : MemoryClass(buffer, address), length_(length) {}",
        "",
        "void MemoryArrayBase::require_index(std::size_t index) const {",
        "    if (index >= length_) throw std::out_of_range(\"memory array index is outside the view\");",
        "}",
        "",
        "std::size_t MemoryArrayBase::element_offset(std::size_t index,",
        "                                              std::size_t width) const {",
        "    require_index(index);",
        "    if (index > (std::numeric_limits<std::size_t>::max() / width))",
        "        throw std::out_of_range(\"memory array offset overflow\");",
        "    return at(index * width);",
        "}",
        "",
    ]
    for record in records:
        name = record["name"]
        base = record["base"]
        lines.extend(
            [
                f"{name}::{name}(Buffer& buffer, std::size_t base_offset, std::uint32_t address)",
                f"    : {base}(buffer, base_offset, address) {{",
            ]
        )
        if name == "CPlayer":
            # All ordinary embedded views are generated from the managed
            # constructor below; the three pointer-dependent views need the
            # explicit C# null check and address-to-buffer translation.
            for assignment in record["assignments"]:
                prop = assignment["property"]
                if prop in {"AIContext", "AIAggro", "AiData"}:
                    continue
                lines.extend(emit_nested_init(record, assignment, class_names))
            lines.extend(
                [
                    "    if (AiDataPtr() != 0) {",
                    "        ai_data_ = std::make_unique<::fruityprime::memory::AiData>(",
                    "            buffer, offset_for_address(AiDataPtr()), AiDataPtr());",
                    f"        {snake_case('AIContext')}_ = std::make_unique<StructArray<::fruityprime::memory::AIContext>>(",
                    "            buffer, offset_for_address(AiDataPtr() + 0x2FC), 20,",
                    "            0xA8, AiDataPtr() + 0x2FC);",
                    f"        {snake_case('AIAggro')}_ = std::make_unique<StructArray<::fruityprime::memory::AIAggro>>(",
                    "            buffer, offset_for_address(AiDataPtr() + 0x1064), 25,",
                    "            0x10, AiDataPtr() + 0x1064);",
                    "    }",
                ]
            )
        elif name == "AIData1":
            for assignment in record["assignments"]:
                prop = assignment["property"]
                if prop == "Data1":
                    lines.extend(
                        [
                            "    if (Data1Ptr() != 0 && Data1Count() > 0) {",
                            "        data1_ = std::make_unique<StructArray<::fruityprime::memory::AIData1>>(",
                            "            buffer, offset_for_address(Data1Ptr()),",
                            "            static_cast<std::size_t>(Data1Count()), 0x24, Data1Ptr());",
                            "    }",
                        ]
                    )
                elif prop == "Data2":
                    lines.extend(
                        [
                            "    if (Data2Ptr() != 0 && Data2Count() > 0) {",
                            "        data2_ = std::make_unique<StructArray<::fruityprime::memory::AIData2>>(",
                            "            buffer, offset_for_address(Data2Ptr()),",
                            "            static_cast<std::size_t>(Data2Count()), 0x18, Data2Ptr());",
                            "    }",
                        ]
                    )
                else:
                    lines.extend(emit_nested_init(record, assignment, class_names))
        else:
            for assignment in record["assignments"]:
                lines.extend(emit_nested_init(record, assignment, class_names))
        lines.extend(["}", ""])

        # Direct-address overload mirrors the managed IntPtr constructor.
        lines.extend(
            [
                f"{name}::{name}(Buffer& buffer, std::uint32_t address)",
                f"    : {name}(buffer, offset_for_address(buffer, address), address) {{}}",
                "",
                f"{name}::~{name}() = default;",
                "",
            ]
        )

        for prop in record["properties"]:
            type_name = prop["type"]
            clean_type, optional = strip_nullable(type_name)
            info = scalar_property(prop, record["constants"])
            if info is not None:
                continue
            prop_name = prop["name"]
            body = prop["body"]
            if record["name"] == "AIAggro" and prop_name.startswith("VarA"):
                if prop_name in {"VarA2", "VarA9", "VarA3", "VarA4"}:
                    masks = {"VarA2": ("0xF", 0), "VarA9": ("0xF0", 4), "VarA3": ("0xF00", 8), "VarA4": ("0xF000", 12)}
                    mask, shift = masks[prop_name]
                    expression = f"static_cast<std::uint8_t>((Bits1() & {mask}) >> {shift})"
                elif prop_name == "VarA10":
                    expression = "static_cast<std::uint8_t>(Bits2() & 0xF)"
                else:
                    expression = "static_cast<std::uint16_t>((Bits2() & 0xFFF0) >> 4)"
                typ = cpp_type(type_name, class_names)
                lines.append(f"{typ} {record['name']}::{prop_name}() const {{ return {expression}; }}")
                lines.append("")
                continue
            if record["name"] == "AIContext" and prop_name == "AIData1":
                lines.extend(
                    [
                        "::fruityprime::memory::AIData1* AIContext::AIData1() noexcept {",
                        "    const auto pointer = CurData1Iter();",
                        "    if (pointer != last_data1_ptr_) {",
                        "        data1_cache_.reset();",
                        "        if (pointer != 0) data1_cache_ = std::make_unique<::fruityprime::memory::AIData1>(",
                        "            buffer(), offset_for_address(pointer), pointer);",
                        "        last_data1_ptr_ = pointer;",
                        "    }",
                        "    return data1_cache_.get();",
                        "}",
                        "",
                        "const ::fruityprime::memory::AIData1* AIContext::AIData1() const noexcept {",
                        "    return const_cast<AIContext*>(this)->AIData1();",
                        "}",
                        "",
                    ]
                )
                continue
            if record["name"] == "CPlayer" and prop_name == "AggroCount":
                lines.extend(
                    [
                        "std::uint32_t CPlayer::AggroCount() const {",
                        "    if (AiDataPtr() == 0) return 0;",
                        "    return buffer().read_u32_le(offset_for_address(AiDataPtr() + 0x1060));",
                        "}",
                        "",
                        "void CPlayer::AggroCount(std::uint32_t value) {",
                        "    if (AiDataPtr() == 0) return;",
                        "    buffer().write_u32_le(offset_for_address(AiDataPtr() + 0x1060), value);",
                        "}",
                        "",
                    ]
                )
                continue
            if prop_name in {"Slot1", "Slot2"} and "get; set;" in body:
                continue
            if "get;" not in body:
                continue
            typ = cpp_type(clean_type, class_names)
            member = snake_case(prop_name) + "_"
            if optional:
                lines.extend(
                    [
                        f"{typ}* {record['name']}::{prop_name}() noexcept {{ return {member}.get(); }}",
                        f"const {typ}* {record['name']}::{prop_name}() const noexcept {{ return {member}.get(); }}",
                        "",
                    ]
                )
            else:
                lines.extend(
                    [
                        f"{typ}& {record['name']}::{prop_name}() noexcept {{ return *{member}; }}",
                        f"const {typ}& {record['name']}::{prop_name}() const noexcept {{ return *{member}; }}",
                        "",
                    ]
                )

        if name == "AIAggro":
            lines.extend(
                [
                    "void AIAggro::UpdateSlots(const std::array<::fruityprime::memory::CPlayer*, 4>& players) {",
                    "    slot1_ = -1;",
                    "    slot2_ = -1;",
                    "    for (std::size_t index = 0; index < players.size(); ++index) {",
                    "        if (players[index] != nullptr && Player1() == players[index]->address())",
                    "            slot1_ = static_cast<std::int32_t>(index);",
                    "        if (players[index] != nullptr && Player2() == players[index]->address())",
                    "            slot2_ = static_cast<std::int32_t>(index);",
                    "    }",
                    "}",
                    "",
                ]
            )

    lines.extend(["} // namespace fruityprime::memory", ""])
    return "\n".join(lines)


def prepare_records(source: str) -> list[dict]:
    records = class_records(source)
    class_names = {record["name"] for record in records}
    for record in records:
        record["constants"] = parse_constants(record["body"])
        record["properties"] = parse_properties(record["body"])
        record["constructor"] = parse_constructor(record["body"], record["name"])
        record["assignments"] = parse_new_assignments(record["constructor"])
        for prop in record["properties"]:
            info = scalar_property(prop, record["constants"])
            if info is not None:
                continue
            clean, _ = strip_nullable(prop["type"])
            if prop["name"] in {"Slot1", "Slot2"} and "get; set;" in prop["body"]:
                continue
            if record["name"] == "AIAggro" and prop["name"].startswith("VarA"):
                continue
            if record["name"] == "AIContext" and prop["name"] == "AIData1":
                continue
            if record["name"] == "CPlayer" and prop["name"] == "AggroCount":
                continue
            if "get;" not in prop["body"]:
                raise ValueError(f"unhandled property {record['name']}.{prop['name']}: {prop['body']}")
            cpp_type(clean, class_names)
    return records


def main() -> None:
    parser = argparse.ArgumentParser()
    parser.add_argument("--check", action="store_true", help="validate without writing")
    args = parser.parse_args()
    records = prepare_records(SOURCE.read_text(encoding="utf-8-sig"))
    header = emit_header(records)
    cpp = emit_cpp(records)
    if args.check:
        if not HEADER.exists() or HEADER.read_text(encoding="utf-8") != header:
            raise SystemExit(f"generated header is stale: {HEADER}")
        if not CPP.exists() or CPP.read_text(encoding="utf-8") != cpp:
            raise SystemExit(f"generated implementation is stale: {CPP}")
        print(f"validated {len(records)} classes")
        print(f"validated {sum(len(r['properties']) for r in records)} properties")
        return
    HEADER.write_text(header, encoding="utf-8", newline="\n")
    CPP.write_text(cpp, encoding="utf-8", newline="\n")
    print(f"generated {HEADER}")
    print(f"generated {CPP}")
    print(f"classes={len(records)} properties={sum(len(r['properties']) for r in records)}")


if __name__ == "__main__":
    main()
