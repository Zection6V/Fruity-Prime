#include "../Formats/camera_sequence.hpp"
#include "Assets/game_assets.hpp"

#include <cassert>
#include <cmath>
#include <cstdlib>
#include <cstdint>
#include <iostream>
#include <optional>
#include <string>
#include <vector>

namespace {

void put_u16(std::vector<std::uint8_t>& bytes, std::uint16_t value) {
    bytes.push_back(static_cast<std::uint8_t>(value));
    bytes.push_back(static_cast<std::uint8_t>(value >> 8));
}

void put_i16(std::vector<std::uint8_t>& bytes, std::int16_t value) {
    put_u16(bytes, static_cast<std::uint16_t>(value));
}

void put_u32(std::vector<std::uint8_t>& bytes, std::uint32_t value) {
    bytes.push_back(static_cast<std::uint8_t>(value));
    bytes.push_back(static_cast<std::uint8_t>(value >> 8));
    bytes.push_back(static_cast<std::uint8_t>(value >> 16));
    bytes.push_back(static_cast<std::uint8_t>(value >> 24));
}

void put_fixed(std::vector<std::uint8_t>& bytes, float value) {
    put_u32(bytes, static_cast<std::uint32_t>(
        static_cast<std::int32_t>(std::lround(value * 4096.0F))));
}

void put_vec3(std::vector<std::uint8_t>& bytes, float x, float y, float z) {
    put_fixed(bytes, x);
    put_fixed(bytes, y);
    put_fixed(bytes, z);
}

void put_keyframe(std::vector<std::uint8_t>& bytes, float x, float fov,
                  float move_time, float hold_time, const char* node,
                  std::int16_t position_entity_type = -1,
                  std::int16_t position_entity_id = -1,
                  std::int16_t target_entity_type = -1,
                  std::int16_t target_entity_id = -1,
                  std::uint16_t message_id = 0,
                  std::uint16_t message_param = 0,
                   bool use_entity_transform = false,
                   std::int16_t message_target_type = -1,
                   std::int16_t message_target_id = -1) {
    put_vec3(bytes, x, 0.0F, 0.0F);
    put_vec3(bytes, 0.0F, 0.0F, 1.0F);
    put_fixed(bytes, 0.0F);
    put_fixed(bytes, fov);
    put_fixed(bytes, move_time);
    put_fixed(bytes, hold_time);
    put_fixed(bytes, 0.0F);
    put_fixed(bytes, 0.0F);
    bytes.push_back(0);
    bytes.push_back(0);
    bytes.push_back(0);
    bytes.push_back(0);
    bytes.push_back(use_entity_transform ? 1 : 0);
    bytes.push_back(0);
    put_u16(bytes, 0);
    put_i16(bytes, position_entity_type);
    put_i16(bytes, position_entity_id);
    put_i16(bytes, target_entity_type);
    put_i16(bytes, target_entity_id);
    put_i16(bytes, message_target_type);
    put_i16(bytes, message_target_id);
    put_u16(bytes, message_id);
    put_u16(bytes, message_param);
    put_fixed(bytes, 1.0F);
    put_u32(bytes, 0);
    put_u32(bytes, 0);
    for (int i = 0; i < 16; ++i) {
        bytes.push_back(node[i] == '\0' ? 0
                                         : static_cast<std::uint8_t>(node[i]));
    }
}

} // namespace

