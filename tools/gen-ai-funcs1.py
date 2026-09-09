#!/usr/bin/env python3
"""Transliterate PlayerAi's Func1_* behaviours from PlayerAi.cs.

Eighty-four of them, most one to three lines: set a queued entity lookup, set
or clear a flag, delegate to a helper.  They are the kind of thing that is
tedious to retype and easy to get subtly wrong -- one flag bit off and a bot
behaves almost right -- so they are converted rather than transcribed.

Anything the converter does not recognise is emitted as a body that says so,
with the managed source in a comment, rather than as a silent no-op.

    python tools/gen-ai-funcs1.py

Writes src/MphRead.Native/Entities/Players/ai_funcs1.generated.cpp.
"""

from __future__ import annotations

import pathlib
import re
import sys

ROOT = pathlib.Path(__file__).resolve().parent.parent
SOURCE = ROOT / "src" / "MphRead" / "Entities" / "Players" / "PlayerAi.cs"
OUTPUT = (ROOT / "src" / "MphRead.Native" / "Entities" / "Players"
          / "ai_funcs1.generated.cpp")

QUEUED = re.compile(
    r"^_queuedFindEntityAction\s*=\s*AiQueuedEnt\.Type(\d+);$")
FLAG_SET = re.compile(r"^Flags([234])\s*\|=\s*AiFlags\1\.(\w+);$")
FLAG_CLEAR = re.compile(r"^Flags([234])\s*&=\s*~AiFlags\1\.(\w+);$")
FIND_REF = re.compile(r"^FindEntityRef\(AiEntRefType\.Type(\d+)\);$")
FIELD_SET = re.compile(r"^_(\w+)\s*=\s*(-?\d+);$")
FIELD_COPY = re.compile(r"^_(\w+)\s*=\s*_(\w+);$")
# The two calls that dominate the behaviours: point the bot at an item,
# or remember an item spawn, in both cases through an entity reference
# slot the find pass fills in.
TARGET_ITEM = re.compile(
    r"^UpdateTargetItem\(_entityRefs\.Field(\d+)\);$")
SET_ENTITY = re.compile(r"^SetEntity\(_entityRefs\.Field(\d+)\);$")
FIND_AND_NULL = re.compile(r"^UpdateTargetItem\(null\);$")
# The inlined form of SetEntity, which several behaviours spell out:
# keep the spawn only when it still has an item on it.  A reference the
# find pass never filled in is -1, which takes the same branch as null.
SPAWN_TERNARY = re.compile(
    r"^_itemSpawnC4 = _entityRefs\.Field(\d+)\?\.Item != null"
    r" \? _entityRefs\.Field\1 : null;$")


def method_body(text: str, name: str) -> str | None:
    marker = "private void %s(" % name
    at = text.find(marker)
    if at < 0:
        return None
    start = text.index("{", at)
    depth = 0
    index = start
    while True:
        if text[index] == "{":
            depth += 1
        elif text[index] == "}":
            depth -= 1
            if depth == 0:
                break
        index += 1
    return text[start + 1:index]


def statements(body: str) -> list[str]:
    """Source lines with comments and blanks removed."""
    out = []
    for line in body.splitlines():
        line = line.split("//")[0].strip()
        if line:
            out.append(line)
    return out


# The bot's own fields, in the order the managed class spells them.  Anything
# not here is left unconverted rather than guessed at.
FIELDS = {
    "weapon1": "weapon1_",
    "weapon2": "weapon2_",
    "findWeaponIndex": "find_weapon_index_",
    "queuedFindEntityAction": "queued_find_entity_action_",
    "targetPlayer": "target_slot_",
    "field118": "field118_",
    "field102c": "field102c_",
    "field1030": "field1030_",
    "field102E": "field102e_",
    "field1034": "field1034_",
    "field116": "field116_",
    "field1020": "field1020_",
    "field1032": "field1032_",
    "field30": "field30_",
    "field9C": "field9c_",
    "field78": "field78_",
    "shotDelay": "shot_delay_",
    "forceDisable": "force_disable_",
    "nodeDataSetIndex": "node_data_set_index_",
    "nodeDataSelOff": "node_data_sel_off_",
    "nodeDataSelOn": "node_data_sel_on_",
    "targetHalfturret": "target_halfturret_",
    "targetDefense": "target_defense_",
    "targetDoor": "target_door_",
    "itemSpawnC4": "item_spawn_",
    "itemC8": "target_item_",
    "octolithFlagCC": "octolith_flag_cc_",
    "octolithFlagD4": "octolith_flag_d4_",
    "octolithFlagDC": "octolith_flag_dc_",
    "flagBaseD0": "flag_base_d0_",
    "flagBaseD8": "flag_base_d8_",
    "flagBaseE0": "flag_base_e0_",
    "node3C": "node3c_",
    "node40": "node40_",
    "node44": "node44_",
    "node48": "node48_",
    "touchAimX": "touch_aim_x_",
    "touchAimY": "touch_aim_y_",
    "hasTouch": "has_touch_",
    "framesWithTouch": "frames_with_touch_",
    "framesWithoutTouch": "frames_without_touch_",
    "buttonAimX": "button_aim_x_",
    "buttonAimY": "button_aim_y_",
}

