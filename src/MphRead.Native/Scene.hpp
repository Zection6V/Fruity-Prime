#pragma once

#include "Formats/camera_sequence.hpp"
#include "Entities/CamSeq/CamSeqEntity.hpp"
#include "GameState.hpp"
#include "Entities/gameplay.hpp"
#include "Messaging.hpp"
#include "Entities/runtime_entities.hpp"
#include "Entities/static_entities.hpp"

#include <cstdint>
#include <memory>
#include <optional>
#include <string_view>
#include <vector>

namespace fruityprime::scene_runtime {

// Scene.cs equivalent. Asset decoding stays in scene::Room, while this
// object owns the mutable state shared by the simulation, message queue, and
// frontends.
class Scene {
public:
    struct FadeRequest {
        formats::FadeType type = formats::FadeType::None;
        float seconds = 0.0F;
        bool overwrite = false;
    };

    explicit Scene(const assets::Store& assets) noexcept : assets_(assets) {}
    ~Scene();

    [[nodiscard]] bool load_room(const scene::RoomDefinition& definition,
                                 gameplay::Config config = {});
    [[nodiscard]] bool load_room(std::string_view catalog_name,
                                 gameplay::Config config = {});
    [[nodiscard]] bool start_camera_sequence(
        int sequence_id, std::string_view asset_path,
        camera::CameraState initial_camera,
        camera::Flags flags = camera::Flags::None,
        std::uint16_t transition_time = 0);
    // Mods/Render/PreviewCamera.cs equivalent.
    void set_preview_camera(formats::Vector3 position,
                            formats::Vector3 target) noexcept;
    void stop_camera_sequence() noexcept;
    void unload() noexcept;
    void tick();

    [[nodiscard]] bool loaded() const noexcept { return room_.has_value(); }
    [[nodiscard]] const scene::Room* room() const noexcept {
        return room_.has_value() ? &*room_ : nullptr;
    }
    [[nodiscard]] gameplay::Session* session() noexcept {
        return session_.has_value() ? &*session_ : nullptr;
    }
    [[nodiscard]] const gameplay::Session* session() const noexcept {
        return session_.has_value() ? &*session_ : nullptr;
    }
    [[nodiscard]] game::State& state() noexcept { return state_; }
    [[nodiscard]] const game::State& state() const noexcept { return state_; }
    [[nodiscard]] messaging::Queue& messages() noexcept { return messages_; }
    [[nodiscard]] runtime::EntityPool& dynamic_entities() noexcept {
        return dynamic_entities_;
    }
    [[nodiscard]] const entities::static_entities::World*
    static_entities() const noexcept {
        return static_entities_.has_value() ? &*static_entities_ : nullptr;
    }
    [[nodiscard]] const std::vector<std::unique_ptr<
        entities::cam_seq::CameraSequenceEntity>>&
    camera_sequence_entities() const noexcept {
        return camera_sequence_entities_;
    }
    [[nodiscard]] camera::Playback* camera_sequence() noexcept {
        return camera_playback_.has_value() ? &*camera_playback_ : nullptr;
    }
    [[nodiscard]] const camera::Playback* camera_sequence() const noexcept {
        return camera_playback_.has_value() ? &*camera_playback_ : nullptr;
    }
    [[nodiscard]] const camera::CameraState* camera_state() const noexcept {
        return camera_state_.has_value() ? &*camera_state_ : nullptr;
    }
    [[nodiscard]] const std::optional<FadeRequest>& fade_request()
        const noexcept {
        return fade_request_;
    }
    void clear_fade_request() noexcept { fade_request_.reset(); }
    [[nodiscard]] bool form_lock_active() const noexcept {
        return form_lock_active_;
    }
    [[nodiscard]] bool form_lock_alt() const noexcept {
        return form_lock_alt_;
    }
    [[nodiscard]] std::optional<camera::EntityPose> resolve_camera_entity(
        std::int16_t type, std::int16_t id) const;

private:
    void create_camera_sequence_entities();
    void process_room_transition(
        const gameplay::RoomTransitionRequest& request);

    const assets::Store& assets_;
    gameplay::Config config_{};
    std::optional<scene::Room> room_;
    std::optional<gameplay::Session> session_;
    std::optional<camera::File> camera_file_;
    std::optional<camera::Playback> camera_playback_;
    std::optional<camera::CameraState> camera_state_;
    game::State state_;
    messaging::Queue messages_;
    runtime::EntityPool dynamic_entities_;
    std::optional<entities::static_entities::World> static_entities_;
    std::vector<std::unique_ptr<entities::cam_seq::CameraSequenceEntity>>
        camera_sequence_entities_;
    std::optional<FadeRequest> fade_request_;
    bool form_lock_active_ = false;
    bool form_lock_alt_ = false;
    float tick_seconds_ = 1.0F / 60.0F;
};

} // namespace fruityprime::scene_runtime

namespace MphReadNative {
using RuntimeScene = ::fruityprime::scene_runtime::Scene;
}
