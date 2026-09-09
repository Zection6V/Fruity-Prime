#pragma once

#include "Metadata/metadata.hpp"

#include <array>
#include <cstddef>
#include <cstdint>
#include <string_view>

namespace fruityprime::players {

// Native counterpart of PlayerEntity.AvailableArray. The managed table has
// nine slots, while Set intentionally updates only the eight cartridge
// weapon bits; preserving that detail keeps weapon-selection state stable
// across the C# and native implementations.
class AvailableArray {
public:
    AvailableArray() noexcept = default;
    AvailableArray(const AvailableArray& source) noexcept {
        owned_ = source.values();
    }
    AvailableArray& operator=(const AvailableArray& source) noexcept {
        if (this != &source) values() = source.values();
        return *this;
    }

    // PlayerEntity's array is the same live inventory storage used by the
    // session. This avoids the previous one-way copy where writes through
    // AvailableWeapons silently disappeared.
    void bind(std::array<bool, 9>& values) noexcept { bound_ = &values; }
    void unbind() noexcept {
        owned_ = values();
        bound_ = nullptr;
    }

    void clear_all() noexcept { values().fill(false); }
    void set_all() noexcept { values().fill(true); }
    void set(std::uint16_t value) noexcept {
        for (std::size_t i = 0; i < 8; ++i) {
            values()[i] = (value & (1u << i)) != 0;
        }
    }
    void copy_from(const AvailableArray& source) noexcept {
        values() = source.values();
    }

    [[nodiscard]] bool operator[](std::size_t index) const noexcept {
        return values()[index];
    }
    bool& operator[](std::size_t index) noexcept { return values()[index]; }
    [[nodiscard]] bool operator[](formats::BeamType key) const noexcept {
        return (*this)[static_cast<std::size_t>(key)];
    }
    bool& operator[](formats::BeamType key) noexcept {
        return (*this)[static_cast<std::size_t>(key)];
    }

private:
    [[nodiscard]] std::array<bool, 9>& values() noexcept {
        return bound_ == nullptr ? owned_ : *bound_;
    }
    [[nodiscard]] const std::array<bool, 9>& values() const noexcept {
        return bound_ == nullptr ? owned_ : *bound_;
    }

    std::array<bool, 9> owned_{};
    std::array<bool, 9>* bound_ = nullptr;
};

// The managed PlayerValues table is authored as 12.4 fixed-point integers.
// Keep the raw values here instead of rounding them into a renderer-specific
// float table: collision, camera, HUD, and future player-AI ports all need the
// same source values.
struct Profile {
    metadata::Hunter hunter = metadata::Hunter::Samus;
    std::string_view name;

    std::int32_t walk_biped_traction = 0;
    std::int32_t strafe_biped_traction = 0;
    std::int32_t walk_speed_cap = 0;
    std::int32_t strafe_speed_cap = 0;
    std::int32_t alt_min_hspeed = 0;
    std::int32_t boost_speed_cap = 0;
    std::int32_t biped_gravity = 0;
    std::int32_t alt_air_gravity = 0;
    std::int32_t alt_ground_gravity = 0;
    std::int32_t jump_speed = 0;
    std::int32_t walk_speed_factor = 0;
    std::int32_t alt_ground_speed_factor = 0;
    std::int32_t strafe_speed_factor = 0;
    std::int32_t air_speed_factor = 0;
    std::int32_t stand_speed_factor = 0;
    std::int32_t roll_alt_traction = 0;
    std::int32_t alt_collision_radius = 0;
    std::int32_t alt_collision_y = 0;
    std::uint16_t boost_charge_min = 0;
    std::uint16_t boost_charge_max = 0;
    std::int32_t boost_speed_min = 0;
    std::int32_t boost_speed_max = 0;
    std::int32_t normal_fov = 0;
    std::int32_t zoom_sensitivity_factor = 0;
    std::int32_t biped_collision_radius = 0;
    std::int16_t damage_invulnerability = 0;
    std::uint16_t spawn_invulnerability = 0;
    std::int32_t gun_idle_time = 0;
    std::int16_t multiplayer_ammo_cap = 0;
    std::uint8_t ammo_recharge = 0;
    std::uint16_t energy_tank = 0;
    std::uint8_t alt_form_strafe = 0;
    std::int32_t fall_damage_max = 0;
    std::uint16_t alt_attack_damage = 0;
    std::int16_t alt_attack_cooldown = 0;

    [[nodiscard]] static float fixed_to_float(std::int32_t value) noexcept {
        return static_cast<float>(value) / 4096.0F;
    }
    [[nodiscard]] float walk_speed_scale() const noexcept {
        return static_cast<float>(walk_speed_factor) / 3604.0F;
    }
    [[nodiscard]] float alt_speed_scale() const noexcept {
        return static_cast<float>(alt_ground_speed_factor) / 3952.0F;
    }
};

constexpr std::size_t ProfileCount = metadata::HunterCount;

[[nodiscard]] const std::array<Profile, ProfileCount>& profiles() noexcept;
[[nodiscard]] const Profile& profile(std::uint8_t hunter) noexcept;

} // namespace fruityprime::players
