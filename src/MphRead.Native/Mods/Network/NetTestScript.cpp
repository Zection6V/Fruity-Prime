#include "Mods/Network/net_test_script.hpp"

#include "Entities/gameplay.hpp"
#include "Metadata/metadata.hpp"

#include <algorithm>
#include <array>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <limits>
#include <vector>

namespace fruityprime::net {
namespace {

constexpr std::array<TestPhase, NetTestScript::PhaseCount> Tour{
    TestPhase::Idle,          TestPhase::Walk,       TestPhase::Jump,
    TestPhase::Turn,          TestPhase::Shoot,      TestPhase::SwitchWeapons,
    TestPhase::Charge,        TestPhase::MorphA,     TestPhase::AltAttackA,
    TestPhase::MorphB,        TestPhase::AltAttackB, TestPhase::Unmorph,
    TestPhase::Zoom,          TestPhase::Afflict,    TestPhase::Duel};

constexpr float TurnRate = 6.0F;
constexpr float FiringCone = 6.0F;
constexpr float PreferredRange = 4.0F;

void set_button(IntentButtons& buttons, IntentButtons button,
                bool down) noexcept {
    std::uint32_t value = static_cast<std::uint32_t>(buttons);
    if (down) {
        value |= static_cast<std::uint32_t>(button);
    } else {
        value &= ~static_cast<std::uint32_t>(button);
    }
    buttons = static_cast<IntentButtons>(value);
}

[[nodiscard]] Vec3 subtract(Vec3 left, Vec3 right) noexcept {
    return {left.x - right.x, left.y - right.y, left.z - right.z};
}

[[nodiscard]] float length(Vec3 value) noexcept {
    return std::sqrt(value.x * value.x + value.y * value.y
                     + value.z * value.z);
}

[[nodiscard]] bool finite(Vec3 value) noexcept {
    return std::isfinite(value.x) && std::isfinite(value.y)
        && std::isfinite(value.z);
}

} // namespace

NetTestScript::NetTestScript() noexcept : order_(Tour), aim_() {
    reset();
}

void NetTestScript::reset() noexcept {
    phase_seconds_ = DefaultPhaseSeconds;
    enabled_ = false;
    local_slot_ = -1;
    frame_ = 0;
    server_elapsed_ = -1.0;
    offline_slot_ = -1;
    stuck_frames_ = 0;
    stuck_direction_ = false;
    last_position_ = {};
    last_position_valid_ = false;
    release_frames_ = 0;
    aim_delta_x_ = 0.0F;
    aim_delta_y_ = 0.0F;
    frames_on_target_ = 0;
    aim_.reset();
}

void NetTestScript::set_phase_seconds(double seconds) noexcept {
    if (std::isfinite(seconds) && seconds > 0.0) {
        phase_seconds_ = seconds;
    }
}

TestPhase NetTestScript::phase(double server_elapsed) const noexcept {
    const double elapsed = server_elapsed >= 0.0 && std::isfinite(server_elapsed)
        ? server_elapsed : static_cast<double>(frame_) / 60.0;
    const auto index = static_cast<std::size_t>(
        std::max(0.0, std::floor(elapsed / phase_seconds_))) % PhaseCount;
    return order_[index];
}

void NetTestScript::apply(gameplay::Session& session, std::uint8_t slot,
                          double server_elapsed) {
    if (!enabled_) {
        return;
    }
    ++frame_;
    server_elapsed_ = server_elapsed;
    drive(session, slot);
    server_elapsed_ = -1.0;
}

void NetTestScript::apply_offline(gameplay::Session& session,
                                  std::uint8_t slot, int frame) {
    offline_slot_ = static_cast<int>(slot);
    frame_ = std::max(0, frame);
    server_elapsed_ = -1.0;
    drive(session, slot);
    offline_slot_ = -1;
}

void NetTestScript::hold_fire(gameplay::Session& session, std::uint8_t slot,
                              bool down) {
    Controls controls;
    clear(controls);
    hold(controls, IntentButtons::Shoot, down);
    finish(session, slot, controls);
}

void NetTestScript::rest(gameplay::Session& session, std::uint8_t slot,
                         bool want_biped) {
    if (!session.has_player(slot)) {
        return;
    }
    Controls controls;
    clear(controls);
    const auto& player = session.player(slot);
    if (player.health == 0) {
        hold(controls, IntentButtons::Shoot, true);
    } else if (want_biped
               && (player.flags & PlayerState::FlagAltForm) != 0
               && settled(session, slot)) {
        hold(controls, IntentButtons::Morph, true);
        static_cast<void>(aim_.force_form(session, slot, false));
    }
    finish(session, slot, controls);
}

void NetTestScript::walk_forward(gameplay::Session& session,
                                 std::uint8_t slot) {
    if (!session.has_player(slot)) {
        return;
    }
    Controls controls;
    clear(controls);
    if (session.player(slot).health == 0) {
        hold(controls, IntentButtons::Shoot, true);
    } else {
        hold(controls, IntentButtons::MoveUp, true);
    }
    finish(session, slot, controls);
}

void NetTestScript::drive(gameplay::Session& session, std::uint8_t slot) {
    if (!session.has_player(slot)) {
        return;
    }
    Controls controls;
    clear(controls);
    const int target_slot = find_target(session, slot);
    aim_at(session, slot, target_slot);
    const bool on_target = target_slot >= 0;
    const TestPhase current = phase(server_elapsed_);

    const auto weapon = aim_.weapon_state(session, slot);
    if (current != TestPhase::Zoom && weapon.zoomed && frame_ % 8 == 0) {
        // Native Session treats Zoom as a held state, so perform the managed
        // toggle explicitly and publish the resulting ZoomedState bit.
        static_cast<void>(aim_.set_zoom(session, slot, false));
    }

    switch (current) {
    case TestPhase::Idle:
        break;
    case TestPhase::Walk:
        square(controls);
        break;
    case TestPhase::Jump:
        square(controls);
        hold(controls, IntentButtons::Jump, frame_ % 45 < 3);
        break;
    case TestPhase::Turn:
        aim_delta_x_ = 4.0F;
        aim_delta_y_ = std::sin(static_cast<float>(frame_) / 40.0F) * 2.0F;
        aim_.apply_script_aim(session, slot, aim_delta_x_, aim_delta_y_);
        break;
    case TestPhase::Shoot:
        hold(controls, IntentButtons::Shoot, frame_ % 30 < 20);
        break;
    case TestPhase::SwitchWeapons:
        hold(controls, IntentButtons::NextWeapon, frame_ % 30 == 0);
        hold(controls, IntentButtons::Shoot,
             frame_ % 30 > 10 && frame_ % 30 < 25);
        break;
    case TestPhase::Charge:
        hold(controls, IntentButtons::Shoot, true);
        break;
    case TestPhase::MorphA:
        morph_or_shoot(session, slot, controls, even(slot));
        break;
    case TestPhase::AltAttackA:
        alt_attack_or_shoot(controls, even(slot), on_target);
        break;
    case TestPhase::MorphB:
        morph_or_shoot(session, slot, controls, !even(slot));
        break;
    case TestPhase::AltAttackB:
        alt_attack_or_shoot(controls, !even(slot), on_target);
        break;
    case TestPhase::Unmorph:
        hold(controls, IntentButtons::Morph,
             settled(session, slot)
                 && (session.player(slot).flags & PlayerState::FlagAltForm) != 0
                 && frame_ % 40 == 0);
        if ((session.player(slot).flags & PlayerState::FlagAltForm) != 0
            && frame_ % 40 == 0) {
            static_cast<void>(aim_.force_form(session, slot, false));
        }
        square(controls);
        break;
    case TestPhase::Zoom: {
        if (!aim_.can_zoom(session, slot)) {
            arm_zoom_weapon(session, slot);
        }
        const auto current_weapon = aim_.weapon_state(session, slot);
        const bool want_zoom = current_weapon.can_zoom
            && !current_weapon.zoomed && frame_ % 8 == 0;
        if (want_zoom) {
            static_cast<void>(aim_.set_zoom(session, slot, true));
        }
        hold(controls, IntentButtons::Shoot,
             current_weapon.can_zoom && frame_ % 40 < 8);
        break;
    }
    case TestPhase::Afflict:
        arm_affinity_weapon(session, slot);
        duel(session, slot, controls, target_slot, on_target, true);
        break;
    case TestPhase::Duel:
        duel(session, slot, controls, target_slot, on_target, false);
        break;
    }
    finish(session, slot, controls);
}

void NetTestScript::clear(Controls& controls) noexcept {
    controls.buttons = IntentButtons::None;
    aim_delta_x_ = 0.0F;
    aim_delta_y_ = 0.0F;
}

void NetTestScript::finish(gameplay::Session& session, std::uint8_t slot,
                           const Controls& controls) {
    if (!session.has_player(slot)) {
        return;
    }
    IntentButtons buttons = controls.buttons;
    const auto& player = session.player(slot);
    // Native Input has no separate Morphing transition field.  Its fixed
    // step consumes AltFormState directly, so preserve the current form after
    // the explicit edge operation above.
    set_button(buttons, IntentButtons::AltFormState,
               (player.flags & PlayerState::FlagAltForm) != 0);
    set_button(buttons, IntentButtons::ZoomedState,
               (player.flags & PlayerState::FlagZoomed) != 0);
    const Vec3 aim = aim_.gun_vector(slot);
    session.set_input(slot, gameplay::Input{buttons, aim, 0xff});
    if (buttons != IntentButtons::None) {
        aim_.note_input(session, slot);
    }
}

void NetTestScript::aim_at(gameplay::Session& session, std::uint8_t slot,
                           int target_slot) noexcept {
    aim_delta_x_ = 0.0F;
    aim_delta_y_ = 0.0F;
    if (target_slot < 0
        || !session.has_player(static_cast<std::uint8_t>(target_slot))) {
        return;
    }
    const auto target = aim_.aim_target(
        session, static_cast<std::uint8_t>(target_slot));
    const auto delta = aim_.aim_delta_towards(session, slot, target);
    if (!finite({delta.x, delta.y, 0.0F})) {
        return;
    }
    aim_delta_x_ = std::clamp(delta.x, -TurnRate, TurnRate);
    aim_delta_y_ = std::clamp(delta.y, -TurnRate, TurnRate);
    if (std::abs(delta.x) < FiringCone && std::abs(delta.y) < FiringCone) {
        ++frames_on_target_;
    }
    aim_.apply_script_aim(session, slot, aim_delta_x_, aim_delta_y_);
}

int NetTestScript::find_target(const gameplay::Session& session,
                               std::uint8_t slot) const noexcept {
    if (network_active_) {
        for (std::size_t step = 1; step < NetConfig::SlotCapacity; ++step) {
            const auto candidate = static_cast<std::uint8_t>(
                (static_cast<std::size_t>(slot) + step)
                % NetConfig::SlotCapacity);
            if (!session.has_player(candidate)) {
                continue;
            }
            const auto& target = session.player(candidate);
            if ((target.flags & PlayerState::FlagActive) != 0
                && (target.flags & PlayerState::FlagSpawned) != 0
                && target.health > 0) {
                return candidate;
            }
        }
        return -1;
    }

    int best = -1;
    float best_distance = std::numeric_limits<float>::max();
    if (!session.has_player(slot)) {
        return best;
    }
    const auto origin = session.player(slot).position;
    for (std::size_t candidate = 0;
         candidate < NetConfig::SlotCapacity; ++candidate) {
        const auto other = static_cast<std::uint8_t>(candidate);
        if (other == slot || !session.has_player(other)) {
            continue;
        }
        const auto& target = session.player(other);
        if ((target.flags & PlayerState::FlagActive) == 0
            || (target.flags & PlayerState::FlagSpawned) == 0
            || target.health == 0) {
            continue;
        }
        const float distance = length(subtract(target.position, origin));
        if (distance < best_distance) {
            best_distance = distance;
            best = other;
        }
    }
    return best;
}

bool NetTestScript::settled(const gameplay::Session& session,
                            std::uint8_t slot) const noexcept {
    return session.has_player(slot);
}

bool NetTestScript::even(std::uint8_t slot) const noexcept {
    const int selected = offline_slot_ >= 0 ? offline_slot_ : local_slot_;
    return (selected >= 0 ? selected : static_cast<int>(slot)) % 2 == 0;
}

void NetTestScript::morph_or_shoot(gameplay::Session& session,
                                   std::uint8_t slot, Controls& controls,
                                   bool morphing) {
    const bool alt = (session.player(slot).flags & PlayerState::FlagAltForm) != 0;
    if (morphing) {
        const bool edge = settled(session, slot) && !alt && frame_ % 40 == 0;
        hold(controls, IntentButtons::Morph, edge);
        if (edge) {
            static_cast<void>(aim_.force_form(session, slot, true));
        }
        square(controls);
        hold(controls, IntentButtons::Boost, frame_ % 60 < 20);
        return;
    }
    const bool edge = settled(session, slot) && alt && frame_ % 40 == 0;
    hold(controls, IntentButtons::Morph, edge);
    if (edge) {
        static_cast<void>(aim_.force_form(session, slot, false));
    }
    hold(controls, IntentButtons::Shoot, !alt && frame_ % 30 < 24);
}

void NetTestScript::alt_attack_or_shoot(Controls& controls, bool attacking,
                                        bool on_target) noexcept {
    if (attacking) {
        square(controls);
        hold(controls, IntentButtons::AltAttack, frame_ % 45 < 6);
        return;
    }
    hold(controls, IntentButtons::Shoot, on_target && frame_ % 30 < 24);
}

void NetTestScript::duel(gameplay::Session& session, std::uint8_t slot,
                         Controls& controls, int target_slot, bool on_target,
                         bool charged) {
    if (target_slot < 0) {
        square(controls);
        hold(controls, IntentButtons::Shoot, frame_ % 60 < 20);
        return;
    }
    const auto& player = session.player(slot);
    const auto& target = session.player(static_cast<std::uint8_t>(target_slot));
    const float distance = length(subtract(target.position, player.position));
    const float moved = last_position_valid_
        ? length(subtract(player.position, last_position_)) : 0.0F;
    last_position_ = player.position;
    last_position_valid_ = true;
    stuck_frames_ = moved < 0.02F ? stuck_frames_ + 1 : 0;
    const bool stuck = stuck_frames_ > 20;
    if (stuck && stuck_frames_ > 90) {
        stuck_frames_ = 0;
        stuck_direction_ = !stuck_direction_;
    }
    hold(controls, IntentButtons::MoveUp, !stuck && distance > PreferredRange);
    hold(controls, IntentButtons::MoveDown,
         !stuck && distance < PreferredRange / 2.0F);
    hold(controls, IntentButtons::MoveLeft,
         stuck ? stuck_direction_
               : distance <= PreferredRange && frame_ / 90 % 2 == 0);
    hold(controls, IntentButtons::MoveRight,
         stuck ? !stuck_direction_
               : distance <= PreferredRange && frame_ / 90 % 2 == 1);
    hold(controls, IntentButtons::Jump,
         stuck ? stuck_frames_ % 30 < 3 : frame_ % 150 < 3);
    if (!charged) {
        hold(controls, IntentButtons::Shoot, on_target && frame_ % 30 < 24);
        return;
    }
    const auto weapon = aim_.weapon_state(session, slot);
    if (release_frames_ > 0) {
        --release_frames_;
        return;
    }
    if (on_target && weapon.can_charge) {
        release_frames_ = 4;
        // Session's fixed-step projectile boundary is edge-triggered.  The
        // four released frames above intentionally create that edge while
        // keeping the charge phase deterministic for every client.
        return;
    }
    hold(controls, IntentButtons::Shoot, true);
}

void NetTestScript::square(Controls& controls) noexcept {
    const int direction = frame_ / 60 % 4;
    hold(controls, IntentButtons::MoveUp, direction == 0);
    hold(controls, IntentButtons::MoveRight, direction == 1);
    hold(controls, IntentButtons::MoveDown, direction == 2);
    hold(controls, IntentButtons::MoveLeft, direction == 3);
}

void NetTestScript::hold(Controls& controls, IntentButtons button,
                         bool down) noexcept {
    set_button(controls.buttons, button, down);
}

void NetTestScript::arm_zoom_weapon(gameplay::Session& session,
                                    std::uint8_t slot) {
    for (std::uint8_t weapon = 0;
         weapon < static_cast<std::uint8_t>(metadata::WeaponCount); ++weapon) {
        if (metadata::weapon_info(weapon).can_zoom) {
            static_cast<void>(aim_.set_weapon(session, slot, weapon));
            return;
        }
    }
}

void NetTestScript::arm_affinity_weapon(gameplay::Session& session,
                                        std::uint8_t slot) {
    if (!session.has_player(slot)) {
        return;
    }
    const auto hunter = session.player_hunter(slot);
    static_cast<void>(aim_.set_weapon(
        session, slot, metadata::hunter_info(hunter).affinity_weapon));
}

} // namespace fruityprime::net
