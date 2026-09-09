#include "enemy_decode_common.hpp"
// Native port of src/MphRead/Entities/Enemies/19_Cretaphid.cs.
// This member definition lives in its enemy module; Session only dispatches it.
#include "19_Cretaphid.hpp"
#include "enemy_common.hpp"

namespace fruityprime::gameplay {

void Session::update_cretaphid(EnemyState& agent) {
    const auto& profile = agent.cretaphid;
    if (!profile.supported) {
        return;
    }

    constexpr std::uint8_t Intro = 0;
    constexpr std::uint8_t Eyes = 1;
    constexpr std::uint8_t Crystal = 2;
    constexpr std::uint8_t PhaseChange = 3;
    constexpr std::uint8_t Defeated = 4;
    const float seconds = config_.tick_seconds;
    const auto frame_step = static_cast<std::uint32_t>(std::max(
        1.0F, std::round(std::max(0.0F, seconds * 60.0F))));
    const auto decrement = [frame_step](std::uint32_t& value) noexcept {
        value = value > frame_step ? value - frame_step : 0;
    };

    agent.attack_timer = std::max(0.0F, agent.attack_timer - seconds);
    if (agent.cretaphid_state == Defeated) {
        agent.state = Defeated;
        agent.invulnerable = true;
        agent.cretaphid_crystal_open = false;
        agent.velocity = {};
        decrement(agent.cretaphid_state_timer);
        if (agent.cretaphid_state_timer == 0) {
            const auto id = agent.id;
            static_cast<void>(destroy_enemy(id));
        }
        return;
    }

    std::size_t target_index = players_.size();
    float nearest_squared = std::numeric_limits<float>::max();
    for (std::size_t index = 0; index < players_.size(); ++index) {
        const auto& player = players_[index];
        if (!objective_player(player)) {
            continue;
        }
        const float distance = distance_squared(player.position,
                                                agent.position);
        if (distance <= 13.0F * 13.0F && distance < nearest_squared) {
            nearest_squared = distance;
            target_index = index;
        }
    }
    if (target_index != players_.size()) {
        auto& target = players_[target_index];
        agent.target_slot = target.slot_index;
        agent.facing = normalized_or(
            {target.position.x - agent.position.x, 0.0F,
             target.position.z - agent.position.z}, agent.facing);
        if (distance_squared(target.position, agent.position)
                <= 1.25F * 1.25F
            && agent.attack_timer <= 0.0F) {
            apply_enemy_contact_damage(agent, target, 10);
            agent.attack_timer = 0.5F;
        }
    } else {
        agent.target_slot = 0xff;
    }
    agent.velocity = {};
    agent.invulnerable = true;
    agent.state = agent.cretaphid_state;

    const std::size_t phase_index = std::min<std::size_t>(
        agent.cretaphid_phase, profile.phases.size() - 1);
    const auto& phase = profile.phases[phase_index];
    std::size_t vulnerable_eyes = 0;
    std::size_t living_eyes = 0;
    for (const auto& child : enemies_) {
        if (child.parent_enemy_id != agent.id
            || child.cretaphid_part_kind != 1 || !child.active
            || child.cretaphid_part_state == 2 || child.health == 0) {
            continue;
        }
        ++living_eyes;
        if ((child.state == 1 || child.state == 2 || child.state == 4)
            && !child.invulnerable) {
            ++vulnerable_eyes;
        }
    }

    const auto crystal = std::find_if(
        enemies_.begin(), enemies_.end(),
        [parent_id = agent.id](const EnemyState& value) {
            return value.parent_enemy_id == parent_id
                && value.cretaphid_part_kind == 2 && value.active;
        });
    const auto reset_phase_parts = [this, &agent, profile](
                                       std::size_t next_phase) {
        const auto& next = profile.phases[next_phase];
        for (auto& child : enemies_) {
            if (child.parent_enemy_id != agent.id
                || child.cretaphid_part_kind == 0) {
                continue;
            }
            child.cretaphid_phase = static_cast<std::uint8_t>(next_phase);
            child.cretaphid_part_state = 0;
            child.cretaphid_part_timer = 0;
            child.cretaphid_part_shot_timer = 0;
            child.cretaphid_part_shots_remaining = 0;
            child.active = true;
            child.visible = true;
            child.invulnerable = true;
            if (child.cretaphid_part_kind == 1) {
                const auto index = std::min<std::size_t>(
                    child.cretaphid_part_index, next.eye_state.size() - 1);
                child.health = child.health_max = profile.eye_health;
                child.state = next.eye_state[index];
                child.cretaphid_beam_type = next.eye_beam_type[index];
                child.cretaphid_segment_index = static_cast<std::uint8_t>(
                    index / 4u);
            } else {
                // Behavior00 lowers the crystal to the threshold of the
                // phase that just ended, then switches the eye ring. The
                // next phase's threshold is consumed only after the player
                // gets a fresh exposed-crystal window.
                const auto crystal_phase = next_phase > 0 ? next_phase - 1 : 0;
                child.health = profile.phases[crystal_phase].crystal_health;
                child.health_max = profile.crystal_health;
                child.state = 0;
            }
        }
    };

    switch (agent.cretaphid_state) {
    case Intro:
        decrement(agent.cretaphid_state_timer);
        if (agent.cretaphid_state_timer == 0) {
            agent.cretaphid_state = Eyes;
            agent.state = Eyes;
        }
        return;
    case Eyes:
        // Enemy19 waits for the phase's active eye group to be destroyed.
        // Closed eyes remain visible but are invulnerable, so count only the
        // authored vulnerable states here.
        if (living_eyes != 0 && vulnerable_eyes != 0) {
            return;
        }
        if (crystal == enemies_.end()) {
            agent.cretaphid_state = Defeated;
            agent.cretaphid_state_timer = 36u * 2u;
            return;
        }
        agent.cretaphid_state = Crystal;
        agent.state = Crystal;
        agent.cretaphid_crystal_open = true;
        crystal->state = Crystal;
        crystal->visible = true;
        crystal->invulnerable = false;
        crystal->cretaphid_part_shot_timer = static_cast<std::uint32_t>(
            phase.crystal_shot_delay) * 2u;
        return;
    case Crystal:
        agent.cretaphid_crystal_open = true;
        agent.state = Crystal;
        if (crystal == enemies_.end() || crystal->health <= phase.crystal_health) {
            agent.cretaphid_crystal_open = false;
            if (agent.cretaphid_phase + 1u >= profile.phases.size()) {
                agent.cretaphid_state = Defeated;
                agent.cretaphid_state_timer = 36u * 2u;
                agent.state = Defeated;
                agent.health = 1;
                return;
            }
            agent.cretaphid_state = PhaseChange;
            agent.cretaphid_state_timer = static_cast<std::uint32_t>(
                profile.phase_flash_time) * 2u;
            agent.state = PhaseChange;
            if (crystal != enemies_.end()) {
                crystal->invulnerable = true;
            }
            return;
        }
        return;
    case PhaseChange:
        decrement(agent.cretaphid_state_timer);
        if (agent.cretaphid_state_timer != 0) {
            return;
        }
        agent.cretaphid_phase = static_cast<std::uint8_t>(
            std::min<std::size_t>(agent.cretaphid_phase + 1u,
                                  profile.phases.size() - 1));
        reset_phase_parts(agent.cretaphid_phase);
        agent.cretaphid_crystal_open = false;
        agent.cretaphid_state = Eyes;
        agent.state = Eyes;
        return;
    default:
        agent.cretaphid_state = Intro;
        agent.cretaphid_state_timer = 60u * 2u;
        agent.state = Intro;
        return;
    }
}
} // namespace fruityprime::gameplay

