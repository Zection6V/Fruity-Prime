#pragma once

#include "Utility/repack_collision.hpp"
#include "Utility/rng.hpp"

#include <array>
#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <span>
#include <string>
#include <string_view>
#include <vector>

namespace fruityprime::assets {
class Store;
}

namespace fruityprime::testing::misc {

// FVDS is the intermediate movie format used by Testing/TestMisc.cs. It is
// distinct from the cartridge VXDS stream handled by Formats/movie.cpp, so
// keep its framing explicit instead of treating the two formats as identical.
struct FvHeader {
    std::array<char, 4> magic{};
    std::int32_t frame_count = 0;
    std::int32_t frame_width = 0;
    std::int32_t frame_height = 0;
    std::int32_t frame_rate_fixed = 0;
    std::int32_t audio_sample_rate = 0;
    std::int32_t total_data_size = 0;
    std::int32_t max_data_size = 0;
    std::int32_t field_24 = 0;
};

struct FvFrame {
    std::size_t file_offset = 0;
    std::int32_t data_size = 0;
    std::int32_t seek_back_offset = 0;
    std::int32_t seek_ahead_offset = 0;
    std::int32_t frame_index = 0;
    std::vector<std::uint8_t> video;
    std::vector<std::uint8_t> audio;
};

struct FvFile {
    FvHeader header;
    std::vector<FvFrame> frames;
    std::array<std::int32_t, 4> trailer{};
    bool found_max_data_size = false;

    [[nodiscard]] std::size_t audio_frame_total() const noexcept {
        std::size_t total = 0;
        for (const auto& frame : frames) {
            total += frame.audio.size() / 40U;
        }
        return total;
    }
};

[[nodiscard]] FvFile parse_fv(std::span<const std::uint8_t> bytes);
[[nodiscard]] FvFile read_fv_file(const std::filesystem::path& path);

struct FvTestResult {
    std::filesystem::path path;
    FvFile file;
    bool verify_requested = false;

    [[nodiscard]] std::size_t frame_count() const noexcept {
        return file.frames.size();
    }
    [[nodiscard]] std::size_t audio_frame_total() const noexcept {
        return file.audio_frame_total();
    }
};

[[nodiscard]] FvTestResult test_fv(const std::filesystem::path& path,
                                   bool verify = false);
[[nodiscard]] std::vector<FvTestResult> test_all_fv(
    const std::filesystem::path& movies_directory);
[[nodiscard]] std::vector<FvTestResult> verify_fv(
    const std::filesystem::path& movies_directory);

struct CameraSequenceReport {
    std::size_t attempted = 0;
    std::size_t loaded = 0;
    std::size_t missing = 0;
    std::size_t keyframes = 0;
};

[[nodiscard]] CameraSequenceReport test_camera_sequence_files(
    const std::filesystem::path& camera_directory);
[[nodiscard]] CameraSequenceReport test_camera_sequences(
    const assets::Store& assets);

struct CollisionTestReport {
    std::size_t resources = 0;
    std::size_t mph = 0;
    std::size_t first_hunt = 0;
    std::size_t portals = 0;
};

[[nodiscard]] CollisionTestReport test_all_collision(
    const assets::Store& assets);
[[nodiscard]] CollisionTestReport test_all_fh_collision(
    const assets::Store& assets);

// These byte-producing entry points cover the collision portion of the
// managed room conversion helpers. Model/entity packing remains in its own
// utilities and is intentionally not hidden in this test module.
[[nodiscard]] std::vector<std::uint8_t> convert_room_to_mph(
    const assets::Store& assets, std::string_view room,
    utility::repack_collision::RepackFilter filter =
        utility::repack_collision::RepackFilter::All);
[[nodiscard]] std::vector<std::uint8_t> convert_room_to_fh(
    const assets::Store& assets, std::string_view room,
    utility::repack_collision::RepackFilter filter =
        utility::repack_collision::RepackFilter::All);

[[nodiscard]] utility::CameraShakeResult test_camera_shake(
    utility::Rng& rng, int shake = 204) noexcept;

} // namespace fruityprime::testing::misc
