#include "Metadata/sound_metadata.hpp"

#include <array>
#include <limits>
#include <stdexcept>
#include <string_view>

namespace fruityprime::sound {
namespace {

struct Layout {
    std::string_view key;
    std::size_t enemy_damage_offset;
    std::size_t enemy_death_offset;
    std::size_t terrain_offset;
    std::size_t beam_offset;
    std::size_t hunter_offset;
    std::uint32_t platform_overlay;
    std::size_t platform_offset;
};

// These are the current values from Extract.cs::RomData.  They are offsets
// in decompressed runtime files, not offsets into the compressed cartridge
// overlay. Keep one entry per version so a revision cannot silently use the
// wrong ARM9 table.
constexpr std::array<Layout, 8> Layouts{{
    {"A76E0", 0x9B574, 0x9B644, 0x1D828, 0x1D8B8, 0x1D96C, 12, 0x81E4},
    {"AMHE0", 0xC54A8, 0xC5578, 0x1DA08, 0x1DA98, 0x1DB4C, 15, 0x8284},
    {"AMHE1", 0xC5D30, 0xC5E00, 0x1DA68, 0x1DAF8, 0x1DBAC, 15, 0x8284},
    {"AMHJ0", 0xC7278, 0xC7348, 0x1DA68, 0x1DAF8, 0x1DBAC, 15, 0x8284},
    {"AMHJ1", 0xC7238, 0xC7308, 0x1DA68, 0x1DAF8, 0x1DBAC, 15, 0x8284},
    {"AMHK0", 0xBE4DC, 0xBE5AC, 0x1BDBA, 0x1BE4A, 0x1BEFE, 15, 0x7CC0},
    {"AMHP0", 0xC5D50, 0xC5E20, 0x1DA08, 0x1DA98, 0x1DB4C, 15, 0x8284},
    {"AMHP1", 0xC5DD0, 0xC5EA0, 0x1DA68, 0x1DAF8, 0x1DBAC, 15, 0x8284},
}};

void require_range(std::span<const std::uint8_t> bytes, std::size_t offset,
                   std::size_t length, std::string_view name) {
    if (offset > bytes.size() || length > bytes.size() - offset) {
        throw std::out_of_range("sound metadata " + std::string(name)
                                + " is outside its runtime file");
    }
}

[[nodiscard]] std::uint16_t u16(std::span<const std::uint8_t> bytes,
                                std::size_t offset) {
    require_range(bytes, offset, 2, "u16 table");
    return static_cast<std::uint16_t>(bytes[offset])
        | static_cast<std::uint16_t>(bytes[offset + 1]) << 8;
}

[[nodiscard]] std::uint32_t u32(std::span<const std::uint8_t> bytes,
                                std::size_t offset) {
    require_range(bytes, offset, 4, "u32 table");
    return static_cast<std::uint32_t>(bytes[offset])
        | static_cast<std::uint32_t>(bytes[offset + 1]) << 8
        | static_cast<std::uint32_t>(bytes[offset + 2]) << 16
        | static_cast<std::uint32_t>(bytes[offset + 3]) << 24;
}

[[nodiscard]] std::int32_t signed_u16(std::span<const std::uint8_t> bytes,
                                      std::size_t offset) {
    const auto value = u16(bytes, offset);
    return value == 0xffffU ? -1 : static_cast<std::int32_t>(value);
}

[[nodiscard]] std::int32_t signed_u32(std::span<const std::uint8_t> bytes,
                                      std::size_t offset) {
    const auto value = u32(bytes, offset);
    return value == 0xffffffffU ? -1 : static_cast<std::int32_t>(value);
}

[[nodiscard]] const Layout* find_layout(std::string_view key) noexcept {
    for (const auto& layout : Layouts) {
        if (layout.key == key) {
            return &layout;
        }
    }
    return nullptr;
}

} // namespace

SoundMetadata SoundMetadata::from_tables(
    std::string game_key, std::span<const std::uint8_t> arm9,
    std::span<const std::uint8_t> overlay2,
    std::span<const std::uint8_t> platform_overlay,
    std::size_t enemy_damage_offset, std::size_t enemy_death_offset,
    std::size_t terrain_offset, std::size_t beam_offset,
    std::size_t hunter_offset, std::size_t platform_offset) {
    SoundMetadata result;
    result.game_key = std::move(game_key);
    for (std::size_t index = 0; index < EnemyCount; ++index) {
        result.enemy_damage_sfx[index] = signed_u32(
            arm9, enemy_damage_offset + index * sizeof(std::uint32_t));
        result.enemy_death_sfx[index] = signed_u32(
            arm9, enemy_death_offset + index * sizeof(std::uint32_t));
    }
    for (std::size_t row = 0; row < TerrainCount; ++row) {
        for (std::size_t column = 0; column < TerrainSfxCount; ++column) {
            result.terrain_sfx[row][column] = signed_u16(
                overlay2, terrain_offset
                    + (row * TerrainSfxCount + column) * sizeof(std::uint16_t));
        }
    }
    for (std::size_t row = 0; row < BeamCount; ++row) {
        for (std::size_t column = 0; column < BeamSfxCount; ++column) {
            result.beam_sfx[row][column] = signed_u16(
                overlay2, beam_offset
                    + (row * BeamSfxCount + column) * sizeof(std::uint16_t));
        }
    }
    for (std::size_t row = 0; row < HunterCount; ++row) {
        for (std::size_t column = 0; column < HunterSfxCount; ++column) {
            result.hunter_sfx[row][column] = signed_u16(
                overlay2, hunter_offset
                    + (row * HunterSfxCount + column) * sizeof(std::uint16_t));
        }
    }
    for (std::size_t row = 0; row < PlatformCount; ++row) {
        for (std::size_t column = 0; column < PlatformSfxCount; ++column) {
            result.platform_sfx[row][column] = signed_u16(
                platform_overlay, platform_offset
                    + (row * PlatformSfxCount + column) * sizeof(std::uint16_t));
        }
    }
    return result;
}

std::optional<SoundMetadata> SoundMetadata::load(
    const assets::Store& assets) {
    const auto key = assets.game_key();
    if (!key.has_value()) {
        return std::nullopt;
    }
    const Layout* layout = find_layout(*key);
    if (layout == nullptr) {
        return std::nullopt;
    }
    const auto arm9 = assets.bytes("_bin/arm9.bin");
    const auto overlay2 = assets.bytes("_bin/overlay9_2");
    const auto platform = assets.bytes(
        "_bin/overlay9_" + std::to_string(layout->platform_overlay));
    return from_tables(*key, arm9, overlay2, platform,
                       layout->enemy_damage_offset, layout->enemy_death_offset,
                       layout->terrain_offset, layout->beam_offset,
                       layout->hunter_offset, layout->platform_offset);
}

std::int32_t SoundMetadata::hunter(std::uint8_t hunter_id,
                                   HunterSfx effect) const noexcept {
    const auto row = static_cast<std::size_t>(hunter_id);
    const auto column = static_cast<std::size_t>(effect);
    return row < hunter_sfx.size() && column < hunter_sfx[row].size()
        ? hunter_sfx[row][column] : -1;
}

std::int32_t SoundMetadata::beam(std::uint8_t beam_id,
                                 BeamSfx effect) const noexcept {
    const auto row = static_cast<std::size_t>(beam_id);
    const auto column = static_cast<std::size_t>(effect);
    return row < beam_sfx.size() && column < beam_sfx[row].size()
        ? beam_sfx[row][column] : -1;
}

std::int32_t SoundMetadata::terrain(std::uint8_t terrain_id,
                                    TerrainSfx effect) const noexcept {
    const auto row = static_cast<std::size_t>(terrain_id);
    const auto column = static_cast<std::size_t>(effect);
    return row < terrain_sfx.size() && column < terrain_sfx[row].size()
        ? terrain_sfx[row][column] : -1;
}

std::int32_t SoundMetadata::platform(std::uint8_t platform_id,
                                     std::size_t effect) const noexcept {
    const auto row = static_cast<std::size_t>(platform_id);
    return row < platform_sfx.size() && effect < platform_sfx[row].size()
        ? platform_sfx[row][effect] : -1;
}

std::int32_t SoundMetadata::enemy_damage(
    std::uint8_t enemy_id) const noexcept {
    const auto index = static_cast<std::size_t>(enemy_id);
    return index < enemy_damage_sfx.size() ? enemy_damage_sfx[index] : -1;
}

std::int32_t SoundMetadata::enemy_death(
    std::uint8_t enemy_id) const noexcept {
    const auto index = static_cast<std::size_t>(enemy_id);
    return index < enemy_death_sfx.size() ? enemy_death_sfx[index] : -1;
}

} // namespace fruityprime::sound