namespace fruityprime::enemy {

CretaphidProfile decode_cretaphid_profile(
    std::uint8_t id, std::span<const std::uint8_t> fields,
    net::Vec3 origin) noexcept {
    CretaphidProfile result;
    if (id != static_cast<std::uint8_t>(formats::EnemyType::Cretaphid)
        || fields.size() < 4 + 4 * 64) {
        return result;
    }

    result.subtype = static_cast<std::uint8_t>(std::min<std::uint32_t>(
        detail::read_u32(fields, 0), 3u));
    result.hurt_volume = detail::read_volume(fields, 4, origin);

    const auto make_phase = [](
        std::uint16_t crystal_health,
        std::uint16_t crystal_shot_time,
        std::uint16_t crystal_shot_delay,
        std::uint16_t crystal_up_time,
        std::uint16_t crystal_beam_damage,
        std::uint16_t eye_beam_damage,
        std::uint16_t eye_splash_damage,
        std::uint16_t eye_contact_damage,
        std::array<std::uint8_t, 12> eye_state,
        std::array<std::uint8_t, 12> eye_beam_type,
        std::array<std::uint8_t, 12> eye_beam_spawn_min,
        std::array<std::uint8_t, 12> eye_beam_spawn_max,
        std::array<std::uint16_t, 12> eye_beam_cooldown,
        std::array<std::uint16_t, 12> eye_state_timer0,
        std::array<std::uint16_t, 12> eye_state_timer1,
        std::array<std::uint16_t, 12> eye_state_timer2,
        std::array<std::uint16_t, 12> eye_state_timer3) {
        CretaphidPhase result;
        result.crystal_health = crystal_health;
        result.crystal_shot_time = crystal_shot_time;
        result.crystal_shot_delay = crystal_shot_delay;
        result.crystal_up_time = crystal_up_time;
        result.crystal_beam_damage = crystal_beam_damage;
        result.eye_beam_damage = eye_beam_damage;
        result.eye_splash_damage = eye_splash_damage;
        result.eye_contact_damage = eye_contact_damage;
        result.eye_state = eye_state;
        result.eye_beam_type = eye_beam_type;
        result.eye_beam_spawn_min = eye_beam_spawn_min;
        result.eye_beam_spawn_max = eye_beam_spawn_max;
        result.eye_beam_cooldown = eye_beam_cooldown;
        result.eye_state_timer0 = eye_state_timer0;
        result.eye_state_timer1 = eye_state_timer1;
        result.eye_state_timer2 = eye_state_timer2;
        result.eye_state_timer3 = eye_state_timer3;
        return result;
    };

    const auto u8 = [](std::uint8_t value) {
        return detail::fill_cretaphid_array<std::uint8_t>(value);
    };
    const auto u16 = [](std::uint16_t value) {
        return detail::fill_cretaphid_array<std::uint16_t>(value);
    };
    static const std::array<std::array<CretaphidPhase, 3>, 4> values{{
        {{
            make_phase(360, 30, 1, 150, 6, 3, 0, 3,
                {5, 5, 5, 2, 2, 2, 2, 2, 2, 2, 2, 2},
                u8(2), u8(1), u8(2), u16(5), u16(30), u16(30),
                u16(200), u16(20)),
            make_phase(200, 20, 1, 120, 6, 3, 0, 3,
                {2, 2, 2, 5, 5, 5, 5, 2, 2, 2, 2, 2},
                u8(2), u8(1), u8(2), u16(5), u16(30), u16(30),
                u16(280), u16(20)),
            make_phase(0, 13, 1, 90, 6, 3, 0, 3,
                {2, 2, 2, 2, 2, 2, 2, 5, 5, 5, 5, 5},
                u8(2), u8(1), u8(2), u16(5), u16(30), u16(30),
                u16(360), u16(20))
        }},
        {{
            make_phase(400, 15, 10, 150, 10, 5, 0, 3,
                {1, 1, 1, 0, 0, 0, 0, 4, 4, 4, 4, 4},
                u8(1), u8(1), u8(1), u16(10), u16(130), u16(50),
                u16(10), u16(50)),
            make_phase(230, 12, 10, 120, 10, 5, 0, 3,
                {0, 0, 0, 4, 4, 4, 4, 1, 1, 1, 1, 1},
                u8(1), u8(1), u8(1), u16(10), u16(130), u16(45),
                u16(10), u16(45)),
            make_phase(0, 9, 10, 90, 10, 5, 0, 3,
                {1, 0, 1, 0, 0, 1, 0, 1, 0, 1, 0, 1},
                u8(1), u8(1), u8(1), u16(10),
                {60, 90, 30, 60, 90, 120, 30, 60, 90, 120, 90, 30},
                u16(45), u16(10), u16(45))
        }},
        {{
            make_phase(420, 15, 10, 150, 12, 4, 1, 8,
                {0, 0, 0, 1, 1, 1, 1, 4, 4, 4, 4, 4},
                {2, 2, 2, 1, 1, 1, 1, 1, 1, 1, 1, 1},
                u8(1), u8(1), {5, 5, 5, 10, 10, 10, 10, 10, 10, 10, 10, 10},
                {30, 30, 30, 130, 130, 130, 130, 130, 130, 130, 130, 130},
                {30, 30, 30, 50, 50, 50, 50, 50, 50, 50, 50, 50},
                {180, 180, 180, 10, 10, 10, 10, 10, 10, 10, 10, 10},
                {20, 20, 20, 50, 50, 50, 50, 50, 50, 50, 50, 50}),
            make_phase(230, 12, 10, 120, 12, 4, 1, 8,
                {1, 1, 1, 0, 0, 0, 0, 4, 4, 4, 4, 4},
                {1, 1, 1, 2, 2, 2, 2, 1, 1, 1, 1, 1},
                u8(1), u8(1), {10, 10, 10, 5, 5, 5, 5, 10, 10, 10, 10, 10},
                {130, 130, 130, 30, 30, 30, 30, 130, 130, 130, 130, 130},
                {45, 45, 45, 30, 30, 30, 30, 45, 45, 45, 45, 45},
                {10, 10, 10, 220, 220, 220, 220, 10, 10, 10, 10, 10},
                {45, 45, 45, 20, 20, 20, 20, 45, 45, 45, 45, 45}),
            make_phase(0, 9, 10, 90, 12, 4, 1, 8,
                {1, 1, 1, 4, 4, 4, 4, 0, 0, 0, 0, 0},
                {1, 1, 1, 1, 1, 1, 1, 2, 2, 2, 2, 2},
                u8(1), u8(1), {10, 10, 10, 10, 10, 10, 10, 5, 5, 5, 5, 5},
                {40, 40, 40, 40, 40, 40, 40, 30, 30, 30, 30, 30},
                {45, 45, 45, 45, 45, 45, 45, 30, 30, 30, 30, 30},
                {10, 10, 10, 10, 10, 10, 10, 260, 260, 260, 260, 260},
                {45, 45, 45, 45, 45, 45, 45, 20, 20, 20, 20, 20})
        }},
        {{
            make_phase(385, 12, 10, 150, 18, 4, 1, 10,
                {0, 0, 0, 1, 1, 1, 1, 4, 4, 4, 4, 4},
                {2, 2, 2, 0, 0, 0, 0, 0, 0, 0, 0, 0},
                u8(1), u8(1), {5, 5, 5, 10, 10, 10, 10, 10, 10, 10, 10, 10},
                {30, 30, 30, 130, 130, 130, 130, 130, 130, 130, 130, 130},
                {30, 30, 30, 50, 50, 50, 50, 50, 50, 50, 50, 50},
                {180, 180, 180, 10, 10, 10, 10, 10, 10, 10, 10, 10},
                {20, 20, 20, 50, 50, 50, 50, 50, 50, 50, 50, 50}),
            make_phase(220, 10, 10, 120, 18, 4, 1, 10,
                {1, 1, 1, 0, 0, 0, 0, 4, 4, 4, 4, 4},
                {0, 0, 0, 2, 2, 2, 2, 0, 0, 0, 0, 0},
                u8(1), u8(1), {10, 10, 10, 5, 5, 5, 5, 10, 10, 10, 10, 10},
                {130, 130, 130, 30, 30, 30, 30, 130, 130, 130, 130, 130},
                {45, 45, 45, 30, 30, 30, 30, 45, 45, 45, 45, 45},
                {10, 10, 10, 220, 220, 220, 220, 10, 10, 10, 10, 10},
                {45, 45, 45, 20, 20, 20, 20, 45, 45, 45, 45, 45}),
            make_phase(0, 8, 10, 90, 18, 4, 1, 10,
                {1, 1, 1, 4, 4, 4, 4, 0, 0, 0, 0, 0},
                {0, 0, 0, 0, 0, 0, 0, 2, 2, 2, 2, 2},
                u8(1), u8(1), {10, 10, 10, 10, 10, 10, 10, 5, 5, 5, 5, 5},
                {40, 40, 40, 40, 40, 40, 40, 30, 30, 30, 30, 30},
                {45, 45, 45, 45, 45, 45, 45, 30, 30, 30, 30, 30},
                {10, 10, 10, 10, 10, 10, 10, 260, 260, 260, 260, 260},
                {45, 45, 45, 45, 45, 45, 45, 20, 20, 20, 20, 20})
        }}
    }};

    static constexpr std::array<std::uint16_t, 4> health{
        490, 540, 570, 550};
    static constexpr std::array<std::uint16_t, 4> eye_health{
        12, 4, 4, 4};
    const std::size_t subtype = std::min<std::size_t>(
        result.subtype, values.size() - 1);
    result.crystal_health = health[subtype];
    result.eye_health = eye_health[subtype];
    result.phase_flash_time = 60;
    result.collision_radius = 3276.0F / 4096.0F;
    result.phases = values[subtype];
    result.supported = result.hurt_volume.kind != scene::VolumeKind::Invalid;
    return result;
}

} // namespace fruityprime::enemy

