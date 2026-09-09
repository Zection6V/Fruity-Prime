#include "Entities/CamSeq/CamSeqEntity.hpp"
#include "Assets/game_assets.hpp"
#include "Scene.hpp"

#include <cassert>
#include <chrono>
#include <cmath>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <stdexcept>
#include <string>
#include <vector>

namespace {

void put_u16(std::vector<std::uint8_t>& bytes, std::uint16_t value) {
    bytes.push_back(static_cast<std::uint8_t>(value));
    bytes.push_back(static_cast<std::uint8_t>(value >> 8));
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

std::vector<std::uint8_t> camera_bytes() {
    std::vector<std::uint8_t> bytes;
    put_u16(bytes, 1);
    bytes.push_back(1);
    bytes.push_back(0);
    put_u32(bytes, 0);
    put_vec3(bytes, 1.0F, 2.0F, 3.0F);
    put_vec3(bytes, 0.0F, 0.0F, 1.0F);
    put_fixed(bytes, 0.0F);  // roll
    put_fixed(bytes, 30.0F); // fov
    put_fixed(bytes, 0.0F);  // move time
    put_fixed(bytes, 0.0F);  // hold time
    put_fixed(bytes, 0.0F);  // fade in
    put_fixed(bytes, 0.0F);  // fade out
    bytes.insert(bytes.end(), {0, 0, 0, 0, 0, 0});
    put_u16(bytes, 0);
    for (int i = 0; i < 6; ++i) {
        put_u16(bytes, 0xffffU);
    }
    put_u16(bytes, 0);
    put_u16(bytes, 0);
    put_fixed(bytes, 1.0F);
    put_u32(bytes, 0);
    put_u32(bytes, 0);
    for (int i = 0; i < 16; ++i) {
        bytes.push_back(i == 0 ? 'r' : (i == 1 ? 'm' : 0));
    }
    if (bytes.size() != 108) {
        throw std::runtime_error("synthetic camera fixture has wrong size");
    }
    return bytes;
}

} // namespace

int main() {
    const auto root = std::filesystem::temp_directory_path()
        / ("fruity_prime_camseq_entity_"
           + std::to_string(std::chrono::high_resolution_clock::now()
                                .time_since_epoch()
                                .count()));
    try {
        std::filesystem::create_directories(root / "cameraEditor");
        {
            std::ofstream output(root / "cameraEditor/test.bin",
                                 std::ios::binary);
            const auto bytes = camera_bytes();
            output.write(reinterpret_cast<const char*>(bytes.data()),
                         static_cast<std::streamsize>(bytes.size()));
            if (!output) {
                throw std::runtime_error("could not write camera fixture");
            }
        }

        const auto assets = fruityprime::assets::Store::from_directory(root);
        fruityprime::scene_runtime::Scene scene(assets);
        fruityprime::scene::CameraSequenceData data;
        data.sequence_id = 0;
        data.delay_frames = 1;
        data.block_input = true;
        data.force_alt_form = true;
        data.end_message_target_id = 42;
        data.end_message = 0x1234;
        data.end_message_parameter = 9;
        fruityprime::entities::cam_seq::CameraSequenceEntity entity(
            scene,
            {data, 7, "cameraEditor/test.bin", "test_sequence"},
            fruityprime::camera::CameraState{
                {10.0F, 10.0F, 10.0F}, {}, {10.0F, 10.0F, 11.0F},
                {0.0F, 1.0F, 0.0F}, {0.0F, 0.0F, 1.0F}, 45.0F, 0.0F, {}});

        entity.handle_message(
            fruityprime::entities::cam_seq::CameraSequenceEntity::ActivateMessage);
        assert(entity.active());
        assert(!entity.started());
        entity.process();
        assert(entity.delay_timer() == 1 && !entity.started());
        entity.process();
        assert(entity.delay_timer() == 2 && !entity.started());
        entity.process();
        assert(entity.started());
        assert(scene.camera_sequence() != nullptr
               && scene.camera_sequence()->block_input()
               && scene.camera_sequence()->force_alt());

        scene.camera_sequence()->process(1.0F / 60.0F);
        entity.process();
        assert(!entity.active() && scene.camera_sequence() == nullptr);
        assert(scene.messages().size() == 1);
        const auto& end_message = scene.messages().entries().front();
        assert(end_message.cartridge_message == 0x1234);
        assert(end_message.sender == 7 && end_message.target == 42
               && end_message.parameter1 == 9);

        fruityprime::scene::CameraSequenceData loop_data;
        loop_data.sequence_id = 0;
        loop_data.loop = true;
        fruityprime::entities::cam_seq::CameraSequenceEntity loop_entity(
            scene,
            {loop_data, 8, "cameraEditor/test.bin", "loop_sequence"},
            fruityprime::camera::CameraState{
                {20.0F, 20.0F, 20.0F}, {}, {20.0F, 20.0F, 21.0F},
                {0.0F, 1.0F, 0.0F}, {0.0F, 0.0F, 1.0F}, 45.0F, 0.0F, {}});
        if (!loop_entity.activate()) {
            throw std::runtime_error("loop camera sequence did not activate");
        }
        assert(loop_entity.started());
        scene.camera_sequence()->process(1.0F / 60.0F);
        loop_entity.process();
        assert(loop_entity.active() && loop_entity.started()
               && scene.camera_sequence() != nullptr
               && !scene.camera_sequence()->complete());
        loop_entity.handle_message(
            fruityprime::entities::cam_seq::CameraSequenceEntity::SetActiveMessage,
            0);
        assert(loop_entity.handoff_timer()
               == fruityprime::entities::cam_seq::CameraSequenceEntity::HandoffFrames);
        const auto handoff_frames = loop_entity.handoff_timer();
        for (std::uint32_t i = 0; i < handoff_frames; ++i) {
            loop_entity.process();
        }
        assert(!loop_entity.active()
               && fruityprime::entities::cam_seq::CameraSequenceEntity::current()
                      == nullptr);

        std::filesystem::remove_all(root);
        std::cout << "camera sequence entity tests passed\n";
        return 0;
    } catch (const std::exception& error) {
        std::filesystem::remove_all(root);
        std::cerr << "camera sequence entity tests failed: " << error.what()
                  << '\n';
        return 1;
    }
}
