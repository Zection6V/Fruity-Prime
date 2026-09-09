#!/usr/bin/env python3
"""Shared C#-to-C++ transliteration for the PlayerAi behaviour families.

The Func1, Func2 and Func4 generators all convert the same shapes, so the
converter lives here rather than being copied three times -- copying it is
how the native tree ended up with three separate spellings of the same HUD
table.

Nothing here tries to be a C# compiler.  It recognises the statement forms
these behaviours actually use and refuses the rest, and a body it cannot
convert entirely is reported as unconverted rather than half-done: a
behaviour that runs the parts we understood without the parts we did not is
a bot doing something the game never does.
"""

from __future__ import annotations

import re

# The bot's own fields, managed name (without the leading underscore) to the
# native member.  Anything not here is left unconverted rather than guessed.
FIELDS = {
    "weapon1": "weapon1_",
    "weapon2": "weapon2_",
    "findWeaponIndex": "find_weapon_index_",
    "queuedFindEntityAction": "queued_find_entity_action_",
    "targetPlayer": "target_slot_",
    "field118": "field118_",
    "Field118": "field118_",
    "field102C": "field102c_",
    "field102E": "field102e_",
    "field1030": "field1030_",
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

BUTTONS = {"Up", "Down", "Left", "Right", "A", "B", "X", "Y", "L", "R",
           "Start", "Select"}

QUEUED = re.compile(r"^_queuedFindEntityAction\s*=\s*AiQueuedEnt\.Type(\d+);$")
FLAG_SET = re.compile(r"^Flags([234])\s*\|=\s*AiFlags\1\.(\w+);$")
FLAG_CLEAR = re.compile(r"^Flags([234])\s*&=\s*~AiFlags\1\.(\w+);$")
FIND_REF = re.compile(r"^FindEntityRef\(AiEntRefType\.Type(\d+)\);$")
FIELD_SET = re.compile(r"^_(\w+)\s*=\s*(-?\d+);$")
FIELD_COPY = re.compile(r"^_(\w+)\s*=\s*_(\w+);$")
TARGET_ITEM = re.compile(r"^UpdateTargetItem\(_entityRefs\.Field(\d+)\);$")
SET_ENTITY = re.compile(r"^SetEntity\(_entityRefs\.Field(\d+)\);$")
TARGET_NULL = re.compile(r"^UpdateTargetItem\(null\);$")
SPAWN_TERNARY = re.compile(
    r"^_itemSpawnC4 = _entityRefs\.Field(\d+)\?\.Item != null"
    r" \? _entityRefs\.Field\1 : null;$")
BUTTON_DOWN = re.compile(r"^_(?:touch)?[Bb]uttons\.(\w+)\.IsDown = true;$")
CALL = re.compile(r"^(\w+)\(\);$")
CALL_CONTEXT = re.compile(r"^(\w+)\(context\);$")

CHECK_BEAM = re.compile(r"^CheckBeam\(BeamType\.(\w+)\)$")
CHECK_CHARGE = re.compile(r"^CheckCharge\(GetBeamType\(_(\w+)\)\)$")
AVAILABLE = re.compile(
    r"^(!?)_?(?:player\.)?_?[Aa]vailableWeapons\[BeamType\.(\w+)\]$")
FIELD_EQUALS = re.compile(r"^_(\w+) == (-?\d+)$")
TEST_FLAG = re.compile(r"^(!?)Flags([234])\.TestFlag\(AiFlags\2\.(\w+)\)$")
IS_ALT = re.compile(r"^(!?)_player\.IsAltForm$")

TOUCH_BUTTONS = {"Morph", "Unmorph", "PowerBeam", "Missile", "VoltDriver",
                 "Battlehammer", "Imperialist", "Judicator", "Magmaul",
                 "ShockCoil", "OmegaCannon"}

# The AiContext fields a behaviour writes.  Every one is a plain member on
# the native struct, so the only work is the spelling.
CONTEXT_SET = re.compile(
    r"^context\.Field([0-9A-F]{2}) = (.+);$")
# Field4 through FieldF are the thirteen bytes; the two-digit names above
# are the ints and vectors after them.
CONTEXT_SET_BYTE = re.compile(
    r"^context\.Field([4-9A-F]) = (.+);$")
FRAMES = re.compile(
    r"^_(touchButtons|buttons)\.(\w+)\.Frames(Up|Down)"
    r" (>=|>|<=|<|==|!=) (.+)$")
BUTTON_ASSIGN = re.compile(
    r"^_(touchButtons|buttons)\.(\w+)\.IsDown = (true|false);$")
RNG_COMPARE = re.compile(
    r"^Rng\.GetRandomInt2\((\d+)\) (==|!=|<|>|<=|>=) (\d+)$")
NODE_FROM_REF = re.compile(r"^_(node[0-9A-F]{2}) = _entityRefs\.Field(\d+);$")
# Field118 is a property; its use sites spell it without the underscore.
PROPERTY_SET = re.compile(r"^(Field\w+) = (-?\d+);$")
PROPERTY_STEP = re.compile(r"^_?(\w+)(\+\+|--);$")
# A field set from the random number generator, which several behaviours use
# to decide how long to wait before trying something again.
FIELD_RANDOM = re.compile(
    r"^_(\w+) = Rng\.GetRandomInt2\(([0-9 *+()-]+)\)"
    r"(?: \+ ([0-9 *+()-]+))?;$")
QUEUED_NONE = re.compile(
    r"^_queuedFindEntityAction\s*=\s*AiQueuedEnt\.None;$")
# Debug.Assert has no effect in a release build, and the native tree has no
# equivalent that would be right to invent here.
ASSERT = re.compile(r"^Debug\.Assert\(.*\);$")
FIELD_STEP_BY = re.compile(r"^_?(\w+) (\+=|-=) ([0-9 *+()-]+);$")
CONTEXT_STEP_BY = re.compile(
    r"^context\.Field([0-9A-F]{1,2}) (\+=|-=) ([0-9 *+()-]+);$")
# The two big behaviours are switches over a context byte, and a C# range
# pattern is a run of ordinary cases once it is written out.
SWITCH = re.compile(r"^switch \((.+)\)$")
CASE = re.compile(r"^case (-?\d+):$")
CASE_RANGE = re.compile(r"^case >= (-?\d+) and <= (-?\d+):$")
CONTEXT_READ = re.compile(r"^context\.Field([0-9A-F]{1,2})$")
CONTEXT_COMPARE = re.compile(
    r"^context\.Field([0-9A-F]{1,2}) (==|!=|<=|>=|<|>) (-?\d+)$")
FIELD_COMPARE = re.compile(r"^_(\w+) (==|!=|<=|>=|<|>) (-?\d+)$")
# PlayerAi's predicates answer with an int, and nearly all of them do it
# in one of five shapes.
RETURN_INT = re.compile(r"^return (-?\d+);$")
RETURN_TERNARY = re.compile(r"^return (.+) \? (\d+) : (\d+);$")
RETURN_PREDICATE = re.compile(
    r"^return (Func3_\w+)\(context, param\)( \^ 1)?;$")
RETURN_CONDITION = re.compile(r"^return (.+);$")
BOT_LEVEL = re.compile(
    r"^_player\.BotLevel (==|!=|<=|>=|<|>) (-?\d+)$")
# An entity reference the find pass never filled in is null there and -1
# here, so "has one" is the same question either way.
REF_PRESENT = re.compile(
    r"^_entityRefs\.Field(\d+) (!=|==) null$")
HEALTH = re.compile(
    r"^_player\._health (==|!=|<=|>=|<|>) (\d+)$")
HEALTH_FULL = re.compile(
    r"^_player\._health (==|!=|<=|>=|<|>) _player\._healthMax$")
HEALTH_FRACTION = re.compile(
    r"^_player\._health (==|!=|<=|>=|<|>) _player\._healthMax / (\d+)$")
HUNTER = re.compile(r"^_player\.Hunter (==|!=) Hunter\.(\w+)$")

# Formats.Hunter, in the cartridge's order.
HUNTERS = {
    "Samus": 0, "Kanden": 1, "Trace": 2, "Sylux": 3, "Noxus": 4,
    "Spire": 5, "Weavel": 6, "Guardian": 7,
}
# The three-component fields the behaviours aim with.
VECTOR_SET = re.compile(
    r"^_(field\w+) = new Vector3\(([-0-9. f]+), ([-0-9. f]+), "
    r"([-0-9. f]+)\);$")

# The vectors, which are named with an underscore before the offset in the
# native tree because `field_a0_` reads better than `fielda0_`.
VECTOR_FIELDS = {
    "fieldA0": "field_a0_",
    "fieldAC": "field_ac_",
    "fieldB8": "field_b8_",
    "field90": "field90_",
    "field1038": "field1038_",
    "field1048": "field1048_",
    "field1054": "field1054_",
}


def number(text: str) -> str | None:
    """A float literal such as `29` or `-1.5f`, as C++ spells it."""
    text = text.strip().rstrip("f")
    try:
        value = float(text)
    except ValueError:
        return None
    # A float literal needs its point: `29F` is not a number in C++.
    formatted = repr(value)
    if "." not in formatted and "e" not in formatted:
        formatted += ".0"
    return formatted + "F"

INT_ONLY = re.compile(r"^[0-9 +*()-]+$")


def integer(text: str) -> str | None:
    """A constant integer expression such as `10 * 2`, or None."""
    text = text.strip()
    if not INT_ONLY.match(text):
        return None
    try:
        return str(int(eval(text, {"__builtins__": {}}, {})))  # noqa: S307
    except Exception:
        return None


def button_expression(kind: str, name: str) -> str | None:
    """`_buttons.L` or `_touchButtons.Morph` as a native subscript."""
    if kind == "buttons":
        if name not in BUTTONS:
            return None
        return "buttons_[AiButtonId::%s]" % name
    if name not in TOUCH_BUTTONS:
        return None
    return "touch_buttons_[AiTouchButtonId::%s]" % name


def split_top_level(text: str) -> tuple[str, list[str]] | None:
    """`a && b && c` as ("&&", [a, b, c]), if it is joined at all."""
    depth = 0
    parts: list[str] = []
    operator = None
    start = 0
    index = 0
    while index < len(text):
        character = text[index]
        if character == "(":
            depth += 1
        elif character == ")":
            depth -= 1
        elif depth == 0 and text[index:index + 2] in ("&&", "||"):
            found = text[index:index + 2]
            if operator is not None and found != operator:
                # Mixed && and || without parentheses would need C#'s
                # precedence to read correctly; leave it unconverted.
                return None
            operator = found
            parts.append(text[start:index])
            start = index + 2
            index += 2
            continue
        index += 1
    if operator is None:
        return None
    parts.append(text[start:])
    return operator, parts


def convert_condition(text: str) -> str | None:
    """A boolean expression, or None when it is not one we understand."""
    text = text.strip()
    while (text.startswith("(") and text.endswith(")")
           and split_top_level(text[1:-1]) is None
           and convert_condition_atom(text[1:-1]) is not None):
        text = text[1:-1].strip()
    joined = split_top_level(text)
    if joined is not None:
        operator, parts = joined
        converted = [convert_condition(part) for part in parts]
        if any(part is None for part in converted):
            return None
        return (" %s " % operator).join("(%s)" % part for part in converted)
    return convert_condition_atom(text)


def convert_condition_atom(text: str) -> str | None:
    """One comparison, with no && or || left in it."""
    text = text.strip()
    if text.startswith("(") and text.endswith(")"):
        inner = convert_condition(text[1:-1])
        if inner is not None:
            return "(%s)" % inner
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
    matched = REF_PRESENT.match(text)
    if matched:
        return "entity_refs_.entity(%s) %s 0" % (
            matched.group(1), ">=" if matched.group(2) == "!=" else "<")
    matched = HEALTH_FULL.match(text)
    if matched:
        return ("session.player(bot_slot).health %s"
                " session.inventory(bot_slot).health_max" % matched.group(1))
    matched = HEALTH_FRACTION.match(text)
    if matched:
        return ("session.player(bot_slot).health %s"
                " session.inventory(bot_slot).health_max / %s"
                % (matched.group(1), matched.group(2)))
    matched = HEALTH.match(text)
    if matched:
        return "session.player(bot_slot).health %s %s" % (matched.group(1),
                                                          matched.group(2))
    matched = HUNTER.match(text)
    if matched and matched.group(2) in HUNTERS:
        return "session.player_hunter(bot_slot) %s %d" % (
            matched.group(1), HUNTERS[matched.group(2)])
    matched = BOT_LEVEL.match(text)
    if matched:
        return "bot_level_ %s %s" % (matched.group(1), matched.group(2))
    matched = FIELD_COMPARE.match(text)
    if matched and matched.group(1) in FIELDS:
        return "%s %s %s" % (FIELDS[matched.group(1)], matched.group(2),
                             matched.group(3))
    matched = CONTEXT_COMPARE.match(text)
    if matched:
        return "context.field%s %s %s" % (matched.group(1).lower(),
                                          matched.group(2), matched.group(3))
    matched = TEST_FLAG.match(text)
    if matched:
        return "%shas_flag(flags%s_, AiFlags%s::%s)" % (
            matched.group(1), matched.group(2), matched.group(2),
            matched.group(3))
    matched = IS_ALT.match(text)
    if matched:
        return ("%s((session.player(bot_slot).flags"
                " & net::PlayerState::FlagAltForm) != 0)" % matched.group(1))
    matched = FRAMES.match(text)
    if matched:
        button = button_expression(matched.group(1), matched.group(2))
        bound = integer(matched.group(5))
        if button is not None and bound is not None:
            return "%s.frames_%s %s %s" % (
                button, matched.group(3).lower(), matched.group(4), bound)
    matched = RNG_COMPARE.match(text)
    if matched:
        return "utility::get_random_int2(%su) %s %su" % (
            matched.group(1), matched.group(2), matched.group(3))
    return None


def convert(lines: list[str], known: set[str]) -> tuple[list[str], bool]:
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
            # A condition wrapped across source lines arrives here in
            # pieces, which is a body we do not understand rather than one
            # to guess the rest of.
            if not line.endswith(")"):
                understood = False
                out.append(indent + "// not converted: %s" % line)
                continue
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
        if line == "return;":
            out.append(indent + "return;")
            continue
        matched = RETURN_INT.match(line)
        if matched:
            out.append(indent + "return %s;" % matched.group(1))
            continue
        matched = RETURN_PREDICATE.match(line)
        if matched and matched.group(1) in known:
            member = matched.group(1)
            member = member[0].lower() + member[1:]
            # `^ 1` is how the cartridge writes "the opposite of that".
            out.append(indent + "return %s(session, bot_slot, context,"
                       % member)
            out.append(indent + "                  parameters)%s;"
                       % (" ^ 1" if matched.group(2) else ""))
            continue
        matched = RETURN_TERNARY.match(line)
        if matched:
            condition = convert_condition(matched.group(1))
            if condition is not None:
                out.append(indent + "return %s ? %s : %s;"
                           % (condition, matched.group(2), matched.group(3)))
                continue
        matched = RETURN_CONDITION.match(line)
        if matched:
            condition = convert_condition(matched.group(1))
            if condition is not None:
                out.append(indent + "return (%s) ? 1 : 0;" % condition)
                continue
        if line == "break;":
            out.append(indent + "break;")
            continue
        matched = VECTOR_SET.match(line)
        if matched and matched.group(1) in VECTOR_FIELDS:
            parts = [number(matched.group(index)) for index in (2, 3, 4)]
            if all(part is not None for part in parts):
                out.append(indent + "%s = {%s, %s, %s};"
                           % (VECTOR_FIELDS[matched.group(1)], *parts))
                continue
        matched = SWITCH.match(line)
        if matched:
            if matched.group(1).strip() == "context.Func24Id":
                out.append(indent + "switch (context.func24_id)")
                continue
            subject = CONTEXT_READ.match(matched.group(1).strip())
            if subject is not None:
                out.append(indent + "switch (context.field%s)"
                           % subject.group(1).lower())
                continue
            understood = False
            out.append(indent + "// not converted: %s" % line)
            continue
        matched = CASE.match(line)
        if matched:
            out.append(indent + "case %s:" % matched.group(1))
            continue
        matched = CASE_RANGE.match(line)
        if matched:
            first, last = int(matched.group(1)), int(matched.group(2))
            if last - first > 256:
                understood = False
                out.append(indent + "// not converted: %s" % line)
                continue
            # C#'s range pattern has no C++ spelling that is portable, so
            # the run is written out.  These are byte fields, so the runs
            # are short.
            for value in range(first, last + 1):
                out.append(indent + "case %d:" % value)
            continue
        if line == "default:":
            out.append(indent + "default:")
            continue
        matched = CONTEXT_STEP_BY.match(line)
        if matched:
            amount = integer(matched.group(3))
            if amount is not None:
                out.append(indent + "context.field%s %s %s;"
                           % (matched.group(1).lower(), matched.group(2),
                              amount))
                continue
        matched = FIELD_STEP_BY.match(line)
        if matched and matched.group(1) in FIELDS:
            amount = integer(matched.group(3))
            if amount is not None:
                out.append(indent + "%s %s %s;" % (FIELDS[matched.group(1)],
                                                   matched.group(2), amount))
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
        if TARGET_NULL.match(line):
            out.append(indent + "update_target_item(session, -1);")
            continue
        matched = SET_ENTITY.match(line) or SPAWN_TERNARY.match(line)
        if matched:
            out.append(indent + "set_item_spawn(entity_refs_.entity(%s));"
                       % matched.group(1))
            continue
        matched = BUTTON_DOWN.match(line)
        if matched and matched.group(1) in BUTTONS:
            out.append(indent + "buttons_[AiButtonId::%s].is_down = true;"
                       % matched.group(1))
            continue
        matched = BUTTON_ASSIGN.match(line)
        if matched:
            button = button_expression(matched.group(1), matched.group(2))
            if button is not None:
                out.append(indent + "%s.is_down = %s;"
                           % (button, matched.group(3)))
                continue
        if ASSERT.match(line):
            continue
        matched = PROPERTY_SET.match(line)
        if matched and matched.group(1) in FIELDS:
            out.append(indent + "%s = %s;" % (FIELDS[matched.group(1)],
                                              matched.group(2)))
            continue
        matched = PROPERTY_STEP.match(line)
        if matched and matched.group(1) in FIELDS:
            out.append(indent + "%s%s;" % (matched.group(2),
                                           FIELDS[matched.group(1)]))
            continue
        if QUEUED_NONE.match(line):
            out.append(indent
                       + "queued_find_entity_action_ = AiQueuedEnt::None;")
            continue
        matched = FIELD_RANDOM.match(line)
        if matched and matched.group(1) in FIELDS:
            bound = integer(matched.group(2))
            offset = (integer(matched.group(3)) if matched.group(3)
                      else "0")
            if bound is not None and offset is not None:
                out.append(indent + "%s = static_cast<decltype(%s)>("
                           % (FIELDS[matched.group(1)],
                              FIELDS[matched.group(1)]))
                out.append(indent + "    utility::get_random_int2(%su) + %s);"
                           % (bound, offset))
                continue
        matched = CONTEXT_SET_BYTE.match(line)
        if matched:
            value = integer(matched.group(2).strip())
            if value is not None:
                out.append(indent + "context.field%s = %s;"
                           % (matched.group(1).lower(), value))
                continue
        matched = CONTEXT_SET.match(line)
        if matched:
            value = matched.group(2).strip()
            if value == "_player.Position":
                value = "session.player(bot_slot).position"
            elif value not in ("true", "false"):
                value = integer(value) or ""
            if value:
                out.append(indent + "context.field%s = %s;"
                           % (matched.group(1).lower(), value))
                continue
        matched = NODE_FROM_REF.match(line)
        if matched and matched.group(1) in FIELDS:
            out.append(indent + "%s = entity_refs_.entity(%s);"
                       % (FIELDS[matched.group(1)], matched.group(2)))
            continue
        matched = FIELD_COPY.match(line)
        if (matched and matched.group(1) in FIELDS
                and matched.group(2) in FIELDS):
            out.append(indent + "%s = %s;" % (FIELDS[matched.group(1)],
                                              FIELDS[matched.group(2)]))
            continue
        matched = FIELD_SET.match(line)
        if matched and matched.group(1) in FIELDS:
            out.append(indent + "%s = %s;" % (FIELDS[matched.group(1)],
                                              matched.group(2)))
            continue
        matched = CALL.match(line) or CALL_CONTEXT.match(line)
        if matched and matched.group(1) in known:
            member = matched.group(1)
            member = member[0].lower() + member[1:]
            argument = ("session, bot_slot, context"
                        if CALL_CONTEXT.match(line) else "session, bot_slot")
            out.append(indent + "%s(%s);" % (member, argument))
            continue
        understood = False
        out.append(indent + "// not converted: %s" % line)
    if indent != "    ":
        understood = False
    return out, understood


METHOD = re.compile(r"private [\w?<>\[\], ]+? (\w+)\(")


def method_body(text: str, name: str) -> str | None:
    """The braces-balanced body of a private method, or None."""
    at = -1
    for match in METHOD.finditer(text):
        if match.group(1) == name:
            at = match.start()
            break
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
