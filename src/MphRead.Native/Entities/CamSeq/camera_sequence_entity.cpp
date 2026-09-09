#include "Entities/CamSeq/CamSeqEntity.hpp"

#include "Scene.hpp"

#include <algorithm>
#include <limits>
#include <utility>

namespace fruityprime::entities::cam_seq {
namespace {

[[nodiscard]] camera::Flags without_loop(camera::Flags flags) noexcept {
    return static_cast<camera::Flags>(
        static_cast<std::uint8_t>(flags)
        & ~static_cast<std::uint8_t>(camera::Flags::Loop));
}

[[nodiscard]] std::uint32_t delay_limit(
    const scene::CameraSequenceData& data) noexcept {
    return std::min<std::uint32_t>(
        std::numeric_limits<std::uint32_t>::max(),
        static_cast<std::uint32_t>(data.delay_frames) * 2U);
}

} // namespace

CameraSequenceEntity* CameraSequenceEntity::current_ = nullptr;

CameraSequenceEntity::CameraSequenceEntity(
    scene_runtime::Scene& scene, Config config,
    camera::CameraState initial_camera)
    : scene_(scene), config_(std::move(config)),
      initial_camera_(std::move(initial_camera)) {}

CameraSequenceEntity::~CameraSequenceEntity() {
    if (current_ == this) {
        scene_.stop_camera_sequence();
        current_ = nullptr;
    }
}

bool CameraSequenceEntity::activate(bool requested_handoff,
                                    bool quick_start) {
    if (active_) {
        return true;
    }

    bool effective_handoff = false;
    if (current_ != nullptr && current_ != this) {
        if (current_->config_.data.block_input) {
            return false;
        }
        if (current_->handoff_ && config_.data.handoff) {
            if (current_->handoff_timer_ == 0) {
                return false;
            }
            effective_handoff = true;
            if (&current_->scene_ == &scene_
                && current_->scene_.camera_state() != nullptr) {
                handoff_camera_ = *current_->scene_.camera_state();
            }
        }
        current_->cancel();
    } else if (requested_handoff) {
        effective_handoff = true;
        if (scene_.camera_state() != nullptr) {
            handoff_camera_ = *scene_.camera_state();
        }
    }

    active_ = true;
    started_ = false;
    handoff_ = effective_handoff;
    handoff_timer_ = 0;
    delay_timer_ = quick_start ? 7U : 0U;
    current_ = this;

    if (config_.data.delay_frames == 0 || quick_start) {
        if (!try_start()) {
            finish(false);
            return false;
        }
    }
    return true;
}

void CameraSequenceEntity::deactivate() noexcept {
    if (active_ && handoff_timer_ == 0) {
        handoff_timer_ = HandoffFrames;
    }
}

void CameraSequenceEntity::cancel() noexcept {
    if (!active_ && !started_) {
        if (current_ == this) {
            current_ = nullptr;
        }
        return;
    }
    finish(true);
}

void CameraSequenceEntity::process() {
    if (!active_) {
        return;
    }
    if (handoff_timer_ > 0) {
        --handoff_timer_;
        if (handoff_timer_ == 0) {
            cancel();
            return;
        }
    }

    if (!try_start()) {
        finish(false);
        return;
    }
    if (!started_) {
        return;
    }

    camera::Playback* playback = scene_.camera_sequence();
    if (playback == nullptr || !playback->can_end()) {
        return;
    }
    if (config_.data.loop) {
        playback->restart(playback->transition_timer(),
                          playback->transition_time());
        return;
    }
    finish(true);
}

void CameraSequenceEntity::handle_message(std::uint32_t message,
                                          std::int32_t parameter) {
    if (message == ActivateMessage
        || (message == SetActiveMessage && parameter != 0)) {
        static_cast<void>(activate());
    } else if (message == SetActiveMessage && parameter == 0) {
        deactivate();
    }
}

void CameraSequenceEntity::cancel_current() noexcept {
    if (current_ != nullptr) {
        current_->cancel();
    }
}

bool CameraSequenceEntity::try_start() {
    const std::uint32_t limit = delay_limit(config_.data);
    if (delay_timer_ <= limit) {
        ++delay_timer_;
    }
    if (started_ || delay_timer_ <= limit) {
        return true;
    }

    camera::Flags flags = camera::Flags::None;
    if (config_.data.block_input) {
        flags |= camera::Flags::BlockInput;
    }
    if (config_.data.force_alt_form) {
        flags |= camera::Flags::ForceAlt;
    } else if (config_.data.force_biped_form && config_.data.block_input) {
        flags |= camera::Flags::ForceBiped;
    }

    camera::CameraState initial = handoff_camera_.has_value()
        ? *handoff_camera_ : initial_camera_;
    if (!scene_.start_camera_sequence(
            config_.data.sequence_id, config_.asset_path, std::move(initial),
            flags, handoff_ ? 120U : 0U)) {
        return false;
    }
    // CameraSequence.SetUp clears the intrinsic loader loop flag.  The
    // entity-level Data.Loop decision is handled in process() instead.
    if (camera::Playback* playback = scene_.camera_sequence();
        playback != nullptr) {
        playback->set_flags(without_loop(playback->flags()));
    }
    started_ = true;
    return true;
}

void CameraSequenceEntity::finish(bool send_end) noexcept {
    if (started_) {
        scene_.stop_camera_sequence();
    }
    active_ = false;
    started_ = false;
    handoff_ = false;
    handoff_timer_ = 0;
    delay_timer_ = 0;
    handoff_camera_.reset();
    if (current_ == this) {
        current_ = nullptr;
    }
    if (send_end) {
        send_end_message();
    }
}

void CameraSequenceEntity::send_end_message() noexcept {
    if (config_.data.end_message == 0) {
        return;
    }
    static_cast<void>(scene_.messages().send(
        messaging::Message::None, config_.entity_id,
        config_.data.end_message_target_id, config_.data.end_message_parameter,
        0, scene_.state().frame_count, -1, config_.data.end_message));
}

} // namespace fruityprime::entities::cam_seq
