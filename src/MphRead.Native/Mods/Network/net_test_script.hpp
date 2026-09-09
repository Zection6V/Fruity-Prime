#pragma once

#include "Mods/Network/net_protocol.hpp"
#include "Mods/Network/player_entity_net_aim.hpp"

#include <array>
#include <cstddef>
#include <cstdint>

namespace fruityprime::gameplay {
class Session;
}

namespace fruityprime::net {

// The fixed feature tour from MphRead/Mods/Network/NetTestScript.cs.  It is
// a stateful object instead of a process-wide static because native map
// audits can run more than one Session in the same process.  Calls still
// write Session::Input, the same surface as a desktop controller and a bot.
enum class TestPhase : std::uint8_t {
    Idle,
    Walk,
    Jump,
    Turn,
    Shoot,
    SwitchWeapons,
    Charge,
    MorphA,
    AltAttackA,
    MorphB,
    AltAttackB,
    Unmorph,
    Zoom,
    Afflict,
    Duel
};

class NetTestScript final {
public:
    static constexpr std::size_t PhaseCount = 15;
    static constexpr double DefaultPhaseSeconds = 5.0;

    NetTestScript() noexcept;

    void reset() noexcept;
    void set_enabled(bool enabled) noexcept { enabled_ = enabled; }
    [[nodiscard]] bool enabled() const noexcept { return enabled_; }
    void set_phase_seconds(double seconds) noexcept;
    [[nodiscard]] double phase_seconds() const noexcept {
        return phase_seconds_;
    }
    void set_local_slot(int slot) noexcept { local_slot_ = slot; }
    void set_network_active(bool active) noexcept {
        network_active_ = active;
    }

    [[nodiscard]] TestPhase phase(double server_elapsed = -1.0) const noexcept;

    // server_elapsed is used when the authoritative match clock is known;
    // otherwise the local fixed-step frame is the same fallback as C#.
    void apply(gameplay::Session& session, std::uint8_t slot,
               double server_elapsed = -1.0);
    void apply_offline(gameplay::Session& session, std::uint8_t slot,
                       int frame);
    void hold_fire(gameplay::Session& session, std::uint8_t slot, bool down);
    void rest(gameplay::Session& session, std::uint8_t slot,
              bool want_biped);
    void walk_forward(gameplay::Session& session, std::uint8_t slot);

    [[nodiscard]] int frame() const noexcept { return frame_; }
    [[nodiscard]] float aim_delta_x() const noexcept { return aim_delta_x_; }
    [[nodiscard]] float aim_delta_y() const noexcept { return aim_delta_y_; }
    [[nodiscard]] int frames_on_target() const noexcept {
        return frames_on_target_;
    }

private:
    struct Controls {
        IntentButtons buttons = IntentButtons::None;
    };

    void drive(gameplay::Session& session, std::uint8_t slot);
    void clear(Controls& controls) noexcept;
    void finish(gameplay::Session& session, std::uint8_t slot,
                const Controls& controls);
    void aim_at(gameplay::Session& session, std::uint8_t slot,
                int target_slot) noexcept;
    [[nodiscard]] int find_target(const gameplay::Session& session,
                                  std::uint8_t slot) const noexcept;
    [[nodiscard]] bool settled(const gameplay::Session& session,
                               std::uint8_t slot) const noexcept;
    [[nodiscard]] bool even(std::uint8_t slot) const noexcept;
    void morph_or_shoot(gameplay::Session& session, std::uint8_t slot,
                        Controls& controls, bool morphing);
    void alt_attack_or_shoot(Controls& controls, bool attacking,
                             bool on_target) noexcept;
    void duel(gameplay::Session& session, std::uint8_t slot,
              Controls& controls, int target_slot, bool on_target,
              bool charged);
    void square(Controls& controls) noexcept;
    void hold(Controls& controls, IntentButtons button, bool down) noexcept;
    void arm_zoom_weapon(gameplay::Session& session, std::uint8_t slot);
    void arm_affinity_weapon(gameplay::Session& session, std::uint8_t slot);

    std::array<TestPhase, PhaseCount> order_{};
    PlayerEntityNetAim aim_;
    double phase_seconds_ = DefaultPhaseSeconds;
    bool enabled_ = false;
    bool network_active_ = false;
    int local_slot_ = -1;
    int frame_ = 0;
    double server_elapsed_ = -1.0;
    int offline_slot_ = -1;
    int stuck_frames_ = 0;
    bool stuck_direction_ = false;
    Vec3 last_position_{};
    bool last_position_valid_ = false;
    int release_frames_ = 0;
    float aim_delta_x_ = 0.0F;
    float aim_delta_y_ = 0.0F;
    int frames_on_target_ = 0;
};

} // namespace fruityprime::net
