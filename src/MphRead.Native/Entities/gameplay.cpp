#include "Entities/gameplay.hpp"
#include "Entities/Players/PlayerEntity.hpp"
#include "Metadata/metadata.hpp"
#include "gameplay_helpers.hpp"

#include <algorithm>
#include <stdexcept>
namespace fruityprime::gameplay {
using namespace detail;
void Session::set_gorea1a_model(const model::File* model) {
    if (gorea_1a_model_ == model) {
        return;
    }
    gorea_1a_model_ = model;
    gorea_1a_model_instance_.reset();
}
void Session::set_gorea1b_model(const model::File* model) {
    if (gorea_1b_model_ == model) {
        return;
    }
    gorea_1b_model_ = model;
    gorea_1b_model_instance_.reset();
}
void Session::set_gorea2_model(const model::File* model) {
    if (gorea_2_model_ == model) {
        return;
    }
    gorea_2_model_ = model;
    gorea_2_model_instance_.reset();
}
bool Session::sample_gorea_model_node(
    const EnemyState& parent, std::uint8_t expected_type,
    const model::File* model_file,
    std::unique_ptr<model::ModelInstance>& instance,
    std::string_view node_name, formats::Matrix4& transform) {
    if (parent.enemy_type != expected_type || model_file == nullptr) {
        return false;
    }

    try {
        if (instance == nullptr) {
            instance = std::make_unique<model::ModelInstance>(*model_file);
            instance->set_node_animation_ignore_root(true);
        }
        model::ModelInstance& sampler = *instance;
        const int animation = static_cast<int>(parent.gorea_model_animation);
        const auto flags = parent.gorea_animation_loop
            ? model::AnimationFlags::None
            : model::AnimationFlags::NoLoop;
        const auto& current = sampler.animation_info();
        const bool current_no_loop = model::has_flag(
            current.flags[0], model::AnimationFlags::NoLoop);
        if (current.index[0] != animation
            || current_no_loop != !parent.gorea_animation_loop) {
            sampler.set_animation(animation, flags);
        }
        sampler.set_animation_frame(
            0, static_cast<int>(parent.gorea_animation_frame));

        formats::Vector3 facing{
            parent.facing.x, parent.facing.y, parent.facing.z};
        formats::Vector3 up{parent.up.x, parent.up.y, parent.up.z};
        if (facing.length_squared() <= 0.0001F) {
            facing = {0.0F, 0.0F, 1.0F};
        }
        if (up.length_squared() <= 0.0001F) {
            up = {0.0F, 1.0F, 0.0F};
        }
        formats::Matrix4 parent_transform = formats::MatrixOps::get_transform4(
            facing.normalized(), up.normalized(),
            {parent.position.x, parent.position.y, parent.position.z});
        // EntityBase.GetModelTransform returns
        // Matrix4.CreateScale(model.Scale) * entity.Transform.  With the
        // row-vector layout used here that scales the basis rows while the
        // translation row remains the entity position.
        const float scale = sampler.model().world_scale();
        parent_transform.m11 *= scale;
        parent_transform.m12 *= scale;
        parent_transform.m13 *= scale;
        parent_transform.m21 *= scale;
        parent_transform.m22 *= scale;
        parent_transform.m23 *= scale;
        parent_transform.m31 *= scale;
        parent_transform.m32 *= scale;
        parent_transform.m33 *= scale;

        // This is the same early update used by Gorea1A.GetNodeTransform:
        // no bind-pose node transform, animated node state plus the current
        // entity transform.  Child entities call this once per node and the
        // instance is reused, so all attachments see one consistent cursor.
        sampler.animate_nodes(false, parent_transform);
        const auto& nodes = sampler.model().nodes();
        const auto& states = sampler.node_states();
        const auto found = std::find_if(
            nodes.begin(), nodes.end(),
            [node_name](const model::Node& node) {
                return node.name == node_name;
            });
        if (found == nodes.end()) {
            return false;
        }
        const std::size_t index = static_cast<std::size_t>(
            found - nodes.begin());
        if (index >= states.size()) {
            return false;
        }
        transform = states[index].animation;
        return true;
    } catch (const std::exception&) {
        // Missing optional animation data or a malformed model must not make
        // a headless gameplay session unusable.  The attached-part caller
        // retains its decoded bind-pose fallback in this case.
        return false;
    }
}

bool Session::sample_gorea_1a_node(const EnemyState& parent,
                                   std::string_view node_name,
                                   formats::Matrix4& transform) {
    return sample_gorea_model_node(
        parent, static_cast<std::uint8_t>(formats::EnemyType::Gorea1A),
        gorea_1a_model_, gorea_1a_model_instance_, node_name, transform);
}

bool Session::sample_gorea_1b_node(const EnemyState& parent,
                                   std::string_view node_name,
                                   formats::Matrix4& transform) {
    return sample_gorea_model_node(
        parent, static_cast<std::uint8_t>(formats::EnemyType::Gorea1B),
        gorea_1b_model_, gorea_1b_model_instance_, node_name, transform);
}

bool Session::sample_gorea_2_node(const EnemyState& parent,
                                  std::string_view node_name,
                                  formats::Matrix4& transform) {
    return sample_gorea_model_node(
        parent, static_cast<std::uint8_t>(formats::EnemyType::Gorea2),
        gorea_2_model_, gorea_2_model_instance_, node_name, transform);
}

Session::Session(const scene::Room& room, Config config)
    : room_(room), config_(config),
      world_min_{-100.0F, -100.0F, -100.0F},
      world_max_{100.0F, 100.0F, 100.0F} {
    if (config_.tick_seconds <= 0.0F) {
        throw std::invalid_argument("gameplay tick must be positive");
    }

    bool have_bounds = false;
    const auto update_bounds = [this, &have_bounds](formats::Vector3Fx point) {
        const net::Vec3 value = to_float(point);
        if (!have_bounds) {
            world_min_ = value;
            world_max_ = value;
            have_bounds = true;
            return;
        }
        world_min_.x = std::min(world_min_.x, value.x);
        world_min_.y = std::min(world_min_.y, value.y);
        world_min_.z = std::min(world_min_.z, value.z);
        world_max_.x = std::max(world_max_.x, value.x);
        world_max_.y = std::max(world_max_.y, value.y);
        world_max_.z = std::max(world_max_.z, value.z);
    };
    if (room.collision().is_mph()) {
        for (const auto& point : room.collision().mph().points) {
            update_bounds(point);
        }
    } else {
        for (const auto& point : room.collision().first_hunt().points) {
            update_bounds(point);
        }
    }
    if (!have_bounds) {
        world_min_ = {-100.0F, -100.0F, -100.0F};
        world_max_ = {100.0F, 100.0F, 100.0F};
    }
    world_min_.x += config_.world_padding;
    world_min_.z += config_.world_padding;
    world_max_.x -= config_.world_padding;
    world_max_.z -= config_.world_padding;
    initialize_objectives();
    initialize_environment();
}

void Session::tick() {
    constexpr std::uint8_t WeaponCount = 9;
    single_particles_.clear();
    sound_events_.clear();
    for (auto& spawn : player_spawns_) {
        if (spawn.cooldown > 0) {
            --spawn.cooldown;
        }
    }
    for (auto& player : players_) {
        players::PlayerEntity::SessionPlayerTick(
            *this, player.slot_index);
        const auto input = std::find_if(inputs_.begin(), inputs_.end(),
            [&player](const RuntimeInput& value) {
                return value.slot == player.slot_index;
            });
        if (input != inputs_.end() && input->input_received) {
            const bool spectating = has_button(
                input->input.buttons, net::IntentButtons::SpectatingState);
            if (spectating) {
                // A spectator remains a connected slot, but its hunter must
                // not move, fire, respawn, or remain a solid target.  The
                // flag is written every tick because respawn/reset paths can
                // replace the rest of the state while the client keeps
                // spectating.
                player.flags |= net::PlayerState::FlagSpectating;
                player.speed = {};
                input->previous_buttons = input->input.buttons;
                input->fire_cooldown = 0;
                continue;
            }
            player.flags &= static_cast<std::uint8_t>(
                ~net::PlayerState::FlagSpectating);
            if (player.health == 0) {
                player.speed = {};
                input->previous_buttons = input->input.buttons;
                input->fire_cooldown = 0;
                if (input->eliminated) {
                    continue;
                }
                if (input->respawn_ticks > 0) {
                    --input->respawn_ticks;
                }
                if (input->respawn_ticks == 0) {
                    respawn_player(player, *input);
                }
                continue;
            }
            if ((player.flags & net::PlayerState::FlagFrozen) != 0) {
                player.speed = {};
                input->previous_buttons = input->input.buttons;
                input->fire_cooldown = 0;
                continue;
            }
            input->respawn_ticks = 0;
            if (input->fire_cooldown > 0) {
                --input->fire_cooldown;
            }
            if (input->control_lock_ticks == 0) {
                const bool next_weapon = has_button(
                    input->input.buttons, net::IntentButtons::NextWeapon);
                const bool previous_next = has_button(
                    input->previous_buttons, net::IntentButtons::NextWeapon);
                const bool prev_weapon = has_button(
                    input->input.buttons, net::IntentButtons::PrevWeapon);
                const bool previous_prev = has_button(
                    input->previous_buttons, net::IntentButtons::PrevWeapon);
                const bool direct_weapon =
                    input->input.weapon_select < WeaponCount;
                const std::uint8_t previous_weapon = player.current_weapon;
                if (direct_weapon) {
                    player.current_weapon = input->input.weapon_select;
                } else if (next_weapon && !previous_next) {
                    player.current_weapon = static_cast<std::uint8_t>(
                        (player.current_weapon + 1) % WeaponCount);
                } else if (prev_weapon && !previous_prev) {
                    player.current_weapon = player.current_weapon == 0
                        ? static_cast<std::uint8_t>(WeaponCount - 1)
                        : static_cast<std::uint8_t>(player.current_weapon - 1);
                }
                if (player.current_weapon != previous_weapon) {
                    players::PlayerEntity::SessionWeaponChanged(
                        *this, player.slot_index, previous_weapon,
                        player.current_weapon);
                }
                const metadata::WeaponInfo& profile =
                    weapon_profile(player.current_weapon);
                const bool shoot = has_button(input->input.buttons,
                                              net::IntentButtons::Shoot);
                const bool shoot_pressed = shoot
                    && !has_button(input->previous_buttons,
                                   net::IntentButtons::Shoot);
                if (input->fire_cooldown == 0
                    && (shoot_pressed || (shoot && profile.repeat_fire))
                    && consume_weapon_ammo(*input, player.current_weapon)) {
                    spawn_projectile(player, input->input);
                    input->fire_cooldown = profile.cooldown_ticks;
                    players::PlayerEntity::SessionShotFired(
                        *this, player.slot_index);
                }
            }
            apply_input(player, input->input);
            input->previous_buttons = input->input.buttons;
        }
    }
    update_environment();
    update_items();
    update_projectiles();
    update_effects();
    update_enemies();
    update_objectives();
    ++tick_count_;
}

} // namespace fruityprime::gameplay
