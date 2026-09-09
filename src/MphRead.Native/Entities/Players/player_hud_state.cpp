#include "Entities/Players/player_hud_state.hpp"

#include <algorithm>
#include <cmath>

namespace fruityprime::players {

float PlayerHudState::whiteout_position(float global_elapsed) const noexcept {
    // The managed code works in frames, not seconds.
    const float time = (global_elapsed - whiteout_time_) * 60.0F;
    const float time_squared = time * time;
    const float time_cubed = time_squared * time;
    constexpr float Jerk = 0.004F;
    constexpr float InitialAcceleration = 0.0F;
    constexpr float InitialVelocity = 1.0F;
    constexpr float InitialPosition = 0.0F;
    return InitialPosition + InitialVelocity * time
        + 0.5F * InitialAcceleration * time_squared
        + (1.0F / 6.0F) * Jerk * time_cubed;
}

void PlayerHudState::update_whiteout_table(float value) noexcept {
    const int truncated = static_cast<int>(value);
    float amount = 0.9975F;
    for (int i = 95; i >= 0; --i) {
        const int index = i - truncated;
        if (index >= 0 && index <= 95) {
            const float factor =
                (std::pow(amount, 8.0F) * 32.0F - 16.0F) / 16.0F;
            whiteout_table_[static_cast<std::size_t>(index)] = factor;
            whiteout_table_[static_cast<std::size_t>(191 - index)] = factor;
        }
        amount -= 0.0105F;
    }
    // Everything the band has already swept past is fully lit.
    for (int j = 95; j >= 95 - truncated && j >= 0; --j) {
        whiteout_table_[static_cast<std::size_t>(j)] = 16.0F;
        whiteout_table_[static_cast<std::size_t>(191 - j)] = 16.0F;
    }
}

void PlayerHudState::update_whiteout_state(float global_elapsed,
                                           float frame_time) noexcept {
    if (whiteout_state_ == 0) {
        whiteout_factor_ += 2.8125F * frame_time;
        if (whiteout_factor_ >= 1.0F) {
            whiteout_factor_ = 1.0F;
            whiteout_state_ = 1;
        }
        whiteout_amount_ = whiteout_position(global_elapsed);
    } else if (whiteout_state_ == 1) {
        whiteout_amount_ = whiteout_position(global_elapsed);
        if (whiteout_amount_ >= 96.0F) {
            whiteout_amount_ = 96.0F;
            whiteout_state_ = 2;
            whiteout_time_ = global_elapsed;
        }
        update_whiteout_table(whiteout_amount_);
    } else if (whiteout_state_ == 2) {
        // A factor of -1 tells the renderer to use the table value directly
        // rather than as a multiplier.
        whiteout_factor_ = -1.0F;
        const float time = global_elapsed - whiteout_time_;
        const float value = 1.0F - std::min(time / (16.0F / 30.0F), 1.0F);
        whiteout_table_.fill(value);
    }
}

void PlayerHudState::update_disrupted_state() noexcept {
    if (disrupted_state_ == 1) {
        disruption_factor_ += 0.25F / 2.0F;
        if (disruption_factor_ >= 1.0F) {
            disruption_factor_ = 1.0F;
            disrupted_state_ = 2;
        }
    } else if (disrupted_state_ == 2) {
        if (--disrupted_timer_ == 0) {
            disrupted_state_ = 3;
        }
    } else if (disrupted_state_ == 3) {
        disruption_factor_ -= 0.125F / 2.0F;
        if (disruption_factor_ <= 0.0F) {
            disruption_factor_ = 0.0F;
            disrupted_state_ = 0;
            // The cooldown before another disruption can start.
            disrupted_timer_ = 32 * 2;
        }
    } else if (disrupted_state_ != 0) {
        if (--disrupted_timer_ == 0) {
            disrupted_state_ = 1;
        }
    }
}

PlayerHudState::ReticleAnimation PlayerHudState::hud_on_fired_shot(
    const bool scan_visor, const bool fixed_crosshair) noexcept {
    ReticleAnimation animation;
    if (scan_visor || fixed_crosshair) {
        return animation;
    }
    if (!small_reticle_ && !sniper_reticle_) {
        small_reticle_ = true;
        animation = {true, 0, 3, 4};
    }
    small_reticle_timer_ = 60 * 2;
    return animation;
}

PlayerHudState::ReticleAnimation PlayerHudState::update_reticle() noexcept {
    ReticleAnimation animation;
    if (small_reticle_timer_ > 0 && !sniper_reticle_) {
        --small_reticle_timer_;
        if (small_reticle_timer_ == 0 && small_reticle_) {
            animation = {true, 3, 0, 4};
            small_reticle_ = false;
        }
    }
    return animation;
}

PlayerHudState::WeaponSwitch PlayerHudState::hud_on_weapon_switch(
    const int beam, const bool scan_visor) noexcept {
    const ReticleArt before = reticle_art_;
    if (beam != ImperialistBeam || sniper_reticle_) {
        sniper_reticle_ = false;
        reset_reticle();
    } else {
        sniper_reticle_ = true;
        if (!scan_visor) {
            reticle_art_ = ReticleArt::SniperCircle;
        }
    }
    // The weapon icon spins through nineteen frames to the beam picked.
    return {ReticleAnimation{true, 9, 27, 19}, reticle_art_ != before};
}

PlayerHudState::ReticleAnimation PlayerHudState::hud_on_zoom(
    const bool zoom) noexcept {
    ReticleAnimation animation;
    if (hud_zoom_ != zoom) {
        hud_zoom_ = zoom;
        animation = hud_zoom_ ? ReticleAnimation{true, 0, 2, 2}
                              : ReticleAnimation{true, 2, 0, 2};
    }
    return animation;
}

int PlayerHudState::double_damage_icon_frame() const noexcept {
    // Each speed has its own period and its own share of it lit: a third of
    // a second at speed one, a sixth at speed three.  The pickup running
    // out is a thing a player has to be able to see coming.
    struct Pulse {
        float period;
        float lit;
    };
    Pulse pulse;
    switch (double_damage_speed_) {
    case 1: pulse = {35.0F / 30.0F, 30.0F / 30.0F}; break;
    case 2: pulse = {25.0F / 30.0F, 20.0F / 30.0F}; break;
    case 3: pulse = {10.0F / 30.0F, 5.0F / 30.0F}; break;
    default: return 0;
    }
    float past = std::fmod(double_damage_icon_timer_, pulse.period);
    if (past < 0.0F) {
        past += pulse.period;
    }
    return past >= pulse.lit ? 1 : 0;
}

} // namespace fruityprime::players
