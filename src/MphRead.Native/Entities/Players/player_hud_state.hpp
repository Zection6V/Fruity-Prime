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
#include <span>
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
    [[nodiscard]] const std::array<float, WhiteoutTableSize>&
    hud_whiteout_table() const noexcept {
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

    // ---- the pickups the HUD announces ------------------------------
    // Double damage and cloak each put an icon in a corner and spell their
    // name out a character at a time under it.  The icon pulses; how fast
    // is the timer running out, which is what makes it a warning.

    // PlayerHud.UpdateDoubleDamageSpeed.  Speed 1 is when it is picked up,
    // which is the only time the name is spelled out.
    void update_double_damage_speed(int speed) noexcept {
        double_damage_speed_ = speed;
        double_damage_icon_timer_ = 0.0F;
        if (speed == 1) {
            double_damage_text_timer_ = 60.0F / 30.0F;
        }
    }

    // PlayerHud.ProcessDoubleDamageHud.
    void process_double_damage_hud(float remaining,
                                   float frame_time) noexcept {
        if (remaining <= 0.0F) {
            return;
        }
        if (double_damage_text_timer_ > 0.0F) {
            double_damage_text_timer_ -= frame_time;
        }
        double_damage_icon_timer_ += frame_time;
    }

    // Which of the icon's two frames is showing.  The pulse gets shorter as
    // the speed goes up, so a player can see the pickup running out.
    [[nodiscard]] int double_damage_icon_frame() const noexcept;

    [[nodiscard]] float double_damage_text_timer() const noexcept {
        return double_damage_text_timer_;
    }
    [[nodiscard]] int double_damage_speed() const noexcept {
        return double_damage_speed_;
    }

    // PlayerHud.ProcessCloakHud.
    void process_cloak_hud(bool cloaking, float frame_time) noexcept {
        if (!cloaking) {
            hud_cloaking_ = false;
            return;
        }
        if (!hud_cloaking_) {
            hud_cloaking_ = true;
            cloak_text_timer_ = 45.0F / 30.0F;
        }
        if (cloak_text_timer_ > 0.0F) {
            cloak_text_timer_ -= frame_time;
        }
    }

    [[nodiscard]] float cloak_text_timer() const noexcept {
        return cloak_text_timer_;
    }
    [[nodiscard]] bool hud_cloaking() const noexcept { return hud_cloaking_; }

    // ---- the opponent readout ---------------------------------------
    // Hitting somebody, or being hit by them, puts their name, portrait and
    // energy on the bottom of the screen for two seconds.

    // PlayerHud.UpdateOpponent.
    void update_opponent(int slot, int own_slot, bool multiplayer) noexcept {
        if (multiplayer && slot != own_slot) {
            opponent_healthbar_timer_ = 60.0F / 30.0F;
            opponent_index_ = slot;
        }
    }

    // PlayerHud.ProcessOpponent.
    void process_opponent(float frame_time) noexcept {
        if (opponent_index_ == -1 || opponent_healthbar_timer_ <= 0.0F) {
            return;
        }
        opponent_healthbar_timer_ -= frame_time;
        if (opponent_healthbar_timer_ <= 0.0F) {
            opponent_healthbar_timer_ = 0.0F;
            opponent_index_ = -1;
        }
    }

    [[nodiscard]] int opponent_index() const noexcept {
        return opponent_index_;
    }
    [[nodiscard]] float opponent_healthbar_timer() const noexcept {
        return opponent_healthbar_timer_;
    }

    // ---- the weapon wheel -------------------------------------------
    // Six wedges around a point near the top right of the touch screen.
    // Which one the pointer is in is decided by the slope of the offset to
    // it, against five fixed-point ratios the cartridge carries -- so the
    // wedges are equal in angle and not in area.

    // PlayerHud.UpdateWeaponSelect's geometry.  Returns the wedge under the
    // pointer, 0 to 5, or -1 for none; `available` says which beams the
    // player actually has, in wheel order.
    [[nodiscard]] static int weapon_wheel_selection(
        float pointer_x, float pointer_y, float width, float height,
        std::span<const bool, 6> available) noexcept;

    struct WeaponSelect {
        int selection = -1;
        // The wheel clicks when the selection changes, not every frame.
        bool clicked = false;
    };

    // PlayerHud.UpdateWeaponSelect.
    [[nodiscard]] WeaponSelect update_weapon_select(
        float pointer_x, float pointer_y, float width, float height,
        std::span<const bool, 6> available) noexcept {
        const int selection = weapon_wheel_selection(
            pointer_x, pointer_y, width, height, available);
        return {selection, note_weapon_selection(selection)};
    }

    // Whether the wheel selection changed this pass, which is what makes
    // the click.
    [[nodiscard]] bool note_weapon_selection(int selection) noexcept {
        if (selection == previous_weapon_selection_) {
            return false;
        }
        previous_weapon_selection_ = selection;
        return true;
    }

    // ---- the boost bomb readout -------------------------------------
    // PlayerHud.UpdateBoostBombs: the bomb row slides up into view as the
    // player morphs and back off the bottom as they stand up.
    void update_boost_bombs(bool alt_form, bool morphing) noexcept {
        const float target = alt_form || morphing ? 160.0F : 208.0F;
        if (boost_bombs_y_offset_ > target) {
            boost_bombs_y_offset_ -= 1.0F;
        } else if (boost_bombs_y_offset_ < target) {
            boost_bombs_y_offset_ += 1.0F;
        }
    }
    [[nodiscard]] float boost_bombs_y_offset() const noexcept {
        return boost_bombs_y_offset_;
    }

    // ---- the damage arrows ------------------------------------------
    // Eight directions; each blinks for as long as its timer runs.
    static constexpr std::size_t DamageIndicatorCount = 8;

    // PlayerHud.UpdateDamageIndicators.
    void update_damage_indicators() noexcept {
        for (std::size_t i = 0; i < DamageIndicatorCount; ++i) {
            if (damage_indicator_timers_[i] > 0) {
                --damage_indicator_timers_[i];
            }
            // Four frames on, four off, which is what makes it read as a
            // warning rather than as part of the helmet.
            damage_indicator_shown_[i] =
                (damage_indicator_timers_[i] & (4u * 2u)) != 0;
        }
    }
    void set_damage_indicator(std::size_t direction,
                              std::uint16_t frames) noexcept {
        if (direction < DamageIndicatorCount) {
            damage_indicator_timers_[direction] = frames;
        }
    }
    [[nodiscard]] bool damage_indicator_shown(
        std::size_t direction) const noexcept {
        return direction < DamageIndicatorCount
            && damage_indicator_shown_[direction];
    }

    // ---- the two bars' colours ---------------------------------------
    // Both bars flash to say something happened, and the flag is what stops
    // them flashing again every frame while the reason still holds.

    // What UpdateHealthbars and UpdateAmmoBar are told about the frame.
    struct BarFrame {
        int health = 0;
        // Frames since the last heal, hit and ammo pickup, in this head's
        // own frames -- the cartridge counts at half the rate.
        std::uint32_t since_heal = 0xffffffffU;
        std::uint32_t since_damage = 0xffffffffU;
        std::uint32_t since_pickup = 0xffffffffU;
        bool alt_form = false;
        bool morphing = false;
        // HudObjects.HealthOffsetY and HealthOffsetYAlt.
        float health_offset_y = 0.0F;
        float health_offset_y_alt = 0.0F;
    };

    // PlayerHud.UpdateHealthbars.  Low health and a hit both turn the bar
    // red; a heal turns it green.  Low health is checked first, so being
    // hit while nearly dead does not briefly recolour it to something
    // gentler.
    void update_healthbars(const BarFrame& frame) noexcept;

    // PlayerHud.UpdateAmmoBar.
    void update_ammo_bar(const BarFrame& frame) noexcept {
        if (frame.since_pickup < 10U * 2U) {
            if (!ammo_bar_changed_color_) {
                ammo_bar_palette_ = 1;
                ammo_bar_changed_color_ = true;
            }
        } else if (ammo_bar_changed_color_) {
            ammo_bar_palette_ = 0;
            ammo_bar_changed_color_ = false;
        }
    }

    [[nodiscard]] int healthbar_palette() const noexcept {
        return healthbar_palette_;
    }
    [[nodiscard]] int ammo_bar_palette() const noexcept {
        return ammo_bar_palette_;
    }
    [[nodiscard]] float healthbar_y_offset() const noexcept {
        return healthbar_y_offset_;
    }

    // PlayerHud.HudAspectFix.  The HUD is laid out in the DS's 256x192 and
    // stretched to the window, so anything that has to stay square -- an
    // icon, a circle -- is scaled back by this.
    [[nodiscard]] static constexpr float hud_aspect_fix(
        float width, float height) noexcept {
        if (width <= 0.0F || height <= 0.0F) {
            return 1.0F;
        }
        return height / 192.0F * (256.0F / width);
    }

private:
    bool small_reticle_ = false;
    // The cartridge counts at half this rate, so sixty of its frames are
    // a hundred and twenty here.
    std::uint16_t small_reticle_timer_ = 0;
    bool sniper_reticle_ = false;
    bool hud_zoom_ = false;
    ReticleArt reticle_art_ = ReticleArt::TargetCircle;
    int double_damage_speed_ = 0;
    float double_damage_icon_timer_ = 0.0F;
    float double_damage_text_timer_ = 0.0F;
    bool hud_cloaking_ = false;
    float cloak_text_timer_ = 0.0F;
    float opponent_healthbar_timer_ = 0.0F;
    int opponent_index_ = -1;
    int previous_weapon_selection_ = -1;
    float boost_bombs_y_offset_ = 208.0F;
    std::array<std::uint16_t, DamageIndicatorCount>
        damage_indicator_timers_{};
    std::array<bool, DamageIndicatorCount> damage_indicator_shown_{};
    int healthbar_palette_ = 0;
    bool healthbar_changed_color_ = false;
    float healthbar_y_offset_ = 0.0F;
    int ammo_bar_palette_ = 0;
    bool ammo_bar_changed_color_ = false;

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
