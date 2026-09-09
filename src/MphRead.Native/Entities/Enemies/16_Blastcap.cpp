#include "16_Blastcap.hpp"
#include "enemy_common.hpp"

namespace fruityprime::gameplay {
namespace {

constexpr std::uint8_t kNoPlayer = 0xff;
constexpr float kFixedRate = 60.0F;
constexpr std::uint32_t kNoLoopFallbackFrames = 30;

} // namespace

void Session::update_blastcap(EnemyState& agent) {
    const float seconds = std::max(0.0F, config_.tick_seconds);
    const auto frame_step = static_cast<std::uint32_t>(std::max(
        1.0F, std::round(seconds * kFixedRate)));
    const auto decrement = [frame_step](std::uint32_t& value) noexcept {
        value = value > frame_step ? value - frame_step : 0;
    };

    if (agent.blastcap_exploded) {
        if (agent.health == 0) {
            // Behavior01 leaves the health at zero; the managed base entity
            // consumes that terminal state on the following process pass.
            // Keep the same one-frame boundary before notifying Enemy40.
            const auto id = agent.id;
            static_cast<void>(destroy_enemy(id));
            return;
        }
        // Enemy16Entity.State3 increments before calling its subroutine. The
        // cloud's periodic check is the first behavior; expiry is the second
        // behavior and leaves health at zero for the base Destroyed pass.
        agent.state = 3;
        agent.blastcap_next_state = 3;
        agent.target_slot = kNoPlayer;
        agent.velocity = {};
        agent.blastcap_cloud_tick += frame_step;
        if (agent.blastcap_cloud_tick % (10u * 2u) == 0u) {
            constexpr float cloud_radius = 2.0F;
            const float combined_radius = cloud_radius + config_.body_radius;
            for (auto& player : players_) {
                if (!objective_player(player)
                    || distance_squared(player.position, agent.position)
                        >= combined_radius * combined_radius) {
                    continue;
                }
                scene::AreaVolumeData cloud_damage;
                cloud_damage.inside_message = MessageDamage;
                cloud_damage.inside_parameter1 = 2;
                const auto runtime = std::find_if(
                    inputs_.begin(), inputs_.end(),
                    [&player](const RuntimeInput& value) {
                        return value.slot == player.slot_index;
                    });
                if (runtime != inputs_.end()) {
                    apply_area_effect(cloud_damage, player, *runtime);
                }
            }
        }

        if (agent.blastcap_cloud_timer > 0) {
            decrement(agent.blastcap_cloud_timer);
        } else {
            agent.health = 0;
        }
        return;
    }

    if (!agent.blastcap_state_initialized) {
        agent.blastcap_state_initialized = true;
        agent.blastcap_next_state = agent.state;
        agent.blastcap_animation = 2;
        agent.blastcap_animation_no_loop = false;
        agent.blastcap_animation_ended = false;
        agent.blastcap_agitate_timer = 60u * 2u;
        agent.blastcap_cloud_tick = 0;
        agent.blastcap_cloud_timer = 150u * 2u;
    }

    if (agent.blastcap_animation_no_loop
        && agent.blastcap_animation_timer > 0) {
        decrement(agent.blastcap_animation_timer);
        if (agent.blastcap_animation_timer == 0) {
            agent.blastcap_animation_ended = true;
        }
    }

    const std::uint8_t current = agent.blastcap_next_state;
    agent.state = current;
    agent.target_slot = kNoPlayer;
    agent.velocity = {};

    const auto set_animation = [&agent](std::uint8_t animation,
                                         bool no_loop,
                                         std::uint32_t duration) {
        agent.blastcap_animation = animation;
        agent.blastcap_animation_no_loop = no_loop;
        agent.blastcap_animation_timer = no_loop ? duration : 0;
        agent.blastcap_animation_ended = false;
    };
    const auto animation_ended = [&agent]() noexcept {
        return agent.blastcap_animation_no_loop
            && agent.blastcap_animation_ended;
    };

    const auto transition = [&](std::uint8_t next) {
        agent.blastcap_next_state = next;
        if (next == 0) {
            set_animation(2, false, 0);
        } else if (next == 2) {
            set_animation(1, false, 0);
        }
    };

    net::PlayerState* nearest = nullptr;
    float nearest_squared = std::numeric_limits<float>::max();
    for (auto& player : players_) {
        if (!objective_player(player)) {
            continue;
        }
        const float distance = distance_squared(
            player.position, agent.position);
        if (distance < nearest_squared) {
            nearest_squared = distance;
            nearest = &player;
        }
    }
    if (nearest != nullptr) {
        agent.target_slot = nearest->slot_index;
    }

    const auto near_player = [nearest, &agent]() noexcept {
        if (nearest == nullptr) {
            return false;
        }
        constexpr float near_radius = 8.0F + 0.45F;
        return distance_squared(nearest->position, agent.position)
            < near_radius * near_radius;
    };
    const auto touching_player = [nearest, &agent]() noexcept {
        if (nearest == nullptr) {
            return false;
        }
        constexpr float contact_radius = 1.25F;
        return distance_squared(nearest->position, agent.position)
            <= contact_radius * contact_radius;
    };

    const auto trigger_contact = [&]() {
        if (nearest == nullptr || !touching_player()) {
            return false;
        }
        // Enemy16Entity.Behavior03 first damages the player, then calls
        // TakeDamage(100), which enters the cloud path and is deliberately
        // distinct from a normal lethal beam hit.
        apply_enemy_contact_damage(agent, *nearest, 2);
        static_cast<void>(damage_enemy(agent.id, 100));
        return true;
    };

    switch (current) {
    case 0:
        if (agent.blastcap_agitate_timer > 0) {
            decrement(agent.blastcap_agitate_timer);
        } else {
            // Behavior06: agitate, play anim0 without looping, enter state1.
            set_animation(0, true, kNoLoopFallbackFrames);
            agent.blastcap_agitate_timer = 60u * 2u;
            transition(1);
            // The managed subroutine table stops after the first successful
            // behavior.  Do not let the following proximity/contact checks
            // run on the same frame as the agitate transition.
            break;
        }
        if (near_player()) {
            // Behavior05 wins over Behavior03 and returns immediately.
            set_animation(1, false, 0);
            transition(2);
            break;
        }
        if (trigger_contact()) {
            transition(3);
        }
        break;
    case 1:
        if (animation_ended()) {
            transition(0);
        } else if (near_player()) {
            set_animation(1, false, 0);
            transition(2);
        } else if (trigger_contact()) {
            transition(3);
        }
        break;
    case 2:
        if (!near_player()) {
            // Behavior02: restore anim2 and return to idle.
            set_animation(2, false, 0);
            transition(0);
        }
        // Behavior02 succeeds while the player is in the near radius; the
        // managed table therefore does not fall through to Behavior03 on a
        // touching player either.
        break;
    case 3:
        // This state is handled by the exploded branch above. Keep an
        // explicit fallback for a message-created state transition.
        agent.blastcap_exploded = true;
        break;
    default:
        transition(0);
        break;
    }
}

} // namespace fruityprime::gameplay
