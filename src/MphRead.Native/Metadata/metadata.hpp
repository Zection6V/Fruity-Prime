#pragma once

#include "Formats/Types.hpp"

#include <array>
#include <cstdint>
#include <optional>
#include <string_view>

namespace fruityprime::metadata {

// These values mirror the byte-sized enums in Formats.Enums.cs.  Keeping the
// IDs in one native table prevents the renderer, launcher, and network roster
// from each growing a different interpretation of a hunter.
enum class Hunter : std::uint8_t {
    Samus = 0,
    Kanden = 1,
    Trace = 2,
    Sylux = 3,
    Noxus = 4,
    Spire = 5,
    Weavel = 6,
    Guardian = 7,
    Random = 8
};

constexpr std::size_t PlayableHunterCount = 7;
constexpr std::size_t HunterCount = 8;

struct HunterInfo {
    Hunter id = Hunter::Samus;
    std::string_view name;
    std::string_view archive;
    std::string_view base_model_entry;
    std::string_view alt_model_entry;
    std::string_view base_recolor_prefix;
    std::string_view alt_recolor_prefix;
    // Native weapon slot, not the cartridge BeamType ordinal.
    std::uint8_t affinity_weapon = 0;
    // Metadata.HunterModels[hunter][3]: the arm cannon, which is the only
    // part of the hunter the player sees of themselves in first person.
    std::string_view gun_model_entry;
};

[[nodiscard]] const std::array<HunterInfo, HunterCount>& hunters() noexcept;
[[nodiscard]] const HunterInfo* find_hunter(std::string_view name) noexcept;
[[nodiscard]] const HunterInfo& hunter_info(std::uint8_t id) noexcept;
[[nodiscard]] std::optional<std::uint8_t> parse_hunter(
    std::string_view value) noexcept;
[[nodiscard]] std::uint8_t roll_hunter(std::uint32_t seed) noexcept;

struct WeaponInfo {
    std::uint8_t id = 0;
    std::string_view name;
    std::string_view display_name;
    std::uint16_t uncharged_damage = 0;
    float projectile_speed = 0.0F;
    float lifetime_seconds = 0.0F;
    std::uint32_t cooldown_ticks = 0;
    bool repeat_fire = false;
    bool can_charge = false;
    bool can_zoom = false;
    std::uint8_t ammo_type = 0;
    std::uint16_t ammo_cost = 0;
};

constexpr std::size_t WeaponCount = 9;

// The full managed tables, generated from Metadata/Weapons.cs by
// tools/gen-weapon-metadata.py.  WeaponInfo above is the compact
// view the session reads; these carry every field the cartridge
// does -- charge levels, splash, headshots, afflictions, damage
// direction, speed decay, zoom.
struct WeaponRecord;
[[nodiscard]] const std::array<WeaponRecord, 18>& Weapons1P() noexcept;
[[nodiscard]] const std::array<WeaponRecord, 18>& WeaponsMp() noexcept;
[[nodiscard]] const std::array<WeaponRecord, 11>& EnemyWeapons() noexcept;
[[nodiscard]] const std::array<WeaponRecord, 8>& BossWeapons() noexcept;
[[nodiscard]] const std::array<WeaponRecord, 6>& GoreaWeapons() noexcept;
[[nodiscard]] const std::array<WeaponRecord, 4>& PlatformWeapons() noexcept;
[[nodiscard]] const std::array<WeaponRecord, 6>& Ricochets() noexcept;

// The managed multiplayer table has a plain and an affinity entry for each
// playable beam.  The session-facing WeaponInfo above intentionally remains
// compact; this record preserves the complete fields needed by the mechanics
// dump (and by the future charged-projectile path) without making every
// gameplay call site depend on presentation-only values.
struct MultiplayerWeaponInfo {
    formats::BeamType beam = formats::BeamType::PowerBeam;
    std::string_view description;
    std::uint32_t flags = 0;
    std::uint16_t splash_damage = 0;
    std::uint16_t min_charge_splash_damage = 0;
    std::uint16_t charged_splash_damage = 0;
    std::uint8_t shot_cooldown = 0;
    std::uint8_t ammo_type = 0;
    std::array<std::uint8_t, 2> afflictions{0, 0};
    std::uint16_t min_charge = 0;
    std::uint16_t full_charge = 0;
    std::uint16_t ammo_cost = 0;
    std::uint16_t min_charge_cost = 0;
    std::uint16_t charge_cost = 0;
    std::uint16_t uncharged_damage = 0;
    std::uint16_t min_charge_damage = 0;
    std::uint16_t charged_damage = 0;
    std::uint16_t headshot_damage = 0;
    std::uint16_t min_charge_headshot_damage = 0;
    std::uint16_t charged_headshot_damage = 0;
};

constexpr std::size_t MultiplayerWeaponCount = WeaponCount * 2;

[[nodiscard]] const std::array<WeaponInfo, WeaponCount>& weapons() noexcept;
[[nodiscard]] const WeaponInfo& weapon_info(std::uint8_t id) noexcept;
[[nodiscard]] const WeaponInfo* find_weapon(std::string_view name) noexcept;
[[nodiscard]] const std::array<MultiplayerWeaponInfo,
                               MultiplayerWeaponCount>&
multiplayer_weapons() noexcept;

// StorySave and enemy-spawn records use the cartridge BeamType ordinal
// (Power, Volt, Missile, ...).  The native simulation keeps a compact weapon
// table with Missile in slot 1 and Volt in slot 2 so its legacy input cycle
// remains stable.  These helpers are the single conversion boundary between
// those two representations.
[[nodiscard]] std::uint8_t native_weapon_slot_from_beam(
    std::int32_t beam_type) noexcept;
[[nodiscard]] std::int32_t beam_type_from_native_weapon_slot(
    std::uint8_t weapon_slot) noexcept;

// The managed Weapons tables carry two visual variants for every weapon
// (uncharged and charged).  The native simulation currently fires the first
// variant, but keeping both values here prevents the renderer and the future
// charge path from having to reconstruct them from draw-function switches.
struct WeaponVisualInfo {
    std::array<std::uint8_t, 2> draw_func_ids{255, 255};
    std::array<std::uint16_t, 2> colors{0x7fff, 0x7fff};
    std::array<std::uint8_t, 2> collision_effects{255, 255};
    std::array<std::uint8_t, 2> muzzle_effects{255, 255};
    std::array<std::uint8_t, 2> damage_dir_types{0, 0};
    std::array<std::uint8_t, 2> damage_interpolations{0, 0};
};

[[nodiscard]] const WeaponVisualInfo& weapon_visual_info(
    std::uint8_t id) noexcept;

struct EffectInfo {
    std::uint16_t id = 0;
    std::string_view name;
    // Empty means the direct `effects/<name>_PS.bin` path.  Non-empty values
    // identify the managed `_archives/<archive>/...` virtual archive.
    std::string_view archive;
};

constexpr std::size_t EffectCount = 247;

[[nodiscard]] const std::array<EffectInfo, EffectCount>& effects() noexcept;
[[nodiscard]] const EffectInfo* effect_info(std::uint16_t id) noexcept;

struct ItemInfo {
    std::int32_t id = -1;
    std::string_view name;
    // Cartridge asset name from Metadata.Items. The readable name above is
    // retained for launcher/network text and the asset spelling is kept for
    // model/resource lookup.
    std::string_view asset_name;
    // -1 for items that are not weapons.
    std::int8_t weapon = -1;
};

constexpr std::size_t ItemCount = 23;
constexpr std::size_t ItemTableCount = ItemCount + 1;

[[nodiscard]] const std::array<ItemInfo, ItemTableCount>& items() noexcept;
[[nodiscard]] const ItemInfo* item_info(std::int32_t id) noexcept;

constexpr std::size_t FhItemCount = 8;

[[nodiscard]] const std::array<std::string_view, FhItemCount>& fh_items() noexcept;

struct GameModeInfo {
    std::uint8_t id = 0;
    std::string_view name;
    bool team_mode = false;
    bool survival = false;
    bool objective = false;
};

[[nodiscard]] const GameModeInfo* game_mode_info(std::uint8_t id) noexcept;
[[nodiscard]] std::string_view game_mode_name(std::uint8_t id) noexcept;
// Metadata.GetAreaInfo equivalent. Story room IDs 27..92 map to the eight
// planetary areas; the Oubliette and multiplayer rooms use area 8.
[[nodiscard]] std::int32_t area_info(std::int32_t room_id) noexcept;

enum class Effectiveness : std::uint8_t {
    Zero = 0,
    Half = 1,
    Normal = 2,
    Double = 3,
};

struct EnemyInfo {
    formats::EnemyType id = formats::EnemyType::WarWasp;
    std::uint16_t death_effect = 0;
    std::uint8_t audio_range_index = 0;
    std::uint16_t scan_id = 0;
    // Nine two-bit effectiveness values are packed in the cartridge table,
    // one for each weapon slot.
    std::uint32_t effectiveness = 0;
};

constexpr std::size_t EnemyCount = 52;

[[nodiscard]] const std::array<EnemyInfo, EnemyCount>& enemies() noexcept;
[[nodiscard]] const EnemyInfo& enemy_info(std::uint8_t id) noexcept;
[[nodiscard]] std::array<Effectiveness, 9> decode_effectiveness(
    std::uint32_t packed) noexcept;
[[nodiscard]] float damage_multiplier(Effectiveness value) noexcept;

} // namespace fruityprime::metadata

namespace MphReadNative {
namespace Metadata = ::fruityprime::metadata;
}
