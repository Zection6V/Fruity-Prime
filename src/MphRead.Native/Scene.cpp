#include "Scene.hpp"

#include "Entities/room_catalog.hpp"
#include "Formats/Types.hpp"

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <optional>
#include <string>
#include <string_view>
#include <utility>

namespace fruityprime::scene_runtime {
namespace {

[[nodiscard]] formats::Vector3 to_vector3(formats::Vector3Fx value) noexcept {
    return {value.x.to_float(), value.y.to_float(), value.z.to_float()};
}

[[nodiscard]] std::string sequence_display_name(
    std::string_view asset_path) {
    const std::size_t slash = asset_path.find_last_of('/');
    std::string result(asset_path.substr(
        slash == std::string_view::npos ? 0 : slash + 1));
    constexpr std::string_view extension = ".bin";
    if (result.size() >= extension.size()
        && result.compare(result.size() - extension.size(), extension.size(),
                          extension)
               == 0) {
        result.resize(result.size() - extension.size());
    }
    return result;
}

} // namespace

Scene::~Scene() = default;

bool Scene::start_camera_sequence(
    int sequence_id, std::string_view asset_path,
    camera::CameraState initial_camera, camera::Flags flags,
    std::uint16_t transition_time) {
    stop_camera_sequence();
    try {
        camera_file_.emplace(camera::File::from_bytes(
            assets_.bytes(asset_path), sequence_id, std::string(asset_path)));
        camera_playback_.emplace(sequence_id, *camera_file_);
        camera_playback_->set_flags(camera_playback_->flags() | flags);
        camera_playback_->set_entity_resolver(
            [this](std::int16_t type, std::int16_t id) {
                return resolve_camera_entity(type, id);
            });
        camera_playback_->set_message_sink(
            [this](const camera::MessageEvent& event) {
                if (event.message == 0) {
                    return;
                }
                static_cast<void>(messages_.send(
                    messaging::Message::None, -1,
                    event.target_id, event.parameter, 0, state_.frame_count,
                    event.target_type, event.message));
            });
        camera_playback_->set_fade_sink(
            [this](formats::FadeType type, float seconds, bool overwrite) {
                fade_request_ = FadeRequest{type, seconds, overwrite};
            });
        camera_playback_->set_form_lock_sink(
            [this](bool alt_form) {
                form_lock_active_ = true;
                form_lock_alt_ = alt_form;
            });
        camera_playback_->set_up(std::move(initial_camera), transition_time);
        camera_state_ = camera_playback_->camera();
        return true;
    } catch (...) {
        stop_camera_sequence();
        return false;
    }
}

void Scene::stop_camera_sequence() noexcept {
    if (camera_playback_.has_value()) {
        camera_playback_->end();
        camera_state_ = camera_playback_->camera();
    }
    camera_playback_.reset();
    camera_file_.reset();
    fade_request_.reset();
    form_lock_active_ = false;
    form_lock_alt_ = false;
}

bool Scene::load_room(const scene::RoomDefinition& definition,
                      gameplay::Config config) {
    config_ = config;
    stop_camera_sequence();
    camera_sequence_entities_.clear();
    camera_state_.reset();
    try {
        auto loaded = scene::Room::load(assets_, definition);
        room_ = std::move(loaded);
        session_.emplace(*room_, config);
        static_entities_.emplace(room_->entities());
        static_entities_->set_message_sink([this](const auto& info) {
            // A relay volume forwards the message through the same delayed
            // Scene queue used by camera keyframes and gameplay events.
            static_cast<void>(messages_.post(info));
        });
        session_->set_message_sink([this](const auto& info) {
            // Gameplay environment links use the same queue as camera and
            // static-world messages, so a target is not updated halfway
            // through the current fixed step.
            static_cast<void>(messages_.post(info));
        });
        create_camera_sequence_entities();
        tick_seconds_ = std::isfinite(config.tick_seconds)
            && config.tick_seconds > 0.0F
            ? config.tick_seconds
            : 1.0F / 60.0F;
        state_.begin_room(definition.name, 0, config.mode != 2,
                          static_cast<game::Mode>(config.mode));
        return true;
    } catch (...) {
        unload();
        return false;
    }
}

bool Scene::load_room(std::string_view catalog_name,
                      gameplay::Config config) {
    const auto* entry = scene::find_room(catalog_name);
    if (entry == nullptr) {
        return false;
    }
    if (!load_room(entry->definition, config)) {
        return false;
    }
    state_.room_id = static_cast<std::uint32_t>(entry->id);
    if (config.mode == static_cast<std::uint8_t>(game::Mode::SinglePlayer)
        && static_entities_.has_value()) {
        static_entities_->apply_story_save(
            entry->id, state_.story_save);
        if (session_.has_value()) {
            session_->apply_story_room_state(entry->id, state_.story_save);
        }
    }
    return true;
}

void Scene::process_room_transition(
    const gameplay::RoomTransitionRequest& request) {
    if (!session_.has_value()) {
        return;
    }
    const auto* destination = scene::find_room(request.room_name);
    if (destination == nullptr) {
        session_->clear_room_transition();
        return;
    }

    struct CarriedPlayer {
        std::uint8_t slot = 0;
        std::uint8_t hunter = 0;
        bool alt_form = false;
    };
    std::vector<CarriedPlayer> carried;
    carried.reserve(session_->players().size());
    for (const auto& player : session_->players()) {
        carried.push_back(CarriedPlayer{
            player.slot_index,
            static_cast<std::uint8_t>(
                session_->player_profile(player.slot_index).hunter),
            (player.flags & net::PlayerState::FlagAltForm) != 0
        });
    }

    const std::uint8_t local_slot = state_.local_slot;
    state_.transition_state = game::TransitionState::Start;
    state_.transition_room_id = static_cast<std::int32_t>(destination->id);
    state_.transition_alt_form = request.alt_form;
    state_.pause_prevented = true;
    if (!load_room(request.room_name, config_)) {
        state_.pause_prevented = false;
        return;
    }

    for (const auto& player : carried) {
        static_cast<void>(session_->add_player(player.slot, player.hunter));
    }
    if (config_.mode == static_cast<std::uint8_t>(game::Mode::SinglePlayer)
        && session_->has_player(local_slot)) {
        session_->apply_story_save(local_slot, state_.story_save);
    }

    const scene::EntityInstance* target = nullptr;
    for (const auto& entity : room_->entities()) {
        if (entity.kind != scene::EntityKind::Teleporter) {
            continue;
        }
        const auto* data = std::get_if<scene::TeleporterData>(
            &entity.typed_data);
        if (data != nullptr
            && data->load_index == request.target_entity_id) {
            target = &entity;
            break;
        }
    }
    if (target != nullptr) {
        net::Vec3 position{
            target->position.x.to_float(), target->position.y.to_float(),
            target->position.z.to_float()};
        position.y += 0.5F;
        const net::Vec3 facing{
            target->facing_vector.x.to_float(),
            target->facing_vector.y.to_float(),
            target->facing_vector.z.to_float()};
        for (const auto& player : carried) {
            const bool alt_form = player.slot == local_slot
                ? request.alt_form : player.alt_form;
            session_->place_player(player.slot, position, facing, alt_form);
        }
    }
    session_->clear_room_transition();
    state_.pause_prevented = false;
    state_.transition_state = game::TransitionState::None;
    state_.transition_room_id = -1;
    state_.transition_alt_form = false;
}

void Scene::unload() noexcept {
    stop_camera_sequence();
    camera_sequence_entities_.clear();
    session_.reset();
    static_entities_.reset();
    room_.reset();
    dynamic_entities_.clear();
    state_.reset();
    messages_.clear();
    camera_state_.reset();
    tick_seconds_ = 1.0F / 60.0F;
}

void Scene::create_camera_sequence_entities() {
    camera_sequence_entities_.clear();
    if (!room_.has_value()) {
        return;
    }
    for (const scene::EntityInstance& entity : room_->entities()) {
        if (entity.kind != scene::EntityKind::CameraSequence) {
            continue;
        }
        const auto* data = std::get_if<scene::CameraSequenceData>(
            &entity.typed_data);
        if (data == nullptr) {
            continue;
        }
        const std::string asset_path = camera::asset_path(data->sequence_id);
        if (asset_path.empty()) {
            continue;
        }
        const formats::Vector3 position = to_vector3(entity.position);
        const formats::Vector3 facing = to_vector3(entity.facing_vector);
        camera::CameraState initial_camera;
        initial_camera.position = position;
        initial_camera.previous_position = position;
        initial_camera.target = position + facing;
        initial_camera.up_vector = to_vector3(entity.up_vector);
        initial_camera.facing = facing;
        initial_camera.fov = 45.0F;
        initial_camera.node_name = entity.node_name;
        camera_sequence_entities_.push_back(
            std::make_unique<entities::cam_seq::CameraSequenceEntity>(
                *this,
                entities::cam_seq::CameraSequenceEntity::Config{
                    *data, entity.entity_id, asset_path,
                    sequence_display_name(asset_path)},
                initial_camera));
    }
}

std::optional<camera::EntityPose> Scene::resolve_camera_entity(
    std::int16_t type, std::int16_t id) const {
    if (!room_.has_value() || type < 0 || id < 0) {
        return std::nullopt;
    }
    for (const scene::EntityInstance& entity : room_->entities()) {
        if (entity.type != static_cast<std::uint16_t>(type)
            || entity.entity_id != id) {
            continue;
        }
        return camera::EntityPose{
            to_vector3(entity.position), to_vector3(entity.up_vector),
            to_vector3(entity.facing_vector)};
    }
    return std::nullopt;
}

void Scene::tick() {
    if (!loaded() || state_.paused || state_.loading) {
        return;
    }
    if (session_.has_value()) {
        session_->tick();
        if (session_->room_transition().has_value()) {
            const auto request = *session_->room_transition();
            process_room_transition(request);
            return;
        }
    }
    form_lock_active_ = false;
    if (camera_playback_.has_value()) {
        camera_playback_->process(tick_seconds_);
        camera_state_ = camera_playback_->camera();
    }
    const auto frame = state_.frame_count;
    static_cast<void>(messages_.dispatch_due(frame, [this](const auto& info) {
        state_.process_message(info);
        if (session_.has_value()) {
            session_->dispatch_message(info);
        }
        static_cast<void>(dynamic_entities_.dispatch(info));
        if (static_entities_.has_value()) {
            static_cast<void>(static_entities_->dispatch(info));
        }
        if (info.cartridge_message == 0) {
            return;
        }
        if (static_cast<formats::Message>(info.cartridge_message)
                == formats::Message::Checkpoint
            && info.sender >= 0) {
            // GameState.Checkpoint immediately disables the checkpoint entity
            // after recording it.  Keep that side effect in the scene layer,
            // where the target entity collections are owned.
            messaging::MessageInfo disable = info;
            disable.message = messaging::Message::None;
            disable.cartridge_message = static_cast<std::uint32_t>(
                formats::Message::SetActive);
            disable.target = info.sender;
            disable.parameter1 = 0;
            disable.parameter2 = 0;
            if (static_entities_.has_value()) {
                static_cast<void>(static_entities_->dispatch(disable));
            }
            static_cast<void>(dynamic_entities_.dispatch(disable));
        }
        for (const auto& entity : camera_sequence_entities_) {
            if (info.target >= 0 && entity->entity_id() != info.target) {
                continue;
            }
            entity->handle_message(info.cartridge_message, info.parameter1);
        }
    }));
    if (static_entities_.has_value()) {
        static_entities_->process(tick_seconds_);
    }
    for (const auto& entity : camera_sequence_entities_) {
        entity->process();
    }
    static_cast<void>(dynamic_entities_.process(tick_seconds_));
    if (session_.has_value()) {
        state_.sync_players(session_->players());
        const auto& objectives = session_->objective_state();
        state_.player_time = objectives.player_time;
        state_.team_time = objectives.team_time;
        state_.prime_hunter = objectives.prime_hunter;
        state_.update_results();
        state_.process_match_frame(tick_seconds_, session_->players(),
                                   true);
    } else {
        state_.update_time(tick_seconds_);
    }
    state_.process_story_frame(tick_seconds_);
    state_.tick();
}

} // namespace fruityprime::scene_runtime
