#include "Formats/sound_layouts.hpp"
#include "Formats/sound_catalog.hpp"

#include <algorithm>
#include <limits>
#include <stdexcept>
#include <string_view>
#include <utility>

namespace fruityprime::sound {
namespace {

using Bytes = std::span<const std::uint8_t>;

[[noreturn]] void invalid(std::string_view message) {
    throw std::runtime_error("invalid MPH sound catalog: "
                             + std::string(message));
}

void require(bool condition, std::string_view message) {
    if (!condition) {
        invalid(message);
    }
}

void require_range(Bytes bytes, std::size_t offset, std::size_t length,
                   std::string_view what) {
    if (offset > bytes.size() || length > bytes.size() - offset) {
        invalid(std::string(what) + " is outside its file");
    }
}

[[nodiscard]] std::uint16_t u16(Bytes bytes, std::size_t offset) {
    require_range(bytes, offset, 2, "16-bit value");
    return static_cast<std::uint16_t>(bytes[offset])
        | static_cast<std::uint16_t>(bytes[offset + 1]) << 8;
}

[[nodiscard]] std::uint32_t u32(Bytes bytes, std::size_t offset) {
    require_range(bytes, offset, 4, "32-bit value");
    return static_cast<std::uint32_t>(bytes[offset])
        | static_cast<std::uint32_t>(bytes[offset + 1]) << 8
        | static_cast<std::uint32_t>(bytes[offset + 2]) << 16
        | static_cast<std::uint32_t>(bytes[offset + 3]) << 24;
}

struct StringBlock {
    std::vector<std::string> values;
    std::size_t next = 0;
};

[[nodiscard]] StringBlock strings(Bytes bytes, std::size_t offset,
                                   std::size_t count) {
    require(offset <= bytes.size(), "string table starts outside its file");
    StringBlock result;
    result.values.reserve(count);
    for (std::size_t i = 0; i < count; ++i) {
        const std::size_t begin = offset;
        while (offset < bytes.size() && bytes[offset] != 0) {
            ++offset;
        }
        require(offset < bytes.size(), "string table is not null terminated");
        result.values.emplace_back(
            reinterpret_cast<const char*>(bytes.data() + begin),
            offset - begin);
        ++offset;
    }
    result.next = offset;
    return result;
}

[[nodiscard]] std::size_t count_with_records(Bytes bytes, std::size_t record_size,
                                             std::string_view what) {
    require_range(bytes, 0, 4, what);
    const std::uint32_t count = u32(bytes, 0);
    require(count <= (bytes.size() - 4) / record_size,
            std::string(what) + " contains too many records");
    return count;
}

[[nodiscard]] std::size_t records_end(std::size_t count,
                                      std::size_t record_size) {
    constexpr std::size_t max_size = std::numeric_limits<std::size_t>::max();
    require(count <= (max_size - 4) / record_size,
            "sound catalog record count overflows its offset");
    return 4 + count * record_size;
}

[[nodiscard]] std::vector<DgnData> dgn_data(Bytes bytes, std::size_t base,
                                             std::uint32_t offset,
                                             std::uint32_t count) {
    if (count == 0) {
        return {};
    }
    require(offset != 0, "DGN data has a missing non-empty offset");
    require(offset <= std::numeric_limits<std::size_t>::max() - base,
            "DGN data offset overflows its base");
    const std::size_t start = base + offset;
    require(count <= (std::numeric_limits<std::size_t>::max() - start) / 4,
            "DGN data count overflows its range");
    require_range(bytes, start, static_cast<std::size_t>(count) * 4,
                  "DGN data");
    std::vector<DgnData> result;
    result.reserve(count);
    for (std::uint32_t i = 0; i < count; ++i) {
        const std::size_t item = start + static_cast<std::size_t>(i) * 4;
        result.push_back(DgnData{u16(bytes, item), u16(bytes, item + 2)});
    }
    return result;
}

} // namespace

std::vector<SelectEntry> parse_select_list(Bytes bytes) {
    const std::size_t count = count_with_records(
        bytes, 16, "sound select list");
    const std::size_t end = records_end(count, 16);
    const auto names = strings(bytes, end, count);
    std::vector<SelectEntry> result;
    result.reserve(count);
    for (std::size_t i = 0; i < count; ++i) {
        const std::size_t offset = 4 + i * 16;
        result.push_back(SelectEntry{
            names.values[i], u16(bytes, offset), u16(bytes, offset + 2),
            u32(bytes, offset + 4), u32(bytes, offset + 8),
            u32(bytes, offset + 12)
        });
    }
    return result;
}

std::vector<Sound3dEntry> parse_sound_3d_list(Bytes bytes) {
    const std::size_t count = count_with_records(
        bytes, 8, "3D sound list");
    std::vector<Sound3dEntry> result;
    result.reserve(count);
    for (std::size_t i = 0; i < count; ++i) {
        const std::size_t offset = 4 + i * 8;
        result.push_back(Sound3dEntry{
            u32(bytes, offset), u32(bytes, offset + 4)
        });
    }
    return result;
}

SoundTable parse_sound_tables(Bytes bytes) {
    const std::size_t count = count_with_records(
        bytes, 12, "sound table");
    const std::size_t end = records_end(count, 12);
    const auto names = strings(bytes, end, count);

    std::uint8_t max_category = 0;
    for (std::size_t i = 0; i < count; ++i) {
        max_category = std::max(max_category, bytes[4 + i * 12 + 2]);
    }
    const std::size_t category_count = count == 0
        ? 0 : static_cast<std::size_t>(max_category) + 1;
    const auto categories = strings(bytes, names.next, category_count);

    SoundTable result;
    result.categories = categories.values;
    result.entries.reserve(count);
    for (std::size_t i = 0; i < count; ++i) {
        const std::size_t offset = 4 + i * 12;
        const std::uint8_t category_id = bytes[offset + 2];
        require(category_id < result.categories.size(),
                "sound table category ID is outside its string table");
        result.entries.push_back(SoundTableEntry{
            names.values[i], result.categories[category_id],
            u16(bytes, offset), category_id, bytes[offset + 3],
            bytes[offset + 4], bytes[offset + 5], u16(bytes, offset + 6),
            u32(bytes, offset + 8)
        });
    }
    return result;
}

std::vector<RoomMusic> parse_assign_music(Bytes bytes) {
    const std::size_t count = count_with_records(
        bytes, 8, "room music table");
    std::vector<RoomMusic> result;
    result.reserve(count);
    for (std::size_t i = 0; i < count; ++i) {
        const std::size_t offset = 4 + i * 8;
        result.push_back(RoomMusic{
            u16(bytes, offset),
            {u16(bytes, offset + 2), u16(bytes, offset + 4),
             u16(bytes, offset + 6)}
        });
    }
    return result;
}

std::vector<MusicTrack> parse_inter_music_info(Bytes bytes) {
    const std::size_t count = count_with_records(
        bytes, 8, "intermission music table");
    std::vector<MusicTrack> result;
    result.reserve(count);
    for (std::size_t i = 0; i < count; ++i) {
        const std::size_t offset = 4 + i * 8;
        const std::uint16_t sequence = u16(bytes, offset);
        result.push_back(MusicTrack{
            static_cast<std::uint32_t>(i),
            sequence == std::numeric_limits<std::uint16_t>::max()
                ? std::nullopt : std::optional<std::uint16_t>(sequence),
            u16(bytes, offset + 2), u16(bytes, offset + 4),
            u16(bytes, offset + 6)
        });
    }
    return result;
}

std::vector<SfxScriptFile> parse_sfx_script_files(Bytes bytes) {
    const std::size_t count = count_with_records(
        bytes, 8, "SFX script file table");
    const std::size_t header_end = records_end(count, 8);
    std::vector<SfxScriptHeader> headers;
    headers.reserve(count);
    for (std::size_t i = 0; i < count; ++i) {
        const std::size_t offset = 4 + i * 8;
        headers.push_back(SfxScriptHeader{
            u32(bytes, offset), u16(bytes, offset + 4), bytes[offset + 6],
            bytes[offset + 7]
        });
    }

    std::size_t names_offset = header_end;
    if (!headers.empty()) {
        const auto& last = headers.back();
        require(last.offset <= std::numeric_limits<std::size_t>::max()
                    - last.size,
                "SFX script name offset overflows");
        names_offset = static_cast<std::size_t>(last.offset) + last.size;
    }
    const auto names = strings(bytes, names_offset, count);

    std::vector<SfxScriptFile> result;
    result.reserve(count);
    for (std::size_t i = 0; i < count; ++i) {
        const auto& header = headers[i];
        const std::size_t base = header.offset;
        require_range(bytes, base, 4, "SFX script");
        const std::size_t entry_count = count_with_records(
            bytes.subspan(base), 12, "SFX script");
        std::vector<SfxScriptEntry> entries;
        entries.reserve(entry_count);
        for (std::size_t j = 0; j < entry_count; ++j) {
            const std::size_t offset = base + 4 + j * 12;
            entries.push_back(SfxScriptEntry{
                u16(bytes, offset), u16(bytes, offset + 2),
                bytes[offset + 4], bytes[offset + 5],
                u16(bytes, offset + 6), u32(bytes, offset + 8)
            });
        }
        result.push_back(SfxScriptFile{names.values[i], header,
                                       std::move(entries)});
    }
    return result;
}

std::vector<DgnFile> parse_dgn_files(Bytes bytes) {
    const std::size_t count = count_with_records(
        bytes, 8, "DGN file table");
    const std::size_t header_end = records_end(count, 8);
    std::vector<DgnHeader> headers;
    headers.reserve(count);
    for (std::size_t i = 0; i < count; ++i) {
        const std::size_t offset = 4 + i * 8;
        headers.push_back(DgnHeader{
            u32(bytes, offset), u16(bytes, offset + 4), bytes[offset + 6],
            bytes[offset + 7]
        });
    }

    std::size_t names_offset = header_end;
    if (!headers.empty()) {
        const auto& last = headers.back();
        require(last.offset <= std::numeric_limits<std::size_t>::max()
                    - last.size,
                "DGN name offset overflows");
        names_offset = static_cast<std::size_t>(last.offset) + last.size;
    }
    const auto names = strings(bytes, names_offset, count);

    std::vector<DgnFile> result;
    result.reserve(count);
    for (std::size_t i = 0; i < count; ++i) {
        const auto& header = headers[i];
        const std::size_t base = header.offset;
        require_range(bytes, base, 4, "DGN file");
        const std::uint32_t entry_count = u32(bytes, base);
        require(entry_count <= (bytes.size() - base - 4) / 36,
                "DGN file contains too many entries");
        std::vector<DgnEntry> entries;
        entries.reserve(entry_count);
        for (std::uint32_t j = 0; j < entry_count; ++j) {
            const std::size_t offset = base + 4 + static_cast<std::size_t>(j) * 36;
            DgnEntry entry;
            entry.unused = u16(bytes, offset);
            entry.sfx_id = u16(bytes, offset + 2);
            for (std::size_t group = 0; group < 4; ++group) {
                const std::size_t group_offset = offset + 4 + group * 8;
                const std::uint32_t item_count = u32(bytes, group_offset);
                const std::uint32_t item_offset = u32(bytes, group_offset + 4);
                entry.data[group] = dgn_data(bytes, base, item_offset,
                                              item_count);
            }
            entries.push_back(std::move(entry));
        }
        result.push_back(DgnFile{names.values[i], header, std::move(entries)});
    }
    return result;
}

Catalog Catalog::load(const assets::Store& assets) {
    Catalog result;
    result.metadata = SoundMetadata::load(assets);
    result.samples = parse_sample_table(assets.bytes(
        "data/sound/SNDSAMPLES.DAT"));
    result.wfs_samples = parse_sample_table(assets.bytes(
        "data/sound/WFSSNDSAMPLES.DAT"));
    result.bgm_select = parse_select_list(assets.bytes(
        "data/sound/BGMSELECTLIST.DAT"));
    result.sfx_select = parse_select_list(assets.bytes(
        "data/sound/SFXSELECTLIST.DAT"));
    result.sound_3d = parse_sound_3d_list(assets.bytes(
        "data/sound/SND3DLIST.DAT"));
    result.sound_tables = parse_sound_tables(assets.bytes(
        "data/sound/SNDTBLS.DAT"));
    result.room_music = parse_assign_music(assets.bytes(
        "data/sound/ASSIGNMUSIC.DAT"));
    result.music_tracks = parse_inter_music_info(assets.bytes(
        "data/sound/INTERMUSICINFO.DAT"));
    result.sfx_script_files = parse_sfx_script_files(assets.bytes(
        "data/sound/SFXSCRIPTFILES.DAT"));
    result.dgn_files = parse_dgn_files(assets.bytes(
        "data/sound/DGNFILES.DAT"));

    const auto sdat_bytes = assets.bytes("data/sound/sound_data.sdat");
    result.sdat = Sdat::parse(sdat_bytes);
    for (std::size_t i = 0; i < result.sdat->streams().size(); ++i) {
        const auto& info = result.sdat->streams()[i];
        if (!info.present) {
            continue;
        }
        std::string name = i < result.sdat->stream_names().size()
            ? result.sdat->stream_names()[i] : std::string{};
        auto stream = Stream::parse(
            result.sdat->file(info.file_id), info.file_id, std::move(name));
        stream.volume = info.volume / 127.0F;
        result.streams.push_back(std::move(stream));
    }
    return result;
}


std::vector<std::uint8_t> interleave_stream_channels(
    std::span<const std::vector<std::uint8_t>> channels, WaveFormat format) {
    if (channels.empty()) {
        return {};
    }
    const std::size_t bytes_per_sample = format == WaveFormat::Adpcm ? 2u : 1u;
    const std::size_t channel_length = channels[0].size();
    std::vector<std::uint8_t> data(channel_length * channels.size(), 0);
    for (std::size_t i = 0; i < channels.size(); ++i) {
        const auto& channel = channels[i];
        std::size_t destination = i * bytes_per_sample;
        std::size_t source = 0;
        while (source + bytes_per_sample <= channel.size()
               && destination + bytes_per_sample <= data.size()) {
            for (std::size_t k = 0; k < bytes_per_sample; ++k) {
                data[destination++] = channel[source++];
            }
            // Skip the other channels' slots for this sample.
            destination += bytes_per_sample * (channels.size() - 1);
        }
    }
    return data;
}

} // namespace fruityprime::sound