int main() {
    static_assert(fruityprime::camera::MusicData.size() == 199);
    static_assert(fruityprime::camera::SfxData.size() == 199);
    assert(fruityprime::camera::MusicData[0] == 28);
    assert(fruityprime::camera::MusicData[11] == (1 | 0x4000));
    assert(fruityprime::camera::MusicData[46] == (53 | 0x8000));
    assert(fruityprime::camera::SfxData[0] == (92 | 0x8000));
    assert(fruityprime::camera::SfxData[43] == (19 | 0x2000));
    assert(fruityprime::camera::SfxData[46] == (66 | 0x4000));

    assert(fruityprime::camera::asset_name(0)
           == "unit1_land_intro.bin");
    assert(fruityprime::camera::asset_path(198)
           == "cameraEditor/mp26_intro.bin");
    assert(fruityprime::camera::asset_name(-1).empty());
    assert(fruityprime::camera::asset_name(199).empty());

    std::vector<std::uint8_t> bytes;
    put_u16(bytes, 2);
    bytes.push_back(1);
    bytes.push_back(0);
    put_u32(bytes, 0);
    put_keyframe(bytes, 0.0F, 30.0F, 1.0F, 0.25F, "rmMain");
    put_keyframe(bytes, 10.0F, 40.0F, 0.0F, 0.0F, "rmExit");
    assert(bytes.size() == 8 + 2 * 100);

    const auto sequence = fruityprime::camera::File::from_bytes(
        std::move(bytes), 7, "synthetic.bin");
    assert(sequence.id() == 7);
    assert(sequence.name() == "synthetic.bin");
    assert(sequence.header().version == 1);
    assert(sequence.keyframes().size() == 2);
    assert(sequence.keyframes()[0].node_name == "rmMain");
    assert(std::fabs(sequence.duration() - 1.25F) < 0.001F);

    const auto held = sequence.sample(0, 0.10F);
    assert(std::fabs(held.position.x) < 0.001F);
    assert(std::fabs(held.fov - 60.0F) < 0.001F);
    assert(std::fabs(held.target.z - 1.0F) < 0.001F);

    const auto moved = sequence.sample(0, 0.75F);
    assert(std::fabs(moved.position.x - 5.0F) < 0.01F);
    assert(std::fabs(moved.fov - 70.0F) < 0.01F);

    bool rejected = false;
    try {
        auto invalid = std::vector<std::uint8_t>{1, 0, 1, 0, 0, 0, 0, 0};
        (void)fruityprime::camera::File::from_bytes(std::move(invalid));
    } catch (const std::exception&) {
        rejected = true;
    }
    assert(rejected);

    // Playback resolves position/target references before interpolation and
    // keeps the camera transition independent from Scene/OpenGL.
    std::vector<std::uint8_t> playback_bytes;
    put_u16(playback_bytes, 1);
    playback_bytes.push_back(1);
    playback_bytes.push_back(0);
    put_u32(playback_bytes, 0);
    put_keyframe(playback_bytes, 1.0F, 35.0F, 0.0F, 0.0F, "rmMain",
                 10, 7, 11, 9, 7, 4, true, 12, 13);
    const auto playback_file = fruityprime::camera::File::from_bytes(
        std::move(playback_bytes), 8, "playback.bin");
    fruityprime::camera::Playback playback(0, playback_file);
    playback.set_entity_resolver(
        [](std::int16_t type, std::int16_t id)
            -> std::optional<fruityprime::camera::EntityPose> {
            if (type == 10 && id == 7) {
                return fruityprime::camera::EntityPose{
                    {3.0F, 2.0F, 4.0F}, {0.0F, 1.0F, 0.0F},
                    {0.0F, 0.0F, 1.0F}};
            }
            if (type == 11 && id == 9) {
                return fruityprime::camera::EntityPose{
                    {4.0F, 2.0F, 9.0F}, {0.0F, 1.0F, 0.0F},
                    {0.0F, 0.0F, 1.0F}};
            }
            return std::nullopt;
        });
    fruityprime::camera::MessageEvent message_event;
    bool forced_alt = false;
    playback.set_message_sink(
        [&](const fruityprime::camera::MessageEvent& event) {
            message_event = event;
        });
    playback.set_form_lock_sink([&](bool alt_form) { forced_alt = alt_form; });
    playback.set_flags(static_cast<fruityprime::camera::Flags>(
        static_cast<std::uint8_t>(fruityprime::camera::Flags::ForceAlt)));
    playback.set_up(fruityprime::camera::CameraState{
        {100.0F, 100.0F, 100.0F}, {}, {100.0F, 100.0F, 101.0F},
        {0.0F, 1.0F, 0.0F}, {0.0F, 0.0F, 1.0F}, 10.0F, 0.0F, {}});
    assert(std::fabs(playback.camera().position.x - 4.0F) < 0.001F);
    assert(std::fabs(playback.camera().position.y - 2.0F) < 0.001F);
    assert(std::fabs(playback.camera().position.z - 4.0F) < 0.001F);
    assert(std::fabs(playback.camera().fov - 70.0F) < 0.001F);
    playback.process(1.0F / 60.0F);
    assert(message_event.message == 7);
    assert(message_event.parameter == 4);
    assert(message_event.target_type == 12);
    assert(message_event.target_id == 13);
    assert(forced_alt);
    assert(playback.complete() && playback.can_end());

    // Intrinsic multiplayer-intro sequences loop once their final frame has
    // elapsed, matching the managed loader's sequence-id rule.
    std::vector<std::uint8_t> loop_bytes;
    put_u16(loop_bytes, 1);
    loop_bytes.push_back(1);
    loop_bytes.push_back(0);
    put_u32(loop_bytes, 0);
    put_keyframe(loop_bytes, 0.0F, 30.0F, 0.0F, 0.5F, "rmMain");
    const auto loop_file = fruityprime::camera::File::from_bytes(
        std::move(loop_bytes));
    fruityprime::camera::Playback loop(172, loop_file);
    loop.set_up(fruityprime::camera::CameraState{}, 0);
    loop.process(1.0F);
    assert(!loop.complete());
    assert(loop.keyframe_index() == 0);

    if (const char* rom_path = std::getenv("FRUITY_PRIME_TEST_NDS");
        rom_path != nullptr && *rom_path != '\0') {
        try {
            const auto store = fruityprime::assets::Store::from_rom(rom_path);
            const auto real = fruityprime::camera::File::from_bytes(
                store.bytes("cameraEditor/unit1_land_intro.bin"), 0,
                "unit1_land_intro.bin");
            assert(!real.keyframes().empty());
            std::cout << "real camera sequence: file=unit1_land_intro.bin"
                      << " version=" << static_cast<int>(real.version())
                      << " keyframes=" << real.keyframes().size()
                      << " duration=" << real.duration() << "\n";
        } catch (const std::exception& error) {
            std::cerr << "real camera sequence probe skipped: "
                      << error.what() << "\n";
        }
    }

    std::cout << "camera sequence tests passed\n";
    return 0;
}
