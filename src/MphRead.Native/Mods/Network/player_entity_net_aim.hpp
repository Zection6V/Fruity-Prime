#pragma once

#include "Mods/Network/net_protocol.hpp"

#include <array>
#include <cstddef>
#include <cstdint>

namespace fruityprime::gameplay {
class Session;
}

namespace fruityprime::net {

class NetLog;

// Native counterpart of MphRead/Mods/Network/PlayerEntityNetAim.cs.
//
// The managed file is a partial PlayerEntity because it must reach private
// camera, weapon, animation, and collision fields.  Native gameplay keeps
// those fields in the engine-neutral PlayerState/Session boundary instead.
// This object owns the network-only history and applies the same state
// transitions without turning gameplay.cpp into a second network aggregate.
class PlayerEntityNetAim final {
public:
    PlayerEntityNetAim() noexcept;
    static constexpr std::size_t HistoryLength = 120;
    static constexpr std::size_t SlotCapacity = NetConfig::SlotCapacity;

    struct AimDelta {
        float x = 0.0F;
        float y = 0.0F;
    };

    struct Ammo {
        std::uint16_t ua = 0;
        std::uint16_t missiles = 0;
    };

    struct ScoreboardSize {
        std::size_t rows = 0;
        float height = 0.0F;
    };

    struct WeaponState {
        std::uint8_t weapon = 0xff;
        Ammo ammo{};
        bool can_charge = false;
        bool can_zoom = false;
        bool zoomed = false;
    };

    struct Metrics {
        std::uint64_t rejected_values = 0;
        std::uint64_t repaired_facing = 0;
        std::uint64_t repaired_aim = 0;
        std::uint64_t repaired_position = 0;
        std::uint64_t repaired_speed = 0;
        std::uint64_t network_spawns = 0;
        std::uint64_t network_deaths = 0;
        std::uint64_t form_switches = 0;
        std::uint64_t noted_inputs = 0;
    };

    void reset() noexcept;
    void note_room_changed() noexcept;
    void forget_slot(std::uint8_t slot) noexcept;

    // The newest position is stored at index zero, matching the managed
    // ModRecordNetworkPosition/ModGetNetworkPosition search order.
    void record_position(const gameplay::Session& session,
                         std::uint8_t slot, std::uint32_t frame) noexcept;
    [[nodiscard]] bool get_network_position(
        std::uint8_t slot, std::uint32_t frame, Vec3& position) const noexcept;

    [[nodiscard]] Vec3 gun_vector(std::uint8_t slot) const noexcept;
    // Remember an aim captured from a frontend which only has a const
    // Session view.  set_aim additionally writes the Session input used by
    // the fixed-step simulation.
    void remember_aim(std::uint8_t slot, Vec3 aim) noexcept;
    [[nodiscard]] bool set_aim(gameplay::Session& session, std::uint8_t slot,
                               Vec3 aim) noexcept;
    [[nodiscard]] bool refresh_network_aim(
        gameplay::Session& session, std::uint8_t slot, Vec3 aim) noexcept;

    [[nodiscard]] bool set_facing(gameplay::Session& session,
                                  std::uint8_t slot, Vec3 facing) noexcept;
    void set_spectating(gameplay::Session& session, std::uint8_t slot,
                        bool value) noexcept;
    [[nodiscard]] bool in_play(const gameplay::Session& session,
                               std::uint8_t slot) const noexcept;
    [[nodiscard]] bool is_in_play(const gameplay::Session& session,
                                  std::uint8_t slot) const noexcept;
    [[nodiscard]] bool network_spawn(gameplay::Session& session,
                                     std::uint8_t slot, Vec3 position,
                                     Vec3 facing, bool alt_form) noexcept;

    [[nodiscard]] AimDelta aim_delta_towards(
        const gameplay::Session& session, std::uint8_t slot,
        Vec3 target) const noexcept;
    [[nodiscard]] Vec3 aim_target(const gameplay::Session& session,
                                  std::uint8_t slot) const noexcept;

