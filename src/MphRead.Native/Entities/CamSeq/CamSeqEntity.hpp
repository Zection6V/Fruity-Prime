#pragma once

#include "Formats/camera_sequence.hpp"
#include "Messaging.hpp"
#include "Entities/scene.hpp"

#include <cstdint>
#include <optional>
#include <string>

namespace fruityprime::scene_runtime {
class Scene;
}

namespace fruityprime::entities::cam_seq {

// Native counterpart of Entities/CamSeq/CamSeqEntity.cs.  The scene owns the
// decoded camera resource and this controller owns the entity-level state:
// delayed activation, handoff cancellation, per-entity flags, looping, and
// the optional end message.  Audio and player-form side effects are exposed
// by Scene's camera/fade state and remain separate services.
class CameraSequenceEntity final {
public:
    static constexpr std::uint32_t SetActiveMessage = 5;
    static constexpr std::uint32_t ActivateMessage = 18;
    static constexpr std::uint32_t HandoffFrames = 4;

    struct Config {
        scene::CameraSequenceData data;
        std::int16_t entity_id = -1;
        std::string asset_path;
        std::string name;
    };

    CameraSequenceEntity(scene_runtime::Scene& scene, Config config,
                         camera::CameraState initial_camera);
    ~CameraSequenceEntity();

    CameraSequenceEntity(const CameraSequenceEntity&) = delete;
    CameraSequenceEntity& operator=(const CameraSequenceEntity&) = delete;

    [[nodiscard]] const scene::CameraSequenceData& data() const noexcept {
        return config_.data;
    }
    [[nodiscard]] std::int16_t entity_id() const noexcept {
        return config_.entity_id;
    }
    [[nodiscard]] const std::string& name() const noexcept {
        return config_.name;
    }
    [[nodiscard]] bool active() const noexcept { return active_; }
    [[nodiscard]] bool started() const noexcept { return started_; }
    [[nodiscard]] bool handoff() const noexcept { return handoff_; }
    [[nodiscard]] std::uint32_t delay_timer() const noexcept {
        return delay_timer_;
    }
    [[nodiscard]] std::uint32_t handoff_timer() const noexcept {
        return handoff_timer_;
    }

    // Activates the entity. process() is called once after the owning Scene
    // has ticked; passing quick_start reproduces the two octolith-intro
    // entities that skip most of their authored delay.
    [[nodiscard]] bool activate(bool handoff = false,
                                bool quick_start = false);
    void deactivate() noexcept;
    void cancel() noexcept;
    void process();
    void handle_message(std::uint32_t message, std::int32_t parameter = 0);

    static void cancel_current() noexcept;
    [[nodiscard]] static CameraSequenceEntity* current() noexcept {
        return current_;
    }

private:
    [[nodiscard]] bool try_start();
    void finish(bool send_end_message) noexcept;
    void send_end_message() noexcept;

    scene_runtime::Scene& scene_;
    Config config_;
    camera::CameraState initial_camera_;
    std::optional<camera::CameraState> handoff_camera_;
    bool active_ = false;
    bool started_ = false;
    bool handoff_ = false;
    std::uint32_t handoff_timer_ = 0;
    std::uint32_t delay_timer_ = 0;

    static CameraSequenceEntity* current_;
};

} // namespace fruityprime::entities::cam_seq
