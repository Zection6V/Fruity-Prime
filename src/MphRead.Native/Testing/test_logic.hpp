#pragma once

#include "Formats/Types.hpp"

#include <array>
#include <cstdint>
#include <vector>

namespace fruityprime::testing::logic {

struct SaveBlock {
    std::int32_t field_0 = 0;
    std::int32_t field_4 = 0;
    std::int32_t field_8 = 0;
    std::int32_t field_c = 0;
};

struct SaveBufferBlock {
    std::int32_t field_0 = 0;
    std::int32_t field_4 = 0;
    std::int32_t field_8 = 0;
    std::int32_t field_c = 0;
    std::int32_t field_10 = 0;
    std::int32_t field_14 = 0;
    std::int32_t field_18 = 0;
    std::int32_t field_1c = 0;
};

[[nodiscard]] std::vector<SaveBufferBlock> test_save_blocks();

// This is the raw view used by TestLogic.GetCompletionValues.  It remains
// separate from game::StorySave because the test intentionally models the
// memory-class fields and their original widths.
struct StorySaveData {
    std::uint16_t weapons = 0;
    std::uint8_t found_octos = 0;
    std::uint32_t artifacts = 0;
    std::uint32_t max_scan_count = 0;
    std::int32_t scan_count = 0;
    std::uint16_t energy_cap = 0;
    std::array<std::uint16_t, 2> ammo_caps{};
};

struct CompletionValues {
    std::int32_t completion = 0;
    std::int32_t octolith = 0;
    std::int32_t energy_tanks = 0;
    std::int32_t ua_expansions = 0;
    std::int32_t missile_expansions = 0;
};

[[nodiscard]] CompletionValues get_completion_values(
    const StorySaveData& save) noexcept;
[[nodiscard]] std::int32_t get_completion_percentage(
    const StorySaveData& save) noexcept;

enum class Logic1Branch {
    Noxus,
    Kanden,
    OtherHunter,
    SamusSpeedVector,
    SamusPreviousPosition,
    SpireSpeedVector
};

[[nodiscard]] Logic1Branch test_logic_1(
    formats::Hunter hunter = formats::Hunter::Samus,
    std::uint32_t flags = 0) noexcept;

enum class SomeFlags : std::uint32_t {
    None = 0x0,
    SurfaceCollision = 0x10,
    PlatformCollision = 0x80,
    UsedJump = 0x100,
    AltForm = 0x200,
    DrawAltForm = 0x400,
    BlockAiming = 0x1000000,
    WeaponMenu = 0x2000000,
    DrawGunSmoke = 0x80000000U
};

enum class MoreFlags : std::uint32_t {
    None = 0x0,
    FullCharge = 0x1,
    HideModel = 0x2,
    WeaponFiring = 0x4,
    AltFormAttack = 0x8
};

// TestLogic2 is a decompiler probe rather than the production renderer.  The
// trace preserves its branch and draw decisions while leaving actual model
// submission to the native renderer.
struct PlayerDrawState {
    formats::Vector3 position{};
    formats::Hunter hunter = formats::Hunter::Samus;
    std::uint32_t some_flags = 0;
    std::uint32_t more_flags = 0;
    bool visible = true;
    std::uint8_t field_4bb = 0;
    std::uint32_t health = 0;
    std::int32_t field_358 = 0;
    std::int32_t field_6d0 = 0;
    formats::Vector3 field_64{};
    formats::Vector3 field_b4{};
    std::int16_t field_e2 = 0;
    std::uint8_t field_4d6 = 0;
    std::int32_t field_550 = 0;
    std::int32_t field_46c = 0;
};

struct DrawTrace {
    bool hidden_by_flag = false;
    bool animation_initialized = false;
    bool drew_alt_model = false;
    bool drew_body_model = false;
    bool drew_gun = false;
    bool drew_gun_smoke = false;
    bool drew_attachment = false;
    int draw_count = 0;
};

[[nodiscard]] DrawTrace test_logic_2(
    const PlayerDrawState& player, int player_id) noexcept;

} // namespace fruityprime::testing::logic
