#include "Testing/test_logic.hpp"

#include <algorithm>
#include <array>

namespace fruityprime::testing::logic {
namespace {

[[nodiscard]] bool has_flag(std::uint32_t value,
                            SomeFlags flag) noexcept {
    return (value & static_cast<std::uint32_t>(flag)) != 0;
}

[[nodiscard]] bool has_flag(std::uint32_t value,
                            MoreFlags flag) noexcept {
    return (value & static_cast<std::uint32_t>(flag)) != 0;
}

} // namespace

std::vector<SaveBufferBlock> test_save_blocks() {
    std::vector<SaveBlock> blocks;
    for (int index = 0; index < 3; ++index) {
        blocks.push_back({0, 128, 4276, -23});
    }
    blocks.push_back({0, 128, 64, 2});
    blocks.push_back({0, 128, 164, -1});
    blocks.push_back({0, 128, 488, 9});
    blocks.push_back({0, 128, 8400, 0});

    constexpr std::int32_t save_buffer_size = 0x40000;
    constexpr std::int32_t block_alignment = 0x100;
    const std::int32_t header_size =
        4 * (static_cast<std::int32_t>(blocks.size()) - 1) + 36;
    std::vector<SaveBufferBlock> buffer_blocks(blocks.size());
    std::int32_t size = 0;
    for (std::size_t index = 0; index < blocks.size(); ++index) {
        SaveBlock& block = blocks[index];
        if (block.field_c == 0) {
            continue;
        }
        const std::int32_t aligned_size =
            (block.field_8 + 8 + block_alignment - 1) / block_alignment
            * block_alignment;
        if (block.field_0 == 0 && block.field_c < 0) {
            block.field_c = save_buffer_size * -block.field_c / 100
                / aligned_size;
        }
        SaveBufferBlock& result = buffer_blocks[index];
        result.field_0 = block.field_0;
        result.field_4 = block.field_8;
        result.field_8 = aligned_size;
        result.field_c = 0;
        result.field_14 = block.field_c;
        result.field_10 = block.field_c * aligned_size;
        size += result.field_10;
    }

    std::int32_t offset = block_alignment
        * (header_size + block_alignment - 1) / block_alignment;
    size += offset;
    for (std::size_t index = 0; index < blocks.size(); ++index) {
        SaveBlock& block = blocks[index];
        if (block.field_c != 0) {
            continue;
        }
        const std::int32_t aligned_size =
            (block.field_8 + 8 + block_alignment - 1) / block_alignment
            * block_alignment;
        SaveBufferBlock& result = buffer_blocks[index];
        result.field_0 = block.field_0;
        result.field_4 = block.field_8;
        result.field_8 = aligned_size;
        result.field_c = 0;
        result.field_14 = (save_buffer_size - size) / aligned_size;
        result.field_10 = result.field_14 * aligned_size;
        size += result.field_10;
    }
    for (SaveBufferBlock& block : buffer_blocks) {
        block.field_c = offset;
        offset += block.field_10;
    }
    return buffer_blocks;
}

std::int32_t get_completion_percentage(
    const StorySaveData& save) noexcept {
    if (save.max_scan_count == 0) {
        return 0;
    }
    std::int32_t counts = save.scan_count;
    for (int index = 1; index < 8; ++index) {
        if (index != 2 && (save.weapons & (1U << index)) != 0) {
            ++counts;
        }
    }
    for (int index = 0; index < 8; ++index) {
        if ((save.found_octos & (1U << index)) != 0) {
            ++counts;
        }
    }
    for (int index = 0; index < 24; ++index) {
        if ((save.artifacts & (1U << index)) != 0) {
            ++counts;
        }
    }
    counts += save.energy_cap / 100;
    counts += (static_cast<std::int32_t>(save.ammo_caps[0]) - 400) / 300;
    counts += (static_cast<std::int32_t>(save.ammo_caps[1]) - 50) / 100;
    return 100 * counts
        / (static_cast<std::int32_t>(save.max_scan_count) + 66);
}

CompletionValues get_completion_values(
    const StorySaveData& save) noexcept {
    int octolith_count = 0;
    for (int index = 0; index < 8; ++index) {
        octolith_count += (save.found_octos & (1U << index)) != 0 ? 1 : 0;
    }
    return {
        get_completion_percentage(save),
        100 * octolith_count / 8,
        save.energy_cap / 100,
        (static_cast<std::int32_t>(save.ammo_caps[0]) - 400) / 300,
        (static_cast<std::int32_t>(save.ammo_caps[1]) - 50) / 100
    };
}

Logic1Branch test_logic_1(formats::Hunter hunter,
                          std::uint32_t flags) noexcept {
    if (hunter == formats::Hunter::Noxus) {
        return Logic1Branch::Noxus;
    }
    if (static_cast<std::uint8_t>(hunter)
        > static_cast<std::uint8_t>(formats::Hunter::Samus)
        && hunter != formats::Hunter::Spire) {
        return hunter == formats::Hunter::Kanden
            ? Logic1Branch::Kanden : Logic1Branch::OtherHunter;
    }
    if (static_cast<std::uint8_t>(hunter)
            > static_cast<std::uint8_t>(formats::Hunter::Samus)
        || (flags & 0x80U) != 0) {
        return hunter == formats::Hunter::Spire
            ? Logic1Branch::SpireSpeedVector
            : Logic1Branch::SamusSpeedVector;
    }
    return Logic1Branch::SamusPreviousPosition;
}

DrawTrace test_logic_2(const PlayerDrawState& player,
                       int player_id) noexcept {
    DrawTrace trace;
    if (has_flag(player.more_flags, MoreFlags::HideModel)) {
        trace.hidden_by_flag = true;
        return trace;
    }
    if (player.hunter == formats::Hunter::Spire
        && has_flag(player.more_flags, MoreFlags::AltFormAttack)) {
        trace.animation_initialized = true;
    }
    if (player_id != 0 && !player.visible) {
        return trace;
    }

    const bool use_player_model = player_id != 0 || player.field_4d6 != 0
        || player.field_550 < player.field_46c;
    if (has_flag(player.some_flags, SomeFlags::AltForm)) {
        trace.drew_alt_model = true;
        ++trace.draw_count;
        if (player.field_4bb != 0) {
            trace.drew_attachment = true;
            ++trace.draw_count;
        }
    } else if (use_player_model) {
        if (player.health > 0) {
            trace.drew_body_model = true;
            ++trace.draw_count;
            if (player.field_4bb != 0) {
                trace.drew_attachment = true;
                ++trace.draw_count;
            }
        }
    } else if (player.field_358 == 0 && player.field_6d0 == 0) {
        trace.drew_gun = true;
        ++trace.draw_count;
        if (has_flag(player.some_flags, SomeFlags::DrawGunSmoke)) {
            trace.drew_gun_smoke = true;
            ++trace.draw_count;
        }
    }
    // _mem20E97B0 is zero in the managed probe, so its final conditional draw
    // is intentionally unreachable here as well.
    return trace;
}

} // namespace fruityprime::testing::logic
