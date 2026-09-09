#!/usr/bin/env python3
"""Generate the native weapon tables from Metadata/Weapons.cs.

The managed WeaponInfo carries around sixty fields per weapon -- charge
levels, splash, headshots, afflictions, damage direction, speed decay, zoom.
Transcribing seventy-one of those by hand is how the native table came to hold
twelve of them, so it is generated instead: this reads the constructor calls
and writes the counterpart table, which makes the two impossible to drift
apart without the generator saying so.

    python tools/gen-weapon-metadata.py

Writes src/MphRead.Native/Metadata/Weapons.generated.cpp and the record in
Metadata/weapon_record.hpp.
"""

from __future__ import annotations

import pathlib
import re
import sys

ROOT = pathlib.Path(__file__).resolve().parent.parent
SOURCE = ROOT / "src" / "MphRead" / "Metadata" / "Weapons.cs"
RECORD = ROOT / "src" / "MphRead.Native" / "Metadata" / "weapon_record.hpp"
TABLE = (ROOT / "src" / "MphRead.Native" / "Metadata"
         / "Weapons.generated.cpp")

# The managed constructor's parameters, in order, with the native type and
# member name.  Arrays keep their length so the record stays fixed-size.
FIELDS = [
    ("description", "std::string_view", "description", None),
    ("beam", "BeamType", "beam", None),
    ("beamKind", "BeamType", "beam_kind", None),
    ("drawFuncIds", "std::uint8_t", "draw_func_ids", 2),
    ("colors", "std::uint16_t", "colors", 2),
    ("priority", "std::uint8_t", "priority", None),
    ("flags", "WeaponFlags", "flags", None),
    ("splashDamage", "std::uint16_t", "splash_damage", None),
    ("minChargeSplashDamage", "std::uint16_t", "min_charge_splash_damage",
     None),
    ("chargedSplashDamage", "std::uint16_t", "charged_splash_damage", None),
    ("splashDmgTypes", "std::uint8_t", "splash_damage_types", 2),
    ("shotCooldown", "std::uint8_t", "shot_cooldown", None),
    ("autofireCooldown", "std::uint8_t", "autofire_cooldown", None),
    ("ammoType", "std::uint8_t", "ammo_type", None),
    ("colEffects", "std::uint8_t", "collision_effects", 2),
    ("muzzleEffects", "std::uint8_t", "muzzle_effects", 2),
    ("dmgDirTypes", "std::uint8_t", "dmg_dir_types", 2),
    ("dmgInterp", "std::uint8_t", "damage_interpolations", 2),
    ("afflictions", "Affliction", "afflictions", 2),
    ("padding21", "std::uint8_t", "padding21", None),
    ("minCharge", "std::uint16_t", "min_charge", None),
    ("fullCharge", "std::uint16_t", "full_charge", None),
    ("ammoCost", "std::uint16_t", "ammo_cost", None),
    ("minChargeCost", "std::uint16_t", "min_charge_cost", None),
    ("chargeCost", "std::uint16_t", "charge_cost", None),
    ("unchargedDamage", "std::uint16_t", "uncharged_damage", None),
    ("minChargeDamage", "std::uint16_t", "min_charge_damage", None),
    ("chargedDamage", "std::uint16_t", "charged_damage", None),
    ("headshotDamage", "std::uint16_t", "headshot_damage", None),
    ("minChargeHeadshotDamage", "std::uint16_t",
     "min_charge_headshot_damage", None),
    ("chargedHeadshotDamage", "std::uint16_t", "charged_headshot_damage",
     None),
    ("unchargedLifespan", "std::uint16_t", "uncharged_lifespan", None),
    ("minChargeLifespan", "std::uint16_t", "min_charge_lifespan", None),
    ("chargedLifespan", "std::uint16_t", "charged_lifespan", None),
    ("speedDecay", "std::uint16_t", "speed_decay_times", 2),
    ("padding42", "std::uint16_t", "padding42", None),
    ("speedInterp", "std::uint16_t", "speed_interpolations", 2),
    ("unchargedDmgDirMag", "std::int32_t", "uncharged_dmg_dir_mag", None),
    ("minChargeDmgDirMag", "std::int32_t", "min_charge_dmg_dir_mag", None),
    ("chargedDmgDirMag", "std::int32_t", "charged_dmg_dir_mag", None),
    ("zoomFov", "std::int32_t", "zoom_fov", None),
    ("unchargedCylRadius", "std::int32_t", "uncharged_cyl_radius", None),
    ("minChargeCylRadius", "std::int32_t", "min_charge_cyl_radius", None),
    ("chargedCylRadius", "std::int32_t", "charged_cyl_radius", None),
    ("unchargedSpeed", "std::int32_t", "uncharged_speed", None),
    ("minChargeSpeed", "std::int32_t", "min_charge_speed", None),
    ("chargedSpeed", "std::int32_t", "charged_speed", None),
    ("unchargedFinalSpeed", "std::int32_t", "uncharged_final_speed", None),
    ("minChargeFinalSpeed", "std::int32_t", "min_charge_final_speed", None),
    ("chargedFinalSpeed", "std::int32_t", "charged_final_speed", None),
    ("unchargedGravity", "std::int32_t", "uncharged_gravity", None),
    ("minChargeGravity", "std::int32_t", "min_charge_gravity", None),
    ("chargedGravity", "std::int32_t", "charged_gravity", None),
    ("unchargedHoming", "std::int32_t", "uncharged_homing", None),
    ("minChargeHoming", "std::int32_t", "min_charge_homing", None),
    ("chargedHoming", "std::int32_t", "charged_homing", None),
    ("homingRange", "std::int32_t", "homing_range", None),
    ("homingTolerance", "std::int32_t", "homing_tolerance", None),
    ("unchargedSplashRadius", "std::int32_t", "uncharged_splash_radius",
     None),
    ("minChargeSplashRadius", "std::int32_t", "min_charge_splash_radius",
     None),
    ("chargedSplashRadius", "std::int32_t", "charged_splash_radius", None),
    ("unchargedDistance", "std::int32_t", "uncharged_distance", None),
    ("minChargeDistance", "std::int32_t", "min_charge_distance", None),
    ("chargedDistance", "std::int32_t", "charged_distance", None),
    ("unchargedSpread", "std::int32_t", "uncharged_spread", None),
    ("minChargeSpread", "std::int32_t", "min_charge_spread", None),
    ("chargedSpread", "std::int32_t", "charged_spread", None),
    ("unRicoLossH", "std::int32_t", "uncharged_rico_loss_h", None),
    ("minRicoLossH", "std::int32_t", "min_charge_rico_loss_h", None),
    ("chRicoLossH", "std::int32_t", "charged_rico_loss_h", None),
    ("unRicoLossV", "std::int32_t", "uncharged_rico_loss_v", None),
    ("minRicoLossV", "std::int32_t", "min_charge_rico_loss_v", None),
    ("chRicoLossV", "std::int32_t", "charged_rico_loss_v", None),
    ("projectileCount", "std::uint8_t", "projectile_count", None),
    ("minChargedProjectileCount", "std::uint8_t",
     "min_charge_projectile_count", None),
    ("chargeProjectileCount", "std::uint8_t", "charge_projectile_count",
     None),
    ("smokeStart", "std::uint16_t", "smoke_start", None),
    ("smokeMinimum", "std::uint16_t", "smoke_minimum", None),
    ("smokeDrain", "std::uint16_t", "smoke_drain", None),
    ("smokeShotAmount", "std::uint16_t", "smoke_shot_amount", None),
    ("smokeChargeAmount", "std::uint16_t", "smoke_charge_amount", None),
]

