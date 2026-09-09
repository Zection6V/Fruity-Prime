// Native counterpart of src/MphRead/Mods/Network/WeaponDps.cs.
//
// The managed implementation is a hidden GameWindow.  This implementation
// intentionally keeps the measurement headless: it creates the same two
// players, puts the shooter in front of the victim, holds the selected weapon
// for a fixed 60 Hz window, and reads damage/projectile state from the native
// Session.  No synthetic damage is injected, so the result exercises the
// ordinary weapon, collision, and health paths.
#include "Mods/Network/weapon_dps.hpp"

#include "Entities/gameplay.hpp"
#include "Metadata/metadata.hpp"
#include "Entities/scene.hpp"

#include <algorithm>
#include <array>
#include <cmath>
#include <iomanip>
#include <limits>
#include <ostream>
#include <stdexcept>

namespace fruityprime::net {
namespace {

constexpr std::array<std::string_view, metadata::WeaponCount> BeamNames{{
    "PowerBeam", "VoltDriver", "Missile", "Battlehammer", "Imperialist",
    "Judicator", "Magmaul", "ShockCoil", "OmegaCannon"
}};

[[nodiscard]] Vec3 add(Vec3 left, Vec3 right) noexcept {
    return {left.x + right.x, left.y + right.y, left.z + right.z};
}

[[nodiscard]] Vec3 subtract(Vec3 left, Vec3 right) noexcept {
    return {left.x - right.x, left.y - right.y, left.z - right.z};
}

[[nodiscard]] Vec3 multiply(Vec3 value, float scalar) noexcept {
    return {value.x * scalar, value.y * scalar, value.z * scalar};
}

[[nodiscard]] float length_squared(Vec3 value) noexcept {
    return value.x * value.x + value.y * value.y + value.z * value.z;
}

[[nodiscard]] Vec3 normalized_or(Vec3 value, Vec3 fallback) noexcept {
    const float squared = length_squared(value);
    if (!std::isfinite(squared) || squared <= 0.000001F) {
        return fallback;
    }
    const float inverse = 1.0F / std::sqrt(squared);
    return multiply(value, inverse);
}

[[nodiscard]] bool active_spawned(const PlayerState& player) noexcept {
    return (player.flags & PlayerState::FlagActive) != 0
        && (player.flags & PlayerState::FlagSpawned) != 0
        && (player.flags & PlayerState::FlagSpectating) == 0
        && player.health > 0;
}

[[nodiscard]] std::string_view beam_name(std::int32_t beam_type) noexcept {
    if (beam_type < 0
        || static_cast<std::size_t>(beam_type) >= BeamNames.size()) {
        return "Unknown";
    }
    return BeamNames[static_cast<std::size_t>(beam_type)];
}

} // namespace

WeaponDpsResult WeaponDps::run(const scene::Room& room,
                               WeaponDpsOptions options) {
    WeaponDpsResult result;
    result.room = room.definition().name;
    result.beam_type = options.beam_type;
    result.requested_seconds = options.seconds;
    result.distance = options.distance;
    result.hunter = std::string(metadata::hunter_info(options.hunter).name);
    result.beam = std::string(beam_name(options.beam_type));

    if (options.hunter >= metadata::PlayableHunterCount) {
        result.error = "hunter must be one of the seven playable hunters";
        return result;
    }
    if (!std::isfinite(options.seconds) || options.seconds < 0.0) {
        result.error = "seconds must be finite and >= 0";
        return result;
    }
    if (!std::isfinite(options.distance)) {
        result.error = "distance must be finite";
        return result;
    }
    result.distance = std::clamp(options.distance, 0.5F, 40.0F);
    result.native_weapon = metadata::native_weapon_slot_from_beam(
        options.beam_type);
    if (result.native_weapon == 0xff) {
        result.error = "beam must be a playable BeamType ordinal (0..8)";
        return result;
    }

    try {
        gameplay::Config config;
        config.mode = 3; // GameMode.Battle
        config.point_goal = 7;
        config.team_mode = false;
        config.friendly_fire = false;
        config.survival_mode = false;

        gameplay::Session session(room, config);
        // The managed probe deliberately reserves slot 0 for the victim and
        // slot 1 for the selected hunter.  Preserve that ordering because it
        // also makes owner-slot accounting in the projectile list unambiguous.
        static_cast<void>(session.add_player(0, 0));
        static_cast<void>(session.add_player(1, options.hunter));

        const PlayerState& initial_victim = session.player(0);
        const Vec3 victim_position = initial_victim.position;
        Vec3 facing = initial_victim.facing;
        facing.y = 0.0F;
        facing = normalized_or(facing, {0.0F, 0.0F, 1.0F});
        const Vec3 shooter_position = add(
            victim_position, multiply(facing, result.distance));
        const Vec3 shooter_facing = multiply(facing, -1.0F);
        const auto health_max = session.inventory(0).health_max;

        session.place_player(0, victim_position, facing, false);
        // configure_story_hunter is the native equivalent of
        // PlayerEntity.InitEnemyHunter: it applies the authored weapon and
        // gives the probe infinite ammo, avoiding an ammo refill becoming part
        // of the DPS result.
        session.configure_story_hunter(
            1, shooter_position, shooter_facing, health_max, health_max, 0,
            static_cast<std::uint32_t>(options.beam_type));
        result.placed = active_spawned(session.player(0))
            && active_spawned(session.player(1));
        if (!result.placed) {
            result.error = "players were not active and spawned";
            return result;
        }

        result.start_health = static_cast<int>(session.player(0).health);
        int last_health = result.start_health;
        result.distance = std::clamp(options.distance, 0.5F, 40.0F);
        const auto requested_ticks = static_cast<std::uint64_t>(std::ceil(
            options.seconds / static_cast<double>(config.tick_seconds)));
        // GameWindow executes one Step even for a zero-second request. Keep
        // the same useful behavior for command-line probes.
        const std::uint64_t ticks = std::max<std::uint64_t>(1, requested_ticks);

        for (std::uint64_t tick = 0; tick < ticks; ++tick) {
            const auto& victim = session.player(0);
            const auto& shooter = session.player(1);
            if (!active_spawned(shooter) || !active_spawned(victim)) {
                break;
            }
            if (result.kill_frames >= 0) {
                break;
            }

            // Aim chest-to-chest, matching the managed probe's deliberate
            // +0.5 y offset. The native projectile is emitted at player.y
            // +0.9, while the native collision sphere is centred on player.y;
            // account for that muzzle offset here so the same chest line is
            // tested instead of sending a horizontal shot over the target.
            const Vec3 to_victim = subtract(
                add(victim.position, {0.0F, 0.4F, 0.0F}),
                add(shooter.position, {0.0F, 0.9F, 0.0F}));
            const Vec3 aim = normalized_or(to_victim, shooter.facing);
            session.set_input(0, gameplay::Input{});
            session.set_input(1, gameplay::Input{
                IntentButtons::Shoot, aim, result.native_weapon});
            session.tick();

            const int current_health = static_cast<int>(
                session.player(0).health);
            if (current_health < last_health) {
                result.damage += last_health - current_health;
                ++result.hits;
            }
            last_health = current_health;
            result.beam_frames += static_cast<int>(std::any_of(
                session.projectiles().begin(), session.projectiles().end(),
                [](const gameplay::Projectile& projectile) {
                    return projectile.owner_slot == 1;
                }));
            ++result.firing_frames;

            // The managed probe keeps the shooter alive after reading the
            // damage. Preserve that guard without healing the victim.
            session.mutable_player(1).health = session.inventory(1).health_max;
            if (current_health == 0) {
                result.killed = true;
                result.kill_frames = result.firing_frames;
                break;
            }
        }

        result.ok = result.placed && result.firing_frames > 0;
        if (!result.ok && result.error.empty()) {
            result.error = "players did not remain active long enough to fire";
        }
    } catch (const std::exception& error) {
        result.error = error.what();
    }
    return result;
}

void WeaponDps::print(std::ostream& output, const WeaponDpsResult& result) {
    if (!result.ok) {
        output << "DPSFAIL " << result.room << " | " << result.hunter << ' '
               << result.beam << " | "
               << (result.error.empty() ? "never got set up" : result.error)
               << '\n';
        return;
    }

    const double seconds = result.firing_frames / 60.0;
    const double window = result.kill_frames > 0
        ? result.kill_frames / 60.0 : seconds;
    const double safe_window = window > std::numeric_limits<double>::epsilon()
        ? window : 1.0 / 60.0;
    output << "DPS " << result.room << " | " << result.hunter
           << " holding " << result.beam << " at "
           << std::fixed << std::setprecision(1) << result.distance
           << " units | ";
    if (result.killed) {
        output << "killed " << result.start_health << " hp in "
               << std::setprecision(2) << (result.kill_frames / 60.0)
               << " s";
    } else {
        output << "did not kill " << result.start_health << " hp in "
               << std::setprecision(1) << seconds << " s";
    }
    output << " | damage " << result.damage
           << " | hits " << result.hits
           << " | " << std::setprecision(1)
           << (result.damage / safe_window) << " per second"
           << " | "
           << (result.hits > 0
                   ? result.damage / static_cast<double>(result.hits) : 0.0)
           << " per hit"
           << " | " << (result.hits / safe_window) << " hits per second"
           << " | beam alive on " << result.beam_frames << " of "
           << result.firing_frames << " frame(s)\n";
}

} // namespace fruityprime::net
