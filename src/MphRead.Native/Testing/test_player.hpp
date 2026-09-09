#pragma once

#include "Formats/Types.hpp"

#include <array>
#include <cstddef>
#include <cstdint>
#include <vector>

namespace fruityprime::testing::player {

struct ButtonControl {
    formats::ButtonFlags button = formats::ButtonFlags::None;
    formats::PressFlags flags = formats::PressFlags::None;
};

struct PlayerControls {
    static constexpr std::size_t Size = 0x9c;

    std::uint32_t flags = 0;
    ButtonControl left;
    ButtonControl right;
    ButtonControl up;
    ButtonControl down;
    ButtonControl field_14;
    ButtonControl field_18;
    ButtonControl field_1c;
    ButtonControl field_20;
    ButtonControl field_24;
    ButtonControl field_28;
    ButtonControl field_2c;
    ButtonControl field_30;
    ButtonControl shoot;
    ButtonControl jump;
    ButtonControl morph;
    ButtonControl field_40;
    ButtonControl field_44;
    ButtonControl bomb;
    ButtonControl unused_4c;
    ButtonControl boost_charge;
    ButtonControl unused_54;
    ButtonControl unused_58;
    ButtonControl unused_5c;
    ButtonControl unused_60;
    ButtonControl noxus_alt_attack;
    ButtonControl unused_68;
    ButtonControl spire_alt_attack;
    ButtonControl trace_alt_attack;
    ButtonControl unused_74;
    ButtonControl weavel_alt_attack;
    ButtonControl zoom;
    ButtonControl respawn;
    ButtonControl unused_84;
    std::int32_t field_88 = 0;
    std::int32_t field_8c = 0;
    std::int32_t field_90 = 0;
    std::int32_t field_94 = 0;
    std::int32_t field_98 = 0;
};

struct PlayerValues {
    static constexpr std::size_t Size = 0x168;

    std::array<std::uint8_t, Size> bytes{};

    [[nodiscard]] std::uint8_t read_u8(std::size_t offset) const noexcept {
        return offset < bytes.size() ? bytes[offset] : 0;
    }

    [[nodiscard]] std::uint16_t read_u16(std::size_t offset) const noexcept {
        if (offset > bytes.size() || bytes.size() - offset < 2) {
            return 0;
        }
        return static_cast<std::uint16_t>(
            static_cast<std::uint16_t>(bytes[offset])
            | (static_cast<std::uint16_t>(bytes[offset + 1]) << 8));
    }

    [[nodiscard]] std::uint32_t read_u32(std::size_t offset) const noexcept {
        if (offset > bytes.size() || bytes.size() - offset < 4) {
            return 0;
        }
        return static_cast<std::uint32_t>(bytes[offset])
             | (static_cast<std::uint32_t>(bytes[offset + 1]) << 8)
             | (static_cast<std::uint32_t>(bytes[offset + 2]) << 16)
             | (static_cast<std::uint32_t>(bytes[offset + 3]) << 24);
    }

    [[nodiscard]] std::int32_t read_i32(std::size_t offset) const noexcept {
        return static_cast<std::int32_t>(read_u32(offset));
    }

    [[nodiscard]] formats::Fixed fixed(std::size_t offset) const noexcept {
        return {read_i32(offset)};
    }

