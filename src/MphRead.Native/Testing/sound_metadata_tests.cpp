#include "Assets/game_assets.hpp"
#include "Metadata/sound_metadata.hpp"
#include "Metadata/MetadataFacade.hpp"
#include "Strings.hpp"
#include "Utility/extract.hpp"

#include <cstdint>
#include <cstdlib>
#include <chrono>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <stdexcept>
#include <string>
#include <vector>

namespace {

void require(bool condition, const std::string& message) {
    if (!condition) {
        throw std::runtime_error(message);
    }
}

void put_u16(std::vector<std::uint8_t>& bytes, std::size_t offset,
             std::uint16_t value) {
    require(offset + 2 <= bytes.size(), "synthetic u16 write is out of range");
    bytes[offset] = static_cast<std::uint8_t>(value);
    bytes[offset + 1] = static_cast<std::uint8_t>(value >> 8);
}

void put_u32(std::vector<std::uint8_t>& bytes, std::size_t offset,
             std::uint32_t value) {
    require(offset + 4 <= bytes.size(), "synthetic u32 write is out of range");
    bytes[offset] = static_cast<std::uint8_t>(value);
    bytes[offset + 1] = static_cast<std::uint8_t>(value >> 8);
    bytes[offset + 2] = static_cast<std::uint8_t>(value >> 16);
    bytes[offset + 3] = static_cast<std::uint8_t>(value >> 24);
}

void test_synthetic_tables() {
    constexpr std::size_t enemy_death_offset = 208;
    constexpr std::size_t terrain_offset = 0;
    constexpr std::size_t beam_offset = 144;
    constexpr std::size_t hunter_offset = 324;

    std::vector<std::uint8_t> arm9(416);
    std::vector<std::uint8_t> overlay2(596);
    std::vector<std::uint8_t> platform(360);

    for (std::size_t index = 0;
         index < fruityprime::sound::SoundMetadata::EnemyCount; ++index) {
        put_u32(arm9, index * 4, 0x1000U + static_cast<std::uint32_t>(index));
        put_u32(arm9, enemy_death_offset + index * 4,
                0x2000U + static_cast<std::uint32_t>(index));
    }
    for (std::size_t index = 0; index < 12 * 6; ++index) {
        put_u16(overlay2, terrain_offset + index * 2,
                static_cast<std::uint16_t>(0x3000U + index));
    }
    for (std::size_t index = 0; index < 9 * 10; ++index) {
        put_u16(overlay2, beam_offset + index * 2,
                static_cast<std::uint16_t>(0x4000U + index));
    }
    for (std::size_t index = 0; index < 8 * 17; ++index) {
        put_u16(overlay2, hunter_offset + index * 2,
                static_cast<std::uint16_t>(0x5000U + index));
    }
    for (std::size_t index = 0; index < 45 * 4; ++index) {
        put_u16(platform, index * 2,
                static_cast<std::uint16_t>(0x6000U + index));
    }

    // The managed parser turns these cartridge sentinels into -1.
    put_u32(arm9, enemy_death_offset + 3 * 4, 0xffffffffU);
    put_u16(overlay2, hunter_offset + (1 * 17 + 7) * 2, 0xffffU);

    const auto metadata = fruityprime::sound::SoundMetadata::from_tables(
        "TEST", arm9, overlay2, platform, 0, enemy_death_offset,
        terrain_offset, beam_offset, hunter_offset, 0);
    fruityprime::metadata::Metadata::SetHunterSfxData(
        std::span<const std::uint8_t>(overlay2).subspan(
            hunter_offset, 8 * 17 * 2));
    fruityprime::metadata::Metadata::SetBeamSfxData(
        std::span<const std::uint8_t>(overlay2).subspan(
            beam_offset, 9 * 10 * 2));
    fruityprime::metadata::Metadata::SetTerrainSfxData(
        std::span<const std::uint8_t>(overlay2).subspan(
            terrain_offset, 12 * 6 * 2));
    fruityprime::metadata::Metadata::SetPlatformSfxData(platform);
    fruityprime::metadata::Metadata::SetEnemyDamageSfxData(
        std::span<const std::uint8_t>(arm9).first(52 * 4));
    fruityprime::metadata::Metadata::SetEnemyDeathSfxData(
        std::span<const std::uint8_t>(arm9).subspan(
            enemy_death_offset, 52 * 4));
    require(metadata.game_key == "TEST", "synthetic game key mismatch");
    require(fruityprime::metadata::Metadata::HunterSfx[1][7] == -1
                && fruityprime::metadata::Metadata::BeamSfx[8][9]
                    == 0x4000 + 8 * 10 + 9
                && fruityprime::metadata::Metadata::EnemyDeathSfx[3] == -1,
            "Metadata static sound tables mismatch");
    require(metadata.enemy_damage(0) == 0x1000
                && metadata.enemy_damage(51) == 0x1033,
            "synthetic enemy damage table mismatch");
    require(metadata.enemy_death(0) == 0x2000
                && metadata.enemy_death(3) == -1
                && metadata.enemy_death(51) == 0x2033,
            "synthetic enemy death table mismatch");
    require(metadata.terrain(2, fruityprime::sound::TerrainSfx::Roll)
                == 0x3000 + 2 * 6 + 4,
            "synthetic terrain table mismatch");
    require(metadata.beam(8, fruityprime::sound::BeamSfx::AffinityChargeShot)
                == 0x4000 + 8 * 10 + 9,
            "synthetic beam table mismatch");
    require(metadata.hunter(1, fruityprime::sound::HunterSfx::Morph) == -1,
            "synthetic hunter sentinel mismatch");
    require(metadata.hunter(7, fruityprime::sound::HunterSfx::MissileCharge)
                == 0x5000 + 7 * 17 + 16,
            "synthetic hunter table mismatch");
    require(metadata.platform(44, 3) == 0x6000 + 44 * 4 + 3,
            "synthetic platform table mismatch");
    require(metadata.hunter(255, fruityprime::sound::HunterSfx::Damage) == -1
                && metadata.platform(0, 4) == -1
                && metadata.enemy_damage(255) == -1,
            "synthetic out-of-range lookup mismatch");
}

void write_bytes(const std::filesystem::path& path,
                 const std::vector<std::uint8_t>& bytes) {
    std::ofstream output(path, std::ios::binary | std::ios::trunc);
    output.write(reinterpret_cast<const char*>(bytes.data()),
                 static_cast<std::streamsize>(bytes.size()));
    require(static_cast<bool>(output), "synthetic runtime file write failed");
}

void test_extracted_tables() {
    const auto root = std::filesystem::temp_directory_path()
        / ("fruity_prime_sound_metadata_"
           + std::to_string(std::chrono::steady_clock::now()
                                .time_since_epoch().count()));
    const auto bins = root / "A76E0" / "_bin";
    try {
        std::filesystem::create_directories(bins);
        std::vector<std::uint8_t> arm9(0x9B644 + 208, 0);
        std::vector<std::uint8_t> overlay2(0x1D96C + 272, 0);
        std::vector<std::uint8_t> platform(0x81E4 + 360, 0);
        put_u32(arm9, 0x9B574, 0x12345678);
        arm9[0x95C68] = 7;
        arm9[0x95A88] = 0xFF;
        arm9[0x96348] = 0xA3;
        put_u16(overlay2, 0x1D828, 0x2345);
        put_u16(platform, 0x81E4, 0x3456);
        write_bytes(bins / "arm9.bin", arm9);
        write_bytes(bins / "overlay9_2", overlay2);
        write_bytes(bins / "overlay9_12", platform);

        const auto store = fruityprime::assets::Store::from_directory(
            root / "A76E0");
        require(store.game_key().has_value()
                    && *store.game_key() == "A76E0",
                "extracted Store did not retain its game key");
        const auto metadata = fruityprime::sound::SoundMetadata::load(store);
        require(metadata.has_value() && metadata->game_key == "A76E0",
                "Extract.LoadRuntimeData layout was not selected");
        require(metadata->enemy_damage(0) == 0x12345678
                    && metadata->terrain(0,
                        fruityprime::sound::TerrainSfx::Walk1) == 0x2345
                    && metadata->platform(0, 0) == 0x3456,
                "extracted runtime sound tables were not loaded");
        require(fruityprime::utility::extract::load_runtime_data(store),
                "Extract.LoadRuntimeData rejected an MPH layout");
        const auto& font = fruityprime::strings::Font::normal();
        require(font.min_character() == 32 && font.widths().size() == 480
                    && font.widths()[0] == 7 && font.offsets()[0] == -1
                    && font.character_data().size() == 0x8000
                    && font.character_data()[0] == 3
                    && font.character_data()[1] == 10,
                "Extract.LoadRuntimeData did not initialize the normal font");
        std::filesystem::remove_all(root);
    } catch (...) {
        std::error_code ignored;
        std::filesystem::remove_all(root, ignored);
        throw;
    }
}

void test_real_rom(const std::filesystem::path& path) {
    if (path.empty()) {
        std::cout << "sound metadata ROM test skipped: set "
                     "FRUITY_PRIME_TEST_NDS\n";
        return;
    }
    require(std::filesystem::is_regular_file(path),
            "configured NDS ROM is not a regular file: " + path.string());

    const auto metadata = fruityprime::sound::SoundMetadata::load(
        fruityprime::assets::Store::from_rom(path));
    require(metadata.has_value(),
            "sound metadata did not recognize the configured ROM version");
    require(metadata->game_key == "AMHE1",
            "sound metadata selected the wrong ROM layout");

    std::size_t hunter_values = 0;
    std::size_t beam_values = 0;
    std::size_t terrain_values = 0;
    std::size_t platform_values = 0;
    std::size_t enemy_damage_values = 0;
    std::size_t enemy_death_values = 0;
    for (const auto& row : metadata->hunter_sfx) {
        for (const auto value : row) {
            hunter_values += value >= 0 ? 1 : 0;
        }
    }
    for (const auto& row : metadata->beam_sfx) {
        for (const auto value : row) {
            beam_values += value >= 0 ? 1 : 0;
        }
    }
    for (const auto& row : metadata->terrain_sfx) {
        for (const auto value : row) {
            terrain_values += value >= 0 ? 1 : 0;
        }
    }
    for (const auto& row : metadata->platform_sfx) {
        for (const auto value : row) {
            platform_values += value >= 0 ? 1 : 0;
        }
    }
    for (const auto value : metadata->enemy_damage_sfx) {
        enemy_damage_values += value >= 0 ? 1 : 0;
    }
    for (const auto value : metadata->enemy_death_sfx) {
        enemy_death_values += value >= 0 ? 1 : 0;
    }

    require(hunter_values > 0 && beam_values > 0 && terrain_values > 0
                && platform_values > 0 && enemy_damage_values > 0
                && enemy_death_values > 0,
            "real ROM sound metadata tables are empty");
    std::cout << "real sound metadata: key=" << metadata->game_key
              << " hunter=" << hunter_values << "/"
              << (fruityprime::sound::SoundMetadata::HunterCount
                  * fruityprime::sound::SoundMetadata::HunterSfxCount)
              << " beam=" << beam_values << "/"
              << (fruityprime::sound::SoundMetadata::BeamCount
                  * fruityprime::sound::SoundMetadata::BeamSfxCount)
              << " terrain=" << terrain_values << "/"
              << (fruityprime::sound::SoundMetadata::TerrainCount
                  * fruityprime::sound::SoundMetadata::TerrainSfxCount)
              << " platform=" << platform_values << "/"
              << (fruityprime::sound::SoundMetadata::PlatformCount
                  * fruityprime::sound::SoundMetadata::PlatformSfxCount)
              << " enemy_damage=" << enemy_damage_values << "/"
              << fruityprime::sound::SoundMetadata::EnemyCount
              << " enemy_death=" << enemy_death_values << "/"
              << fruityprime::sound::SoundMetadata::EnemyCount << '\n';
}

} // namespace

int main(int argc, char** argv) {
    try {
        test_synthetic_tables();
        test_extracted_tables();
        std::filesystem::path rom_path;
        if (argc > 1 && argv[1] != nullptr && argv[1][0] != '\0') {
            rom_path = argv[1];
        } else if (const char* configured = std::getenv(
                       "FRUITY_PRIME_TEST_NDS");
                   configured != nullptr && configured[0] != '\0') {
            rom_path = configured;
        }
        test_real_rom(rom_path);
        std::cout << "native sound metadata tests passed\n";
        return 0;
    } catch (const std::exception& error) {
        std::cerr << "native sound metadata tests failed: " << error.what()
                  << '\n';
        return 1;
    }
}
