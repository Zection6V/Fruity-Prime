#include "Metadata/equip_info.hpp"
#include "Metadata/bot_weapons.hpp"
#include "Metadata/weapon_metadata.hpp"
#include "metadata_tables.hpp"
#include "Metadata/weapon_record.hpp"

#include <algorithm>

namespace fruityprime::metadata {


namespace {

// Metadata.Weapons.WeaponsMP, in the order the native session numbers its
// weapon slots: the cartridge lists Volt Driver before Missile, and this head
// has always had them the other way round.  Everything else lines up.
constexpr std::array<std::size_t, WeaponCount> MpTableIndex{{
    0, 2, 1, 3, 4, 5, 6, 7, 8
}};

// The damage, cooldown and ammo figures used to be typed in beside the names
// and had drifted from the cartridge for the Judicator, the Magmaul and the
// Shock Coil.  They are read out of the generated table instead, so the only
// thing left by hand is what the weapon is called.
[[nodiscard]] std::array<WeaponInfo, WeaponCount> build_weapons() {
    std::array<WeaponInfo, WeaponCount> result = detail::WeaponTable;
    const auto& source = WeaponsMp();
    for (std::size_t index = 0; index < result.size(); ++index) {
        const auto& record = source[MpTableIndex[index]];
        result[index].uncharged_damage = record.uncharged_damage;
        result[index].cooldown_ticks = record.shot_cooldown;
        result[index].ammo_type = record.ammo_type;
        result[index].ammo_cost = record.ammo_cost;
        const auto flags = static_cast<std::uint32_t>(record.flags);
        const auto has = [flags](weapon_table::WeaponFlags flag) {
            return (flags & static_cast<std::uint32_t>(flag)) != 0;
        };
        result[index].repeat_fire = has(weapon_table::WeaponFlags::RepeatFire);
        result[index].can_charge = has(weapon_table::WeaponFlags::CanCharge);
        result[index].can_zoom = has(weapon_table::WeaponFlags::CanZoom);
    }
    return result;
}

[[nodiscard]] const std::array<WeaponInfo, WeaponCount>& compact_weapons() {
    static const std::array<WeaponInfo, WeaponCount> value = build_weapons();
    return value;
}

} // namespace

const std::array<WeaponInfo, WeaponCount>& weapons() noexcept {
    return compact_weapons();
}

const WeaponInfo& weapon_info(std::uint8_t id) noexcept {
    const std::size_t index = std::min<std::size_t>(id, WeaponCount - 1);
    return compact_weapons()[index];
}

const WeaponInfo* find_weapon(std::string_view name) noexcept {
    for (const auto& weapon : compact_weapons()) {
        if (detail::equal_ascii_insensitive(weapon.name, name)
            || detail::equal_ascii_insensitive(weapon.display_name, name)) {
            return &weapon;
        }
    }
    return nullptr;
}

const std::array<MultiplayerWeaponInfo, MultiplayerWeaponCount>&
multiplayer_weapons() noexcept {
    return detail::MultiplayerWeaponTable;
}

std::uint8_t native_weapon_slot_from_beam(std::int32_t beam_type) noexcept {
    // BeamType is the persisted managed ordinal.  The native table's only
    // reordered entries are Volt and Missile.
    constexpr std::array<std::uint8_t, WeaponCount> slots{
        0, 2, 1, 3, 4, 5, 6, 7, 8
    };
    if (beam_type < 0
        || static_cast<std::size_t>(beam_type) >= slots.size()) {
        return 0xff;
    }
    return slots[static_cast<std::size_t>(beam_type)];
}

std::int32_t beam_type_from_native_weapon_slot(
    std::uint8_t weapon_slot) noexcept {
    constexpr std::array<std::int32_t, WeaponCount> beams{
        0, 2, 1, 3, 4, 5, 6, 7, 8
    };
    return weapon_slot < beams.size()
        ? beams[weapon_slot] : -1;
}

const WeaponVisualInfo& weapon_visual_info(std::uint8_t id) noexcept {
    const std::size_t index = std::min<std::size_t>(id, WeaponCount - 1);
    return detail::WeaponVisualTable[index];
}

const std::array<ItemInfo, ItemTableCount>& items() noexcept {
    return detail::ItemTable;
}

const ItemInfo* item_info(std::int32_t id) noexcept {
    if (id < -1 || id >= static_cast<std::int32_t>(ItemCount)) {
        return nullptr;
    }
    return &detail::ItemTable[static_cast<std::size_t>(id + 1)];
}

const std::array<std::string_view, FhItemCount>& fh_items() noexcept {
    return detail::FhItemTable;
}

} // namespace fruityprime::metadata