BEAMS = {
    "PowerBeam": 0, "Missile": 1, "VoltDriver": 2, "Battlehammer": 3,
    "Imperialist": 4, "Judicator": 5, "Magmaul": 6, "ShockCoil": 7,
    "OmegaCannon": 8,
}

CALL = re.compile(r"^(\w+)\(\);$")
CHECK_BEAM = re.compile(r"^CheckBeam\(BeamType\.(\w+)\)$")
CHECK_CHARGE = re.compile(r"^CheckCharge\(GetBeamType\(_(\w+)\)\)$")
AVAILABLE = re.compile(
    r"^(!?)_?(?:player\.)?_?[Aa]vailableWeapons\[BeamType\.(\w+)\]$")
FIELD_EQUALS = re.compile(r"^_(\w+) == (-?\d+)$")


def convert_condition(text: str) -> str | None:
    """A boolean expression, or None when it is not one we understand."""
    text = text.strip()
    matched = CHECK_BEAM.match(text)
    if matched and matched.group(1) in BEAMS:
        return "check_beam(session, bot_slot, %d)" % BEAMS[matched.group(1)]
    matched = CHECK_CHARGE.match(text)
    if matched and matched.group(1) in FIELDS:
        return "check_charge(get_beam_type(static_cast<int>(%s)))" % (
            FIELDS[matched.group(1)])
    matched = AVAILABLE.match(text.replace("_player.", "_"))
    if matched and matched.group(2) in BEAMS:
        return "%ssession.inventory(bot_slot).available_weapons[%d]" % (
            matched.group(1), BEAMS[matched.group(2)])
    matched = FIELD_EQUALS.match(text)
    if matched and matched.group(1) in FIELDS:
        return "%s == %s" % (FIELDS[matched.group(1)],
                             matched.group(2))
    return None


def convert(lines: list[str],
            known: set[str]) -> tuple[list[str], bool]:
    """Convert a body, and say whether everything in it was understood."""
    out: list[str] = []
    understood = True
    indent = "    "
    for line in lines:
        if line == "{":
            out.append(indent + "{")
            indent += "    "
            continue
        if line == "}":
            indent = indent[:-4] or "    "
            out.append(indent + "}")
            continue
        if line.startswith("if (") or line.startswith("else if ("):
            head = "else if" if line.startswith("else") else "if"
            inner = line[line.index("(") + 1:line.rindex(")")]
            condition = convert_condition(inner)
            if condition is None:
                understood = False
                out.append(indent + "// not converted: %s" % line)
                continue
            out.append("%s%s (%s)" % (indent, head, condition))
            continue
        if line == "else":
            out.append(indent + "else")
            continue
        matched = QUEUED.match(line)
        if matched:
            out.append(indent + "queued_find_entity_action_ = "
                       "AiQueuedEnt::Type%s;" % matched.group(1))
            continue
        matched = FLAG_SET.match(line)
        if matched:
            out.append(indent + "flags%s_ = flags%s_ | AiFlags%s::%s;"
                       % (matched.group(1), matched.group(1),
                          matched.group(1), matched.group(2)))
            continue
        matched = FLAG_CLEAR.match(line)
        if matched:
            out.append(indent + "flags%s_ = without(flags%s_, AiFlags%s::%s);"
                       % (matched.group(1), matched.group(1),
                          matched.group(1), matched.group(2)))
            continue
        matched = FIND_REF.match(line)
        if matched:
            out.append(indent + "find_entity_ref(AiEntRefType::Type%s);"
                       % matched.group(1))
            continue
        matched = TARGET_ITEM.match(line)
        if matched:
            out.append(indent + "update_target_item(session,")
            out.append(indent + "                   entity_refs_.entity(%s));"
                       % matched.group(1))
            continue
        if FIND_AND_NULL.match(line):
            out.append(indent + "update_target_item(session, -1);")
            continue
        matched = SET_ENTITY.match(line)
        if matched:
            out.append(indent + "set_item_spawn(entity_refs_.entity(%s));"
                       % matched.group(1))
            continue
        matched = SPAWN_TERNARY.match(line)
        if matched:
            out.append(indent + "set_item_spawn(entity_refs_.entity(%s));"
                       % matched.group(1))
            continue
        if line == "return;":
            out.append(indent + "return;")
            continue
        matched = FIELD_SET.match(line)
        if matched and matched.group(1) in FIELDS:
            out.append(indent + "%s = %s;" % (FIELDS[matched.group(1)],
                                              matched.group(2)))
            continue
        matched = FIELD_COPY.match(line)
        if (matched and matched.group(1) in FIELDS
                and matched.group(2) in FIELDS):
            out.append(indent + "%s = %s;"
                       % (FIELDS[matched.group(1)],
                          FIELDS[matched.group(2)]))
            continue
        matched = CALL.match(line)
        if matched and matched.group(1) in known:
            member = matched.group(1)
            member = member[0].lower() + member[1:]
            out.append(indent + "%s(session, bot_slot);" % member)
            continue
        understood = False
        out.append(indent + "// not converted: %s" % line)
    # A body whose braces did not balance is not understood.
    if indent != "    ":
        understood = False
    return out, understood


