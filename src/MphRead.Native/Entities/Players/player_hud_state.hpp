#pragma once

// Native counterpart of the HUD whiteout and disruption state machines in
// Entities/Players/PlayerHud.cs.
//
// The whiteout is what the screen does when the player is blinded: a band
// sweeps outward from the middle of a 192-entry table, reaches the edge, and
// then fades.  The sweep position follows a cubic in time, not a linear ramp,
// so reproducing the polynomial is what makes it look right rather than
// merely white.
//
// The disruption is the Shock Coil's HUD interference: it ramps in, holds for
// a timer, ramps out, and then waits before it can start again.

#include <array>
#include <cstdint>

namespace fruityprime::players {

class PlayerHudState final {
public:
    static constexpr std::size_t WhiteoutTableSize = 192;

    // PlayerHud.HudWhiteoutState / HudWhiteoutFactor / HudWhiteoutTable.
    // A state of -1 means no whiteout is running.
    [[nodiscard]] int whiteout_state() const noexcept { return whiteout_state_; }
    [[nodiscard]] float whiteout_factor() const noexcept {
        return whiteout_factor_;
    }
    [[nodiscard]] const std::array<float, WhiteoutTableSize>& whiteout_table()
        const noexcept {
        return whiteout_table_;
    }

    // PlayerHud.BeginWhiteout
    void begin_whiteout(float global_elapsed) noexcept {
        whiteout_state_ = 0;
        whiteout_factor_ = 0.0F;
        whiteout_time_ = global_elapsed;
        update_whiteout_table(0.0F);
    }

    // PlayerHud.EndWhiteout
    void end_whiteout() noexcept {
        whiteout_state_ = -1;
        whiteout_factor_ = 0.0F;
    }

    // PlayerHud.UpdateWhiteoutState
    void update_whiteout_state(float global_elapsed, float frame_time) noexcept;

    // PlayerHud.HudDisruptedState / HudDisruptionFactor
    [[nodiscard]] int disrupted_state() const noexcept {
        return disrupted_state_;
    }
    [[nodiscard]] float disruption_factor() const noexcept {
        return disruption_factor_;
    }

    void begin_disrupted() noexcept {
        if (disrupted_state_ == 0) {
            disrupted_state_ = 1;
        }
    }

    // PlayerHud.HudEndDisrupted
    void hud_end_disrupted() noexcept {
        if (disrupted_state_ != 0) {
            disrupted_state_ = 0;
            disrupted_timer_ = 0;
            disruption_factor_ = 0.0F;
        }
    }

    // PlayerHud.UpdateDisruptedState
    void update_disrupted_state() noexcept;

private:
    // PlayerHud.UpdateWhiteoutTable
    void update_whiteout_table(float value) noexcept;

    // The sweep position: initial velocity 1 with a constant jerk of 0.004,
    // evaluated in frames rather than seconds.
    [[nodiscard]] float whiteout_position(float global_elapsed) const noexcept;

    int whiteout_state_ = -1;
    float whiteout_factor_ = 0.0F;
    float whiteout_amount_ = 0.0F;
    float whiteout_time_ = 0.0F;
    std::array<float, WhiteoutTableSize> whiteout_table_{};

    int disrupted_state_ = 0;
    int disrupted_timer_ = 0;
    float disruption_factor_ = 0.0F;
};

} // namespace fruityprime::players