TABLES = [
    ("Weapons1P", "Weapons1P"),
    ("WeaponsMP", "WeaponsMp"),
    ("EnemyWeapons", "EnemyWeapons"),
    ("BossWeapons", "BossWeapons"),
    ("GoreaWeapons", "GoreaWeapons"),
    ("PlatformWeapons", "PlatformWeapons"),
    ("Ricochets", "Ricochets"),
]


def split_arguments(text: str) -> list[str]:
    """Split a constructor argument list on commas at depth zero."""
    parts: list[str] = []
    depth = 0
    current: list[str] = []
    in_string = False
    for index, char in enumerate(text):
        if in_string:
            current.append(char)
            if char == '"' and text[index - 1] != "\\":
                in_string = False
            continue
        if char == '"':
            in_string = True
            current.append(char)
            continue
        if char in "([{":
            depth += 1
        elif char in ")]}":
            depth -= 1
        if char == "," and depth == 0:
            parts.append("".join(current))
            current = []
            continue
        current.append(char)
    if current:
        parts.append("".join(current))
    return [part.strip() for part in parts if part.strip()]


def find_entries(source: str, name: str) -> list[dict[str, str]]:
    """Every `new WeaponInfo(...)` in the named table, as name -> value."""
    start = source.index("IReadOnlyList<WeaponInfo> %s" % name)
    # The table ends at the closing brace of its initializer.
    depth = 0
    index = source.index("{", start)
    end = index
    while True:
        if source[end] == "{":
            depth += 1
        elif source[end] == "}":
            depth -= 1
            if depth == 0:
                break
        end += 1
    block = source[index:end]

    entries: list[dict[str, str]] = []
    cursor = 0
    while True:
        at = block.find("new WeaponInfo(", cursor)
        if at < 0:
            break
        open_paren = block.index("(", at)
        depth = 0
        close = open_paren
        while True:
            if block[close] == "(":
                depth += 1
            elif block[close] == ")":
                depth -= 1
                if depth == 0:
                    break
            close += 1
        body = block[open_paren + 1:close]
        fields: dict[str, str] = {}
        for argument in split_arguments(body):
            label, _, value = argument.partition(":")
            fields[label.strip()] = value.strip()
        entries.append(fields)
        cursor = close
    return entries