    [[nodiscard]] formats::Fixed biped_traction_lr() const noexcept { return fixed(0x000); }
    [[nodiscard]] formats::Fixed biped_traction_fb() const noexcept { return fixed(0x004); }
    [[nodiscard]] formats::Fixed walk_h_speed_cap() const noexcept { return fixed(0x008); }
    [[nodiscard]] formats::Fixed strafe_h_speed_cap() const noexcept { return fixed(0x00c); }
    [[nodiscard]] formats::Fixed min_alt_h_speed() const noexcept { return fixed(0x010); }
    [[nodiscard]] formats::Fixed boost_h_speed_cap() const noexcept { return fixed(0x014); }
    [[nodiscard]] formats::Fixed biped_gravity() const noexcept { return fixed(0x018); }
    [[nodiscard]] formats::Fixed alt_gravity_air() const noexcept { return fixed(0x01c); }
    [[nodiscard]] formats::Fixed alt_gravity_ground() const noexcept { return fixed(0x020); }
    [[nodiscard]] formats::Fixed jump_speed() const noexcept { return fixed(0x024); }
    [[nodiscard]] formats::Fixed walk_speed_factor() const noexcept { return fixed(0x028); }
    [[nodiscard]] formats::Fixed alt_ground_speed_factor() const noexcept { return fixed(0x02c); }
    [[nodiscard]] formats::Fixed strafe_speed_factor() const noexcept { return fixed(0x030); }
    [[nodiscard]] formats::Fixed air_speed_factor() const noexcept { return fixed(0x034); }
    [[nodiscard]] formats::Fixed stand_speed_factor() const noexcept { return fixed(0x038); }
    [[nodiscard]] formats::Fixed field_3c() const noexcept { return fixed(0x03c); }
    [[nodiscard]] formats::Fixed alt_collision_radius() const noexcept { return fixed(0x040); }
    [[nodiscard]] formats::Fixed alt_collision_y() const noexcept { return fixed(0x044); }
    [[nodiscard]] std::uint16_t boost_charge_min() const noexcept { return read_u16(0x048); }
    [[nodiscard]] std::uint16_t boost_charge_max() const noexcept { return read_u16(0x04a); }
    [[nodiscard]] formats::Fixed boost_speed_min() const noexcept { return fixed(0x04c); }
    [[nodiscard]] formats::Fixed boost_speed_max() const noexcept { return fixed(0x050); }
    [[nodiscard]] formats::Fixed alt_speed_cap_h_inc() const noexcept { return fixed(0x054); }
    [[nodiscard]] formats::Fixed field_58() const noexcept { return fixed(0x058); }
    [[nodiscard]] formats::Fixed field_5c() const noexcept { return fixed(0x05c); }
    [[nodiscard]] formats::Fixed walk_view_bob() const noexcept { return fixed(0x060); }
    [[nodiscard]] std::uint32_t field_64() const noexcept { return read_u32(0x064); }
    [[nodiscard]] std::uint16_t field_68() const noexcept { return read_u16(0x068); }
    [[nodiscard]] std::uint16_t padding_6a() const noexcept { return read_u16(0x06a); }
    [[nodiscard]] formats::Fixed regular_fov() const noexcept { return fixed(0x06c); }
    [[nodiscard]] std::uint32_t field_70() const noexcept { return read_u32(0x070); }
    [[nodiscard]] std::uint32_t field_74() const noexcept { return read_u32(0x074); }
    [[nodiscard]] std::uint32_t field_78() const noexcept { return read_u32(0x078); }
    [[nodiscard]] std::uint32_t field_7c() const noexcept { return read_u32(0x07c); }
    [[nodiscard]] std::uint32_t field_80() const noexcept { return read_u32(0x080); }
    [[nodiscard]] std::uint32_t field_84() const noexcept { return read_u32(0x084); }
    [[nodiscard]] std::uint32_t field_88() const noexcept { return read_u32(0x088); }
    [[nodiscard]] std::uint32_t field_8c() const noexcept { return read_u32(0x08c); }
    [[nodiscard]] std::uint32_t field_90() const noexcept { return read_u32(0x090); }
    [[nodiscard]] formats::Fixed min_collision_height() const noexcept { return fixed(0x094); }
    [[nodiscard]] formats::Fixed max_collision_height() const noexcept { return fixed(0x098); }
    [[nodiscard]] formats::Fixed biped_collision_radius() const noexcept { return fixed(0x09c); }
    [[nodiscard]] formats::Fixed field_a0() const noexcept { return fixed(0x0a0); }
    [[nodiscard]] formats::Fixed field_a4() const noexcept { return fixed(0x0a4); }
    [[nodiscard]] formats::Fixed field_a8() const noexcept { return fixed(0x0a8); }
    [[nodiscard]] std::uint16_t damage_invuln() const noexcept { return read_u16(0x0ac); }
    [[nodiscard]] std::uint16_t damage_flash_duration() const noexcept { return read_u16(0x0ae); }
    [[nodiscard]] formats::Fixed field_b0() const noexcept { return fixed(0x0b0); }
    [[nodiscard]] formats::Fixed field_b4() const noexcept { return fixed(0x0b4); }
    [[nodiscard]] formats::Fixed field_b8() const noexcept { return fixed(0x0b8); }
    [[nodiscard]] formats::Fixed smoke_z_offset() const noexcept { return fixed(0x0bc); }
    [[nodiscard]] std::uint32_t bomb_cooldown() const noexcept { return read_u32(0x0c0); }
    [[nodiscard]] formats::Fixed bomb_self_radius() const noexcept { return fixed(0x0c4); }
    [[nodiscard]] formats::Fixed bomb_self_radius_squared() const noexcept { return fixed(0x0c8); }
    [[nodiscard]] formats::Fixed bomb_radius() const noexcept { return fixed(0x0cc); }
    [[nodiscard]] formats::Fixed bomb_radius_squared() const noexcept { return fixed(0x0d0); }
    [[nodiscard]] formats::Fixed bomb_jump_speed() const noexcept { return fixed(0x0d4); }
    [[nodiscard]] std::uint32_t bomb_refill_time() const noexcept { return read_u32(0x0d8); }
    [[nodiscard]] std::uint16_t bomb_damage() const noexcept { return read_u16(0x0dc); }
    [[nodiscard]] std::uint16_t bomb_enemy_damage() const noexcept { return read_u16(0x0de); }
    [[nodiscard]] std::uint16_t field_e0() const noexcept { return read_u16(0x0e0); }
    [[nodiscard]] std::uint16_t spawn_invuln() const noexcept { return read_u16(0x0e2); }
    [[nodiscard]] std::uint16_t field_e4() const noexcept { return read_u16(0x0e4); }
    [[nodiscard]] std::uint16_t padding_e6() const noexcept { return read_u16(0x0e6); }
    [[nodiscard]] std::uint32_t field_e8() const noexcept { return read_u32(0x0e8); }
    [[nodiscard]] std::uint32_t field_ec() const noexcept { return read_u32(0x0ec); }
    [[nodiscard]] std::uint32_t view_sway_start_time() const noexcept { return read_u32(0x0f0); }
    [[nodiscard]] std::uint32_t sway_time_increment() const noexcept { return read_u32(0x0f4); }
    [[nodiscard]] std::uint32_t sway_limit() const noexcept { return read_u32(0x0f8); }
    [[nodiscard]] std::uint32_t field_fc() const noexcept { return read_u32(0x0fc); }
    [[nodiscard]] std::uint16_t mp_ammo_cap() const noexcept { return read_u16(0x100); }
    [[nodiscard]] std::uint8_t ammo_recharge() const noexcept { return read_u8(0x102); }
    [[nodiscard]] std::uint8_t padding_103() const noexcept { return read_u8(0x103); }
    [[nodiscard]] std::uint16_t energy_start() const noexcept { return read_u16(0x104); }
    [[nodiscard]] std::uint16_t field_106() const noexcept { return read_u16(0x106); }
    [[nodiscard]] std::uint8_t alt_ground_no_gravity() const noexcept { return read_u8(0x108); }
    [[nodiscard]] std::uint8_t padding_109() const noexcept { return read_u8(0x109); }
    [[nodiscard]] std::uint16_t padding_10a() const noexcept { return read_u16(0x10a); }
    [[nodiscard]] formats::Fixed fall_damage_speed() const noexcept { return fixed(0x10c); }
    [[nodiscard]] std::uint32_t fall_damage_max() const noexcept { return read_u32(0x110); }
    [[nodiscard]] std::uint32_t field_114() const noexcept { return read_u32(0x114); }
    [[nodiscard]] std::uint32_t field_118() const noexcept { return read_u32(0x118); }
    [[nodiscard]] formats::Fixed jump_pad_slide_factor() const noexcept { return fixed(0x11c); }
    [[nodiscard]] std::uint32_t field_120() const noexcept { return read_u32(0x120); }
    [[nodiscard]] std::uint32_t field_124() const noexcept { return read_u32(0x124); }
    [[nodiscard]] std::uint32_t field_128() const noexcept { return read_u32(0x128); }
    [[nodiscard]] formats::Fixed alt_spin_speed() const noexcept { return fixed(0x12c); }
    [[nodiscard]] std::uint32_t field_130() const noexcept { return read_u32(0x130); }
    [[nodiscard]] std::uint32_t field_134() const noexcept { return read_u32(0x134); }
    [[nodiscard]] std::uint32_t field_138() const noexcept { return read_u32(0x138); }
    [[nodiscard]] std::uint32_t field_13c() const noexcept { return read_u32(0x13c); }
    [[nodiscard]] formats::Fixed field_140() const noexcept { return fixed(0x140); }
    [[nodiscard]] formats::Fixed field_144() const noexcept { return fixed(0x144); }
    [[nodiscard]] formats::Fixed field_148() const noexcept { return fixed(0x148); }
    [[nodiscard]] std::uint32_t field_14c() const noexcept { return read_u32(0x14c); }
    [[nodiscard]] std::uint16_t field_150() const noexcept { return read_u16(0x150); }
    [[nodiscard]] std::uint16_t noxus_alt_attack_startup() const noexcept { return read_u16(0x152); }
    [[nodiscard]] std::uint32_t field_154() const noexcept { return read_u32(0x154); }
    [[nodiscard]] std::uint32_t field_158() const noexcept { return read_u32(0x158); }
    [[nodiscard]] formats::Fixed lunge_h_speed() const noexcept { return fixed(0x15c); }
    [[nodiscard]] formats::Fixed lunge_v_speed() const noexcept { return fixed(0x160); }
    [[nodiscard]] std::uint16_t alt_attack_damage() const noexcept { return read_u16(0x164); }
    [[nodiscard]] std::uint16_t alt_attack_cooldown() const noexcept { return read_u16(0x166); }
};

[[nodiscard]] std::vector<PlayerControls> get_player_controls();
[[nodiscard]] std::vector<PlayerValues> get_player_values();

} // namespace fruityprime::testing::player

