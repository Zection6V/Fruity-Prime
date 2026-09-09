#include "Testing/test_misc.hpp"

#include <array>
#include <cassert>
#include <cstdint>
#include <exception>
#include <filesystem>
#include <fstream>
#include <string>
#include <vector>

namespace {

void append_u32(std::vector<std::uint8_t>& bytes, std::uint32_t value) {
    bytes.push_back(static_cast<std::uint8_t>(value));
    bytes.push_back(static_cast<std::uint8_t>(value >> 8));
    bytes.push_back(static_cast<std::uint8_t>(value >> 16));
    bytes.push_back(static_cast<std::uint8_t>(value >> 24));
}

void put_u32(std::vector<std::uint8_t>& bytes, std::size_t offset,
             std::uint32_t value) {
    bytes.at(offset) = static_cast<std::uint8_t>(value);
    bytes.at(offset + 1) = static_cast<std::uint8_t>(value >> 8);
    bytes.at(offset + 2) = static_cast<std::uint8_t>(value >> 16);
    bytes.at(offset + 3) = static_cast<std::uint8_t>(value >> 24);
}

std::vector<std::uint8_t> make_fvds() {
    std::vector<std::uint8_t> bytes;
    bytes.insert(bytes.end(), {'F', 'V', 'D', 'S'});
    append_u32(bytes, 2);       // frame count
    append_u32(bytes, 256);     // width
    append_u32(bytes, 192);     // height
    append_u32(bytes, 983035);  // 15 fps in 16.16 fixed point
    append_u32(bytes, 32768);   // audio sample rate
    append_u32(bytes, 0);       // total data size, patched below
    append_u32(bytes, 328);     // max frame data size
    append_u32(bytes, 4608);    // managed field24

    // The compressed video payload is intentionally empty here; this fixture
    // exercises the FVDS container and seek/audio framing independently from
    // the large golden-video corpus used by the original C# developer test.
    for (std::uint32_t frame = 0; frame < 2; ++frame) {
        append_u32(bytes, 328); // video + audio + two size fields
        append_u32(bytes, frame == 0 ? 0 : static_cast<std::uint32_t>(-344));
        append_u32(bytes, 0);   // seek ahead
        append_u32(bytes, frame);
        append_u32(bytes, 0);   // video size
        append_u32(bytes, 320); // eight 40-byte audio frames
        bytes.insert(bytes.end(), 320, 0);
    }
    bytes.insert(bytes.end(), 16, 0);
    put_u32(bytes, 24, static_cast<std::uint32_t>(bytes.size() - 36 - 16));
    return bytes;
}

std::vector<std::uint8_t> make_camera_sequence() {
    std::vector<std::uint8_t> bytes;
    bytes.push_back(1);
    bytes.push_back(0);
    bytes.push_back(1);
    bytes.push_back(0);
    append_u32(bytes, 0); // header padding
    bytes.insert(bytes.end(), 100, 0);
    return bytes;
}

} // namespace

int main() {
    using fruityprime::testing::misc::parse_fv;
    using fruityprime::testing::misc::test_camera_sequence_files;
    using fruityprime::testing::misc::test_camera_shake;
    using fruityprime::utility::Rng;

    const auto bytes = make_fvds();
    const auto file = parse_fv(bytes);
    const std::array<char, 4> expected_magic{'F', 'V', 'D', 'S'};
    assert(file.header.magic == expected_magic);
    assert(file.header.frame_count == 2);
    assert(file.frames.size() == 2);
    assert(file.frames[1].seek_back_offset == -344);
    assert(file.found_max_data_size);
    assert(file.audio_frame_total() == 16);

    auto invalid = bytes;
    invalid[0] = 'X';
    bool rejected = false;
    try {
        static_cast<void>(parse_fv(invalid));
    } catch (const std::exception&) {
        rejected = true;
    }
    assert(rejected);

    const auto camera_root = std::filesystem::temp_directory_path()
        / "fruity_prime_test_misc_camera";
    std::error_code error;
    std::filesystem::remove_all(camera_root, error);
    std::filesystem::create_directories(camera_root);
    {
        std::ofstream output(camera_root / "intro.bin", std::ios::binary);
        const auto camera = make_camera_sequence();
        output.write(reinterpret_cast<const char*>(camera.data()),
                     static_cast<std::streamsize>(camera.size()));
    }
    {
        std::ofstream ignored(camera_root / "cameraEditBG.bin",
                              std::ios::binary);
        ignored.put('\0');
    }
    const auto camera_report = test_camera_sequence_files(camera_root);
    assert(camera_report.attempted == 1);
    assert(camera_report.loaded == 1);
    assert(camera_report.keyframes == 1);
    std::filesystem::remove_all(camera_root, error);
    assert(!error);

    Rng rng;
    const auto shake = test_camera_shake(rng);
    assert(shake.frames > 0);
    assert(shake.calls == shake.frames * 3);
    assert(shake.before != shake.after);

    return 0;
}