def convert_scalar(kind: str, value: str) -> str:
    value = " ".join(value.split())
    if kind == "std::string_view":
        return value if value.startswith('"') else '"%s"' % value
    if kind == "BeamType":
        return "BeamType::" + value.split(".")[-1]
    if kind == "Affliction":
        return "Affliction::" + value.split(".")[-1]
    if kind == "WeaponFlags":
        parts = [part.strip().split(".")[-1] for part in value.split("|")]
        return " | ".join("WeaponFlags::" + part for part in parts)
    return value


def convert(kind: str, count: int | None, value: str) -> str:
    if count is None:
        return convert_scalar(kind, value)
    inner = value[value.index("{") + 1:value.rindex("}")]
    items = [convert_scalar(kind, item)
             for item in split_arguments(inner)]
    if len(items) != count:
        raise SystemExit("expected %d items, got %d in %r"
                         % (count, len(items), value))
    return "{{" + ", ".join(items) + "}}"


def emit_record() -> str:
    lines = [
        "#pragma once",
        "",
        "// Generated by tools/gen-weapon-metadata.py from",
        "// src/MphRead/Metadata/Weapons.cs.  Do not edit by hand.",
        "//",
        "// One record per weapon, with the same fields the managed",
        "// WeaponInfo carries.  The compact accessors in Weapons.cpp read",
        "// out of this rather than out of a second, shorter table.",
        "",
        '#include "Formats/Types.hpp"',
        '#include "Metadata/weapon_metadata.hpp"',
        "",
        "#include <array>",
        "#include <cstdint>",
        "#include <string_view>",
        "",
        "namespace fruityprime::metadata {",
        "",
        "// The enums the managed table names live in their own"
        " counterparts;",
        "// naming them here keeps the generated rows reading like the"
        " C# ones.",
        "using formats::Affliction;",
        "using formats::BeamType;",
        "using weapon_table::WeaponFlags;",
        "",
        "struct WeaponRecord {",
    ]
    for _, kind, member, count in FIELDS:
        if count is None:
            lines.append("    %s %s{};" % (kind, member))
        else:
            lines.append("    std::array<%s, %d> %s{};" % (kind, count,
                                                           member))
    lines += [
        "};",
        "",
        "} // namespace fruityprime::metadata",
        "",
    ]
    return "\n".join(lines)


def emit_table(tables: dict[str, list[dict[str, str]]]) -> str:
    lines = [
        "// Generated by tools/gen-weapon-metadata.py from",
        "// src/MphRead/Metadata/Weapons.cs.  Do not edit by hand.",
        '#include "Metadata/weapon_record.hpp"',
        "",
        "namespace fruityprime::metadata {",
        "",
    ]
    for cs_name, cpp_name in TABLES:
        entries = tables[cs_name]
        lines.append("const std::array<WeaponRecord, %d>& %s() noexcept {"
                     % (len(entries), cpp_name))
        lines.append("    static const std::array<WeaponRecord, %d> value{{"
                     % len(entries))
        for fields in entries:
            description = fields.get("description", '""')
            lines.append("        // %s" % description.strip('"'))
            lines.append("        WeaponRecord{")
            rendered = []
            for label, kind, member, count in FIELDS:
                if label not in fields:
                    raise SystemExit("missing %s in %s" % (label, cs_name))
                rendered.append("            /* %s */ %s"
                                % (member, convert(kind, count,
                                                   fields[label])))
            lines.append(",\n".join(rendered))
            lines.append("        },")
        lines.append("    }};")
        lines.append("    return value;")
        lines.append("}")
        lines.append("")
    lines += ["} // namespace fruityprime::metadata", ""]
    return "\n".join(lines)


def main() -> int:
    source = SOURCE.read_text(encoding="utf-8")
    tables = {name: find_entries(source, name) for name, _ in TABLES}
    total = sum(len(value) for value in tables.values())
    RECORD.write_text(emit_record(), encoding="utf-8", newline="\r\n")
    TABLE.write_text(emit_table(tables), encoding="utf-8", newline="\r\n")
    for name, _ in TABLES:
        print("%-18s %d" % (name, len(tables[name])))
    print("total %d weapons, %d fields each" % (total, len(FIELDS)))
    return 0


if __name__ == "__main__":
    sys.exit(main())
