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

    // ---- the reticle -----------------------------------------------
    // PlayerHud's target circle contracts the moment a shot is fired and
    // expands again two seconds later.  The Imperialist's own reticle
    // replaces it entirely and never animates, which is why the sniper
    // flag gates the timer rather than only the artwork.

    // What a pass asked the target-circle model to do.  The state machine
    // lives here; playing the animation is the renderer's half.
    struct ReticleAnimation {
        bool requested = false;
        int start = 0;
        int target = 0;
        int frames = 0;
    };

    // Which artwork the circle should be drawn from.  ResetReticle and the
    // weapon switch are the only two things that change it.
    enum class ReticleArt : std::uint8_t { TargetCircle, SniperCircle };

    // PlayerHud.HudOnFiredShot.  A fixed crosshair does not animate at all,
    // and neither does the scan visor, which has no reticle.
    ReticleAnimation hud_on_fired_shot(bool scan_visor,
                                       bool fixed_crosshair) noexcept;

    // PlayerHud.ResetReticle: back to the target circle, uncontracted.
    void reset_reticle() noexcept {
        reticle_art_ = ReticleArt::TargetCircle;
        small_reticle_ = false;
        small_reticle_timer_ = 0;
    }

    // PlayerHud.UpdateReticle's state half.  The cartridge's expansion is
    // stuck at full contraction for four frames, has one frame of starting
    // to open, and then jumps to fully open; the four-frame animation is
    // what produces that.
    ReticleAnimation update_reticle() noexcept;

    // PlayerHud.HudOnMorphStart, which puts the circle back to its first
    // frame without touching the artwork.
    void hud_on_morph_start() noexcept {
        small_reticle_ = false;
        small_reticle_timer_ = 0;
    }

    // BeamType.Imperialist, the one beam with a reticle of its own.
    static constexpr int ImperialistBeam = 4;

    // What a weapon switch asked for: the icon spins to the beam picked,
    // and the reticle artwork may have changed under it.
    struct WeaponSwitch {
        ReticleAnimation icon;
        bool art_changed = false;
    };

    // PlayerHud.HudOnWeaponSwitch.  Only the Imperialist has a reticle of
    // its own; every other beam goes back to the target circle.
    WeaponSwitch hud_on_weapon_switch(int beam, bool scan_visor) noexcept;

    // PlayerHud.HudOnZoom.
    ReticleAnimation hud_on_zoom(bool zoom) noexcept;

    // PlayerHud.HudOnDisrupted.  A cutscene running is the one thing that
    // suppresses it.
    void hud_on_disrupted(bool cutscene_running,
                          std::uint16_t duration) noexcept {
        if (!cutscene_running) {
            disrupted_state_ = 1;
            disrupted_timer_ = duration;
        }
    }

    // PlayerHud.GetCrosshairColor: green, amber, red.  The thresholds are
    // absolute health, not a fraction of the maximum, so a hunter with an
    // energy tank goes amber at the same reading Samus does.
    struct CrosshairColor {
        float red = 0.0F;
        float green = 0.0F;
        float blue = 0.0F;
    };
    [[nodiscard]] static constexpr CrosshairColor get_crosshair_color(
        int health) noexcept {
        if (health > 60) {
            return {0.0F, 1.0F, 0.0F};
        }
        if (health > 33) {
            return {1.0F, 0.65F, 0.0F};
        }
        return {1.0F, 0.0F, 0.0F};
    }

    [[nodiscard]] bool small_reticle() const noexcept {
        return small_reticle_;
    }
    [[nodiscard]] bool sniper_reticle() const noexcept {
        return sniper_reticle_;
    }
    [[nodiscard]] ReticleArt reticle_art() const noexcept {
        return reticle_art_;
    }
    [[nodiscard]] bool hud_zoom() const noexcept { return hud_zoom_; }

private:
    bool small_reticle_ = false;
    // The cartridge counts at half this rate, so sixty of its frames are
    // a hundred and twenty here.
    std::uint16_t small_reticle_timer_ = 0;
    bool sniper_reticle_ = false;
    bool hud_zoom_ = false;
    ReticleArt reticle_art_ = ReticleArt::TargetCircle;

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