def main() -> int:
    text = SOURCE.read_text(encoding="utf-8")
    names = []
    seen = set()
    for match in re.finditer(r"private void (Func1_\w+)\(", text):
        if match.group(1) not in seen:
            seen.add(match.group(1))
            names.append(match.group(1))

    lines = [
        "// Generated by tools/gen-ai-funcs1.py from",
        "// src/MphRead/Entities/Players/PlayerAi.cs.  Do not edit by hand.",
        "//",
        "// PlayerAi's Func1_* behaviours, which ExecuteFuncs1 dispatches to.",
        "// A body the converter did not recognise says so where it stands,",
        "// rather than compiling to a bot that quietly does nothing.",
        '#include "Entities/Players/PlayerAi.hpp"',
        "",
        "namespace fruityprime::players {",
        "namespace {",
        "",
        "template <typename Flags>",
        "[[nodiscard]] constexpr Flags without(Flags value,",
        "                                      Flags bit) noexcept {",
        "    return static_cast<Flags>(",
        "        static_cast<std::uint32_t>(value)",
        "        & ~static_cast<std::uint32_t>(bit));",
        "}",
        "",
        "} // namespace",
        "",
    ]

    converted = 0
    partial = 0
    for name in names:
        body = method_body(text, name)
        if body is None:
            continue
        code, understood = convert(statements(body), set(names))
        if understood:
            converted += 1
        else:
            partial += 1
        member = name[0].lower() + name[1:]
        lines.append("void PlayerAiData::%s(" % member)
        lines.append("    const gameplay::Session& session,")
        lines.append("    std::uint8_t bot_slot) noexcept {")
        lines.append("    static_cast<void>(session);")
        lines.append("    static_cast<void>(bot_slot);")
        if not understood:
            lines.append("    // NOT PORTED.  The converter did not"
                         " understand every line of this body, and a")
            lines.append("    // half-converted behaviour would run the"
                         " parts it did understand without")
            lines.append("    // the parts it did not.  The managed"
                         " source follows verbatim.")
            for source in statements(body):
                lines.append("    // %s" % source)
        else:
            lines.extend(code if code
                         else ["    // the managed body is empty"])
        lines.append("}")
        lines.append("")
    # gen-ai-dispatch.py numbers behaviours by the sorted order of the
    # distinct names it found, with 0 meaning "runs nothing".  The switch
    # here has to agree with that or every bot runs the wrong behaviour.
    ordered = sorted(names)
    lines.append("// Behaviour index to behaviour, numbered the way")
    lines.append("// tools/gen-ai-dispatch.py numbers them.")
    lines.append("void PlayerAiData::dispatch_funcs1(")
    lines.append("    const gameplay::Session& session,")
    lines.append("    std::uint8_t bot_slot,")
    lines.append("    std::uint8_t behavior) noexcept {")
    lines.append("    switch (behavior) {")
    for position, name in enumerate(ordered):
        member = name[0].lower() + name[1:]
        lines.append("    case %d: %s(session, bot_slot); break;"
                     % (position + 1, member))
    lines.append("    default: break;  // 0 runs nothing")
    lines.append("    }")
    lines.append("}")
    lines.append("")
    lines += ["} // namespace fruityprime::players", ""]

    OUTPUT.write_text("\n".join(lines), encoding="utf-8", newline="\r\n")
    print("%d behaviours: %d fully converted, %d with unconverted lines"
          % (len(names), converted, partial))
    # The declarations the header needs, so they can be pasted in one go.
    decls = ROOT / "tools" / "ai_funcs1.decl.txt"
    decls.write_text(
        "\n".join(
            "    void %s(const gameplay::Session& session,\n"
            "            std::uint8_t bot_slot) noexcept;"
            % (name[0].lower() + name[1:])
            for name in names),
        encoding="utf-8", newline="\r\n")
    print("declarations written to %s" % decls.relative_to(ROOT))
    return 0


if __name__ == "__main__":
    sys.exit(main())
