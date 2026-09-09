#pragma once

#include "Assets/game_assets.hpp"
#include "Sound/sdat.hpp"
#include "Metadata/sound_metadata.hpp"
#include "Sound/sound_resources.hpp"

#include <array>
#include <cstdint>
#include <optional>
#include <span>
#include <string>
#include <vector>

namespace fruityprime::sound {

// The managed sound reader exposes these tables as value objects.  Keep the
// raw fields as well: several IDs are interpreted differently by BGM and SFX
// lists, and callers should not have to guess which conversion was applied.
struct SelectEntry {
    std::string name;
    std::uint16_t id = 0;
    std::uint16_t type = 0;
    std::uint32_t field4 = 0;
    std::uint32_t field8 = 0;
    std::uint32_t field_c = 0;
};

struct Sound3dEntry {
    std::uint32_t falloff_distance = 0;
    std::uint32_t max_distance = 0;
};

struct SoundTableEntry {
    std::string name;
    std::string category;
    std::uint16_t exists = 0;
    std::uint8_t category_id = 0;
    std::uint8_t slot_count = 0;
    std::uint8_t initial_volume = 0;
    std::uint8_t priority = 0;
    std::uint16_t size = 0;
    std::uint32_t data = 0;
};

struct SoundTable {
    std::vector<SoundTableEntry> entries;
    std::vector<std::string> categories;
};

struct RoomMusic {
    std::uint16_t room_id = 0;
    std::array<std::uint16_t, 3> track_ids{};
};

struct MusicTrack {
    std::uint32_t id = 0;
    std::optional<std::uint16_t> sequence_id;
    std::uint16_t fade_out_frames = 0;
    std::uint16_t tracks = 0;
    std::uint16_t fade_in_frames = 0;
};

struct SfxScriptHeader {
    std::uint32_t offset = 0;
    std::uint16_t size = 0;
    std::uint8_t initial_volume = 0;
    std::uint8_t slot_count = 0;
};

struct SfxScriptEntry {
    std::uint16_t sfx_id = 0;
    std::uint16_t delay_frames = 0;
    std::uint8_t volume = 0;
    std::uint8_t pan = 0;
    std::uint16_t pitch = 0;
    // This field is assigned by the runtime.  It is zero in the cartridge
    // table and retained here so the native representation remains writable.
    std::uint32_t handle = 0;
};

struct SfxScriptFile {
    std::string name;
    SfxScriptHeader header;
    std::vector<SfxScriptEntry> entries;
};

struct DgnHeader {
    std::uint32_t offset = 0;
    std::uint16_t size = 0;
    std::uint8_t initial_volume = 0;
    std::uint8_t slot_count = 0;
};

struct DgnData {
    std::uint16_t amount = 0;
    std::uint16_t value = 0;
};

struct DgnEntry {
    std::uint16_t unused = 0;
    std::uint16_t sfx_id = 0;
    std::array<std::vector<DgnData>, 4> data;
};

struct DgnFile {
    std::string name;
    DgnHeader header;
    std::vector<DgnEntry> entries;
};

[[nodiscard]] std::vector<SelectEntry> parse_select_list(
    std::span<const std::uint8_t> bytes);

[[nodiscard]] std::vector<Sound3dEntry> parse_sound_3d_list(
    std::span<const std::uint8_t> bytes);

[[nodiscard]] SoundTable parse_sound_tables(
    std::span<const std::uint8_t> bytes);

[[nodiscard]] std::vector<RoomMusic> parse_assign_music(
    std::span<const std::uint8_t> bytes);

[[nodiscard]] std::vector<MusicTrack> parse_inter_music_info(
    std::span<const std::uint8_t> bytes);

[[nodiscard]] std::vector<SfxScriptFile> parse_sfx_script_files(
    std::span<const std::uint8_t> bytes);

[[nodiscard]] std::vector<DgnFile> parse_dgn_files(
    std::span<const std::uint8_t> bytes);

// All sound assets used by the managed SoundRead facade, loaded through the
// same directory-or-ROM Store as rooms and models.  The SDAT stream list is
// intentionally kept alongside the custom MPH tables because the game uses
// both sources at runtime.
struct Catalog {
    std::vector<Sample> samples;
    std::vector<Sample> wfs_samples;
    std::vector<SelectEntry> bgm_select;
    std::vector<SelectEntry> sfx_select;
    std::vector<Sound3dEntry> sound_3d;
    SoundTable sound_tables;
    std::vector<RoomMusic> room_music;
    std::vector<MusicTrack> music_tracks;
    std::vector<SfxScriptFile> sfx_script_files;
    std::vector<DgnFile> dgn_files;
    std::optional<SoundMetadata> metadata;
    std::optional<Sdat> sdat;
    std::vector<Stream> streams;

    [[nodiscard]] static Catalog load(const assets::Store& assets);
};

} // namespace fruityprime::sound
