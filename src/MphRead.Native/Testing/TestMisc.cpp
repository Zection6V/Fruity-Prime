#include "Testing/test_misc.hpp"

#include "Utility/archive.hpp"
#include "Utility/binary_reader.hpp"
#include "Formats/camera_sequence.hpp"
#include "Formats/collision_format.hpp"
#include "Assets/game_assets.hpp"
#include "Entities/room_catalog.hpp"

#include <algorithm>
#include <fstream>
#include <stdexcept>
#include <string>
#include <string_view>
#include <utility>

namespace fruityprime::testing::misc {
namespace {

using Bytes = std::span<const std::uint8_t>;

[[nodiscard]] std::size_t checked_size(std::int32_t value,
                                       const char* description) {
    if (value < 0) {
        throw std::runtime_error(std::string(description)
                                 + " is negative");
    }
    return static_cast<std::size_t>(value);
}

[[nodiscard]] std::vector<std::uint8_t> read_all(
    const std::filesystem::path& path) {
    std::ifstream input(path, std::ios::binary | std::ios::ate);
    if (!input) {
        throw std::runtime_error("could not open FVDS file: "
                                 + path.string());
    }
    const auto length = input.tellg();
    if (length < 0) {
        throw std::runtime_error("could not determine FVDS file size: "
                                 + path.string());
    }
    std::vector<std::uint8_t> bytes(static_cast<std::size_t>(length));
    input.seekg(0);
    if (!bytes.empty()) {
        input.read(reinterpret_cast<char*>(bytes.data()),
                   static_cast<std::streamsize>(bytes.size()));
        if (!input) {
            throw std::runtime_error("could not read FVDS file: "
                                     + path.string());
        }
    }
    return bytes;
}

[[nodiscard]] bool has_fv_extension(const std::filesystem::path& path) {
    const std::string extension = path.extension().string();
    return extension == ".fv" || extension == ".FV";
}

[[nodiscard]] std::vector<std::uint8_t> room_collision_bytes(
    const assets::Store& assets, const scene::RoomDefinition& definition) {
    if (!definition.external_root.empty()) {
        const auto external = assets::Store::from_directory(
            definition.external_root);
        return external.bytes(definition.collision_entry);
    }
    const auto resource = assets.archive(definition.model_archive);
    for (std::size_t index = 0; index < resource.entries().size(); ++index) {
        if (resource.entries()[index].filename == definition.collision_entry) {
            return resource.file(index);
        }
    }
    throw std::out_of_range("room collision entry was not found: "
                            + definition.collision_entry);
}

[[nodiscard]] collision::File room_collision(
    const assets::Store& assets, std::string_view room) {
    const auto* entry = scene::find_room(room);
    if (entry == nullptr) {
        throw std::invalid_argument("unknown room: " + std::string(room));
    }
    return collision::File::from_bytes(room_collision_bytes(
        assets, entry->definition));
}

[[nodiscard]] CollisionTestReport test_collision_catalog(
    const assets::Store& assets, bool first_hunt_only) {
    CollisionTestReport result;
    const auto visit = [&](const std::vector<scene::RoomCatalogEntry>& rooms) {
        for (const auto& room : rooms) {
            const auto file = collision::File::from_bytes(
                room_collision_bytes(assets, room.definition));
            if (first_hunt_only && file.is_mph()) {
                continue;
            }
            ++result.resources;
            if (file.is_mph()) {
                ++result.mph;
                result.portals += file.mph().portals.size();
            } else {
                ++result.first_hunt;
                result.portals += file.first_hunt().portals.size();
            }
        }
    };
    visit(scene::story_rooms());
    visit(scene::multiplayer_rooms());
    return result;
}

} // namespace

FvFile parse_fv(Bytes bytes) {
    constexpr std::size_t HeaderSize = 36;
    constexpr std::size_t TrailerSize = 16;
    if (bytes.size() < HeaderSize + TrailerSize) {
        throw std::runtime_error("FVDS stream is smaller than header/trailer");
    }

    core::BinaryReader header_reader(bytes.subspan(0, HeaderSize));
    FvFile result;
    for (char& value : result.header.magic) {
        value = static_cast<char>(header_reader.read_u8());
    }
    if (result.header.magic != std::array<char, 4>{'F', 'V', 'D', 'S'}) {
        throw std::runtime_error("FVDS magic mismatch");
    }
    result.header.frame_count = header_reader.read_i32_le();
    result.header.frame_width = header_reader.read_i32_le();
    result.header.frame_height = header_reader.read_i32_le();
    result.header.frame_rate_fixed = header_reader.read_i32_le();
    result.header.audio_sample_rate = header_reader.read_i32_le();
    result.header.total_data_size = header_reader.read_i32_le();
    result.header.max_data_size = header_reader.read_i32_le();
    result.header.field_24 = header_reader.read_i32_le();

    if (result.header.frame_count <= 0
        || result.header.frame_width != 256
        || result.header.frame_height != 192
        || result.header.frame_rate_fixed != 983035
        || result.header.audio_sample_rate != 32768
        || result.header.field_24 != 4608) {
        throw std::runtime_error("FVDS header values are invalid");
    }
    if (result.header.max_data_size < 8) {
        throw std::runtime_error("FVDS maximum frame size is invalid");
    }
    if (bytes.size() - HeaderSize - TrailerSize
        != checked_size(result.header.total_data_size, "FVDS total data size")) {
        throw std::runtime_error("FVDS total data size does not match stream");
    }

    result.frames.reserve(static_cast<std::size_t>(result.header.frame_count));
    std::vector<std::size_t> frame_positions;
    frame_positions.reserve(result.frames.capacity());
    std::vector<std::size_t> skip_ahead_positions;
    std::size_t position = HeaderSize;
    for (std::int32_t index = 0; index < result.header.frame_count; ++index) {
        if (position > bytes.size() - TrailerSize
            || bytes.size() - TrailerSize - position < 24U) {
            throw std::runtime_error("FVDS frame header is outside the stream");
        }
        core::BinaryReader reader(bytes.subspan(
            position, bytes.size() - TrailerSize - position));
        FvFrame frame;
        frame.file_offset = position;
        frame.data_size = reader.read_i32_le();
        frame.seek_back_offset = reader.read_i32_le();
        frame.seek_ahead_offset = reader.read_i32_le();
        frame.frame_index = reader.read_i32_le();
        const std::size_t video_size = checked_size(reader.read_i32_le(),
                                                    "FVDS video size");
        if (video_size > reader.remaining()) {
            throw std::runtime_error("FVDS video payload is outside the stream");
        }
        const auto video = reader.read_bytes(video_size);
        const std::size_t audio_size = checked_size(reader.read_i32_le(),
                                                    "FVDS audio size");
        if (audio_size > reader.remaining()) {
            throw std::runtime_error("FVDS audio payload is outside the stream");
        }
        const auto audio = reader.read_bytes(audio_size);
        if (frame.frame_index != index) {
            throw std::runtime_error("FVDS frame index is not sequential");
        }
        if (frame.seek_back_offset > 0
            || (index == 0 && frame.seek_back_offset != 0)
            || (index == result.header.frame_count - 1
                && frame.seek_ahead_offset > 0)) {
            throw std::runtime_error("FVDS frame seek offsets are invalid");
        }
        if (frame.data_size < 0
            || static_cast<std::int64_t>(frame.data_size)
                != static_cast<std::int64_t>(video_size + audio_size + 8U)) {
            throw std::runtime_error("FVDS frame data size is inconsistent");
        }
        if (audio_size != 320U && audio_size != 360U) {
            throw std::runtime_error("FVDS audio frame size is unsupported");
        }
        if (static_cast<std::size_t>(frame.data_size)
            > static_cast<std::size_t>(result.header.max_data_size)) {
            throw std::runtime_error("FVDS frame exceeds maximum data size");
        }

        frame.video.assign(video.begin(), video.end());
        frame.audio.assign(audio.begin(), audio.end());
        result.found_max_data_size |= frame.data_size
            == result.header.max_data_size;
        frame_positions.push_back(frame.file_offset);
        if (frame.seek_back_offset < 0) {
            const auto target = static_cast<std::int64_t>(frame.file_offset)
                + frame.seek_back_offset;
            const auto found = target < 0 ? frame_positions.end() - 1
                : std::find(frame_positions.begin(), frame_positions.end() - 1,
                            static_cast<std::size_t>(target));
            if (target < 0 || found == frame_positions.end() - 1) {
                throw std::runtime_error("FVDS backward seek target is invalid");
            }
        }
        if (frame.seek_ahead_offset > 0) {
            const auto target = static_cast<std::int64_t>(frame.file_offset)
                + frame.seek_ahead_offset;
            if (target < 0) {
                throw std::runtime_error("FVDS forward seek target is invalid");
            }
            skip_ahead_positions.push_back(static_cast<std::size_t>(target));
        }
        result.frames.push_back(std::move(frame));
        skip_ahead_positions.erase(
            std::remove(skip_ahead_positions.begin(), skip_ahead_positions.end(),
                        result.frames.back().file_offset),
            skip_ahead_positions.end());
        position += 24U + video_size + audio_size;
    }

    if (!result.found_max_data_size) {
        throw std::runtime_error("FVDS stream has no maximum-sized frame");
    }
    if (!skip_ahead_positions.empty()) {
        throw std::runtime_error("FVDS forward seek target was not reached");
    }
    if (position + TrailerSize != bytes.size()) {
        throw std::runtime_error("FVDS frames do not end at the trailer");
    }
    core::BinaryReader trailer_reader(bytes.subspan(position, TrailerSize));
    for (std::int32_t& value : result.trailer) {
        value = trailer_reader.read_i32_le();
        if (value != 0) {
            throw std::runtime_error("FVDS trailer is not zero-filled");
        }
    }
    return result;
}

FvFile read_fv_file(const std::filesystem::path& path) {
    return parse_fv(read_all(path));
}

FvTestResult test_fv(const std::filesystem::path& path, bool verify) {
    return FvTestResult{path, read_fv_file(path), verify};
}

std::vector<FvTestResult> test_all_fv(
    const std::filesystem::path& movies_directory) {
    if (!std::filesystem::is_directory(movies_directory)) {
        throw std::invalid_argument("FVDS movie directory does not exist: "
                                    + movies_directory.string());
    }
    std::vector<std::filesystem::path> paths;
    for (const auto& entry :
         std::filesystem::directory_iterator(movies_directory)) {
        if (entry.is_regular_file() && has_fv_extension(entry.path())) {
            paths.push_back(entry.path());
        }
    }
    std::sort(paths.begin(), paths.end());
    std::vector<FvTestResult> result;
    result.reserve(paths.size());
    for (const auto& path : paths) {
        result.push_back(test_fv(path));
    }
    return result;
}

std::vector<FvTestResult> verify_fv(
    const std::filesystem::path& movies_directory) {
    const auto paths = test_all_fv(movies_directory);
    std::vector<FvTestResult> result;
    result.reserve(paths.size());
    for (auto entry : paths) {
        entry.verify_requested = true;
        result.push_back(std::move(entry));
    }
    return result;
}

CameraSequenceReport test_camera_sequence_files(
    const std::filesystem::path& camera_directory) {
    if (!std::filesystem::is_directory(camera_directory)) {
        throw std::invalid_argument("camera sequence directory does not exist: "
                                    + camera_directory.string());
    }
    std::vector<std::filesystem::path> paths;
    for (const auto& entry :
         std::filesystem::directory_iterator(camera_directory)) {
        if (!entry.is_regular_file()
            || entry.path().filename() == "cameraEditBG.bin") {
            continue;
        }
        paths.push_back(entry.path());
    }
    std::sort(paths.begin(), paths.end());
    CameraSequenceReport result;
    result.attempted = paths.size();
    for (const auto& path : paths) {
        const auto sequence = camera::File::read_file(path);
        ++result.loaded;
        result.keyframes += sequence.keyframes().size();
    }
    return result;
}

CameraSequenceReport test_camera_sequences(const assets::Store& assets) {
    CameraSequenceReport result;
    for (int id = 0; !camera::asset_name(id).empty(); ++id) {
        ++result.attempted;
        try {
            const auto sequence = camera::File::from_bytes(
                assets.bytes(camera::asset_path(id)), id,
                std::string(camera::asset_name(id)));
            ++result.loaded;
            result.keyframes += sequence.keyframes().size();
        } catch (const std::out_of_range&) {
            ++result.missing;
        }
    }
    return result;
}

CollisionTestReport test_all_collision(const assets::Store& assets) {
    return test_collision_catalog(assets, false);
}

CollisionTestReport test_all_fh_collision(const assets::Store& assets) {
    return test_collision_catalog(assets, true);
}

std::vector<std::uint8_t> convert_room_to_mph(
    const assets::Store& assets, std::string_view room,
    utility::repack_collision::RepackFilter filter) {
    static_cast<void>(filter);
    const auto source = room_collision(assets, room);
    if (!source.is_mph()) {
        throw std::invalid_argument("room collision is not an MPH resource: "
                                    + std::string(room));
    }
    return utility::repack_collision::repack(source);
}

std::vector<std::uint8_t> convert_room_to_fh(
    const assets::Store& assets, std::string_view room,
    utility::repack_collision::RepackFilter filter) {
    static_cast<void>(filter);
    const auto source = room_collision(assets, room);
    if (!source.is_first_hunt()) {
        throw std::invalid_argument(
            "room collision is not a First Hunt resource: "
            + std::string(room));
    }
    return utility::repack_collision::repack(source);
}

utility::CameraShakeResult test_camera_shake(utility::Rng& rng,
                                             int shake) noexcept {
    return rng.camera_shake(shake);
}

} // namespace fruityprime::testing::misc
