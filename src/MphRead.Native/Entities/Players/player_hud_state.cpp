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

int PlayerHudState::weapon_wheel_selection(
    const float pointer_x, const float pointer_y, const float width,
    const float height, const std::span<const bool, 6> available) noexcept {
    if (width <= 0.0F || height <= 0.0F) {
        return -1;
    }
    // The wheel is laid out in the DS's own 256x192 and stretched with the
    // window, so the pointer offset is measured in that space too.
    const float ratio_x = width / 256.0F;
    const float ratio_y = height / 192.0F;
    // Left of and below the hub, which sits near the top right corner.
    const float distance_x = 224.0F * ratio_x - pointer_x;
    const float distance_y = pointer_y - 38.0F * ratio_y;
    const float dead_zone = 20.0F * ratio_y;
    if (distance_x <= 0.0F || distance_y <= 0.0F
        || distance_x * distance_x + distance_y * distance_y
            <= dead_zone * dead_zone) {
        // Inside the hub, or outside the quadrant the wheel occupies.
        return -1;
    }
    // The five boundaries between the six wedges, as the cartridge's own
    // fixed-point sines and cosines.  Each pair is one boundary's slope.
    struct Boundary {
        float run;
        float rise;
    };
    static constexpr std::array<Boundary, 5> boundaries{{
        {1060.0F, 3956.0F},
        {2048.0F, 3547.0F},
        {2896.0F, 2896.0F},
        {3547.0F, 2048.0F},
        {3956.0F, 1060.0F},
    }};
    const float slope = distance_x / distance_y;
    // Walking outwards: the first boundary the pointer is past decides the
    // wedge, and a beam the player does not have is simply not selectable
    // rather than selecting the one beside it.
    int wedge = -1;
    for (std::size_t i = 0; i < boundaries.size(); ++i) {
        const float bound = boundaries[i].run * ratio_x
            / (boundaries[i].rise * ratio_y);
        if (slope < bound) {
            wedge = static_cast<int>(i);
            break;
        }
    }
    if (wedge < 0) {
        wedge = static_cast<int>(boundaries.size());
    }
    const auto index = static_cast<std::size_t>(wedge);
    return index < available.size() && available[index] ? wedge : -1;
}

void PlayerHudState::update_healthbars(const BarFrame& frame) noexcept {
    if (frame.health < 25) {
        // Checked before either flash, so being hit while nearly dead does
        // not recolour the bar to something gentler.
        if (!healthbar_changed_color_) {
            healthbar_palette_ = 2;
            healthbar_changed_color_ = true;
        }
    } else if (frame.since_heal < 10U * 2U) {
        if (!healthbar_changed_color_) {
            healthbar_palette_ = 1;
            healthbar_changed_color_ = true;
        }
    } else if (frame.since_damage < 6U * 2U) {
        if (!healthbar_changed_color_) {
            healthbar_palette_ = 2;
            healthbar_changed_color_ = true;
        }
    } else if (healthbar_changed_color_) {
        healthbar_palette_ = 0;
        healthbar_changed_color_ = false;
    }
    // The bar slides to the helmet's own resting offset, half a unit a
    // frame, and further still as a ball.
    float target = frame.health_offset_y;
    if (frame.alt_form || frame.morphing) {
        target += frame.health_offset_y_alt;
    }
    if (healthbar_y_offset_ > target) {
        healthbar_y_offset_ -= 0.5F;
    } else if (healthbar_y_offset_ < target) {
        healthbar_y_offset_ += 0.5F;
    }
}

} // namespace fruityprime::players