    void set_hunter(gameplay::Session& session, std::uint8_t slot,
                    std::uint8_t hunter) noexcept;
    [[nodiscard]] bool start_form_switch(gameplay::Session& session,
                                         std::uint8_t slot) noexcept;
    [[nodiscard]] bool force_form(gameplay::Session& session,
                                  std::uint8_t slot, bool alt_form) noexcept;

    [[nodiscard]] bool set_weapon(gameplay::Session& session,
                                  std::uint8_t slot,
                                  std::uint8_t weapon) noexcept;
    [[nodiscard]] Ammo ammo(const gameplay::Session& session,
                            std::uint8_t slot) const noexcept;
    void set_ammo(gameplay::Session& session, std::uint8_t slot,
                  std::uint16_t ua, std::uint16_t missiles) noexcept;
    [[nodiscard]] bool set_zoom(gameplay::Session& session,
                                std::uint8_t slot, bool zoomed) noexcept;
    [[nodiscard]] bool set_frozen(gameplay::Session& session,
                                  std::uint8_t slot, bool frozen) noexcept;
    [[nodiscard]] bool frozen(const gameplay::Session& session,
                              std::uint8_t slot) const noexcept;
    [[nodiscard]] bool can_zoom(const gameplay::Session& session,
                                std::uint8_t slot) const noexcept;
    [[nodiscard]] WeaponState weapon_state(
        const gameplay::Session& session, std::uint8_t slot) const noexcept;

    // Test/map-audit helpers from the managed partial class.  Native gameplay
    // has no private camera-angle methods, so these rotate the same absolute
    // aim vector and feed it through Session's input boundary.
    void apply_script_aim(gameplay::Session& session, std::uint8_t slot,
                          float delta_x_degrees,
                          float delta_y_degrees) noexcept;
    void note_input(gameplay::Session& session, std::uint8_t slot) noexcept;

    // Keep the last usable transform exactly as the managed network safety
    // net does.  Invalid network values are rejected and never published.
    void repair_vectors(gameplay::Session& session,
                        std::uint8_t slot) noexcept;
    [[nodiscard]] bool net_die(gameplay::Session& session,
                               std::uint8_t slot) noexcept;
    [[nodiscard]] bool can_be_hurt(const gameplay::Session& session,
                                   std::uint8_t slot) const noexcept;

    // The native HUD uses the same fixed 192-pixel layout as PlayerHud.cs.
    [[nodiscard]] static ScoreboardSize scoreboard_size(
        std::size_t active_players, bool ending, bool teams) noexcept;

    void log_collision_range(NetLog& log, std::uint8_t slot,
                             Vec3 previous, Vec3 current) const noexcept;

    [[nodiscard]] bool node_unresolved(std::uint8_t slot) const noexcept;
    [[nodiscard]] const Metrics& metrics() const noexcept { return metrics_; }

private:
    [[nodiscard]] static bool sane(Vec3 value) noexcept;
    [[nodiscard]] static Vec3 normalized_or(Vec3 value,
                                            Vec3 fallback) noexcept;
    [[nodiscard]] static float degrees_to_radians(float degrees) noexcept;
    [[nodiscard]] static float radians_to_degrees(float radians) noexcept;
    [[nodiscard]] static Vec3 rotate_aim(Vec3 aim, float delta_x_degrees,
                                         float delta_y_degrees) noexcept;

    std::array<std::array<Vec3, HistoryLength>, SlotCapacity> positions_{};
    std::array<std::array<std::uint32_t, HistoryLength>, SlotCapacity> frames_{};
    std::array<std::size_t, SlotCapacity> history_counts_{};
    std::array<Vec3, SlotCapacity> aim_{};
    std::array<bool, SlotCapacity> aim_seen_{};
    std::array<Vec3, SlotCapacity> last_good_aim_{};
    std::array<Vec3, SlotCapacity> last_good_facing_{};
    std::array<Vec3, SlotCapacity> last_good_position_{};
    std::array<std::uint32_t, SlotCapacity> form_mismatch_{};
    std::array<bool, SlotCapacity> node_unresolved_{};
    Metrics metrics_{};
};

using NetAim = PlayerEntityNetAim;

} // namespace fruityprime::net
