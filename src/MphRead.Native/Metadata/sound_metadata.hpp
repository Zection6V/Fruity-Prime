#pragma once

#include "Assets/game_assets.hpp"

#include <array>
#include <cstddef>
#include <cstdint>
#include <optional>
#include <span>
#include <string>

namespace fruityprime::sound {

// Column ordinals from Metadata/SoundMeta.cs.  These are kept separate from
// the sound resource parser because the tables live in ARM9/overlay code
// rather than in data/sound files.
enum class HunterSfx : std::uint8_t {
    Damage = 0,
    Death,
    DamageEnemy,
    DeathEnemy,
    Spawn,
    Jump,
    Unmorph,
    Morph,
    Roll,
    BoostCharge,
    Boost,
    Bounce,
    BeamSwitch,
    MissileSwitch,
    MissileOpen,
    MissileClose,
    MissileCharge,
};

enum class BeamSfx : std::uint8_t {
    Charge = 0,
    Shot,
    Ricochet,
    ChargeShot,
    Hit,
    ChargeHit,
    Empty,
    Switch,
    Homing,
    AffinityChargeShot,
};

enum class TerrainSfx : std::uint8_t {
    Walk1 = 0,
    Walk2,
    Land,
    Slide,
    Roll,
    TraceAlt,
};

// Native counterpart of the six initialized tables in SoundMeta.cs. Missing
// cartridge references are represented by -1, exactly like the managed
// 0xffff/0xffffffff conversion.
struct SoundMetadata {
    static constexpr std::size_t HunterCount = 8;
    static constexpr std::size_t HunterSfxCount = 17;
    static constexpr std::size_t BeamCount = 9;
    static constexpr std::size_t BeamSfxCount = 10;
    static constexpr std::size_t TerrainCount = 12;
    static constexpr std::size_t TerrainSfxCount = 6;
    static constexpr std::size_t PlatformCount = 45;
    static constexpr std::size_t PlatformSfxCount = 4;
    static constexpr std::size_t EnemyCount = 52;

    using HunterTable = std::array<
        std::array<std::int32_t, HunterSfxCount>, HunterCount>;
    using BeamTable = std::array<
        std::array<std::int32_t, BeamSfxCount>, BeamCount>;
    using TerrainTable = std::array<
        std::array<std::int32_t, TerrainSfxCount>, TerrainCount>;
    using PlatformTable = std::array<
        std::array<std::int32_t, PlatformSfxCount>, PlatformCount>;
    using EnemyTable = std::array<std::int32_t, EnemyCount>;

    std::string game_key;
    HunterTable hunter_sfx{};
    BeamTable beam_sfx{};
    TerrainTable terrain_sfx{};
    PlatformTable platform_sfx{};
    EnemyTable enemy_damage_sfx{};
    EnemyTable enemy_death_sfx{};

    // Direct ROM and extracted-directory stores expose the same decompressed
    // `_bin/` views that Extract.LoadRuntimeData consumes.
    [[nodiscard]] static std::optional<SoundMetadata> load(
        const assets::Store& assets);

    // Public for focused parser tests and for future First Hunt/version
    // layouts. Offsets are in the already decompressed runtime files.
    [[nodiscard]] static SoundMetadata from_tables(
        std::string game_key, std::span<const std::uint8_t> arm9,
        std::span<const std::uint8_t> overlay2,
        std::span<const std::uint8_t> platform_overlay,
        std::size_t enemy_damage_offset, std::size_t enemy_death_offset,
        std::size_t terrain_offset, std::size_t beam_offset,
        std::size_t hunter_offset, std::size_t platform_offset);

    [[nodiscard]] std::int32_t hunter(std::uint8_t hunter_id,
                                      HunterSfx effect) const noexcept;
    [[nodiscard]] std::int32_t beam(std::uint8_t beam_id,
                                    BeamSfx effect) const noexcept;
    [[nodiscard]] std::int32_t terrain(std::uint8_t terrain_id,
                                       TerrainSfx effect) const noexcept;
    [[nodiscard]] std::int32_t platform(std::uint8_t platform_id,
                                        std::size_t effect) const noexcept;
    [[nodiscard]] std::int32_t enemy_damage(
        std::uint8_t enemy_id) const noexcept;
    [[nodiscard]] std::int32_t enemy_death(
        std::uint8_t enemy_id) const noexcept;
};

} // namespace fruityprime::sound
