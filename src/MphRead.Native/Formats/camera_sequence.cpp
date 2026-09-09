#include "Formats/camera_sequence.hpp"

#include "Utility/binary_reader.hpp"

#include <algorithm>
#include <array>
#include <cmath>
#include <fstream>
#include <limits>
#include <numbers>
#include <span>
#include <stdexcept>
#include <string_view>
#include <utility>

namespace fruityprime::camera {
namespace {

using Bytes = std::span<const std::uint8_t>;

constexpr std::array<std::string_view, 199> kAssetNames{
    "unit1_land_intro.bin",
    "unit2_land_intro.bin",
    "unit3_land_intro.bin",
    "unit4_land_intro.bin",
    "unit4_c1_platform_intro.bin",
    "unit2_co_scan_intro.bin",
    "unit2_co_scan_outro.bin",
    "unit2_co_bit_intro.bin",
    "unit2_c4_teleporter_intro.bin",
    "unit2_co_bit_outro.bin",
    "unit2_co_helm_flyby.bin",
    "unit2_rm1_artifact_intro.bin",
    "unit2_rm1_artifact_outro.bin",
    "unit2_c4_artifact_intro.bin",
    "unit2_c4_artifact_outro.bin",
    "unit2_rm2_kanden_intro.bin",
    "unit2_rm3_artifact_intro.bin",
    "unit2_rm3_artifact_outro.bin",
    "unit2_rm3_kanden_intro.bin",
    "unit4_co_morphballmaze.bin",
    "unit2_b1_octolith_intro.bin",
    "unit2_co_guardian_intro.bin",
    "unit2_rm3_kanden_outro.bin",
    "unit4_rm1_morphballjumps1.bin",
    "unit4_land_guardian_intro.bin",
    "unit4_rm3_scandoor_unlock.bin",
    "unit1_c4_dropmaze_left.bin",
    "unit4_co_morphballmaze_enter.bin",
    "unit4_rm5_arcticspawn_intro.bin",
    "unit4_rm5_arcticspawn_outro.bin",
    "unit1_c4_dropmaze_right.bin",
    "unit4_rm3_hunters_intro.bin",
    "unit4_rm3_hunters_outro.bin",
    "unit4_rm2_switch_intro.bin",
    "unit4_rm2_guardian_intro.bin",
    "unit1_c5_pistonmaze_1.bin",
    "unit1_c5_pistonmaze_2.bin",
    "unit1_c5_pistonmaze_3.bin",
    "unit1_c5_pistonmaze_4.bin",
    "unit4_rm2_guardian_outro.bin",
    "unit3_c2_morphballmaze.bin",
    "unit4_rm5_powerdown.bin",
    "unit4_rm1_morphballjumps2.bin",
    "unit4_rm2_elevator_intro.bin",
    "unit4_rm1_morphballjumps3.bin",
    "unit4_rm5_pillarcrash.bin",
    "unit4_rm1_wasp_intro.bin",
    "unit1_RM1_spire_intro_layer0.bin",
    "unit1_RM1_spire_intro_layer3.bin",
    "unit1_RM1_spire_outro.bin",
    "unit1_RM6_spire_intro_layer3.bin",
    "unit1_c1_shipflyby.bin",
    "unit4_rm3_trace_intro.bin",
    "unit3_rm1_forcefield_unlock.bin",
    "unit3_rm1_ship_battle_end.bin",
    "unit3_rm2_evac_intro.bin",
    "unit3_rm2_evac_fail.bin",
    "unit4_rm1_puzzle_activate.bin",
    "unit4_rm1_artifact_intro.bin",
    "unit1_c0_weavel_intro.bin",
    "bigeye_octolith_intro.bin",
    "unit2_rm4_panel_open_1.bin",
    "unit2_rm4_panel_open_2.bin",
    "unit2_rm4_panel_open_3.bin",
    "unit2_rm4_cntlroom_open.bin",
    "unit2_rm4_teleporter_active.bin",
    "unit2_rm6_teleporter_active.bin",
    "unit1_rm2_rm3door_open.bin",
    "unit1_rm2_c3door_open.bin",
    "unit1_rm3_lavademon_intro.bin",
    "unit1_rm3_magmaul_intro.bin",
    "unit3_rm3_race1.bin",
    "unit3_rm3_race1_fail.bin",
    "unit3_rm3_race2.bin",
    "unit3_rm3_race2_fail.bin",
    "unit3_rm3_incubator_malfunction_intro.bin",
    "unit3_rm3_incubator_malfunction_outro.bin",
    "unit3_rm3_door_unlock.bin",
    "unit1_rm3_forcefield_unlock.bin",
    "unit4_rm4_sniperspot_intro.bin",
    "unit4_rm5_artifact_key_intro.bin",
    "unit4_rm5_artifact_intro.bin",
    "unit3_rm2_door_unlock.bin",
    "unit3_rm2_evac_end.bin",
    "unit1_rm3_forcefield_unlock.bin",
    "unit1_c0_weavel_outro.bin",
    "unit3_rm1_sylux_preship.bin",
    "unit1_rm1_mover_activate_layer3.bin",
    "unit3_rm1_sylux_intro.bin",
    "unit4_rm3_trace_outro.bin",
    "unit4_rm5_sniper_intro.bin",
    "unit3_rm1_artifact_intro.bin",
    "unit1_rm6_spire_escape.bin",
    "unit1_crystalroom_octolith.bin",
    "unit4_co_morphballmaze_exit.bin",
    "unit4_rm3_key_intro.bin",
    "unit2_rm1_door_lock.bin",
    "unit2_rm1_key_intro.bin",
    "unit3_rm4_morphball.bin",
    "unit1_c0_morphball_door_unlock.bin",
    "unit1_rm6_forcefield_lock.bin",
    "unit1_rm6_forcefield_unlock.bin",
    "unit1_land_cockpit.bin",
    "unit2_land_cockpit.bin",
    "unit3_land_cockpit.bin",
    "unit4_land_cockpit.bin",
    "unit1_land_cockpit.bin",
    "unit1_rm1_artifact_intro.bin",
    "unit2_rm5_artifact_intro.bin",
    "unit2_c7_forcefield_lock.bin",
    "unit2_c7_forcefield_unlock.bin",
    "unit2_c7_artifact_intro.bin",
    "unit2_rm8_artifact_intro.bin",
    "unit4_co_door_unlock.bin",
    "unit1_land_cockpit_land.bin",
    "unit1_land_cockpit_takeoff.bin",
    "unit2_land_cockpit_land.bin",
    "unit2_land_cockpit_takeoff.bin",
    "unit3_land_cockpit_land.bin",
    "unit3_land_cockpit_takeoff.bin",
    "unit4_land_cockpit_land.bin",
    "unit4_land_cockpit_takeoff.bin",
    "unit1_land_cockpit_land.bin",
    "unit1_land_cockpit_takeoff.bin",
    "unit1_rm2_mover1_activate.bin",
    "unit1_rm2_mover2_activate.bin",
    "unit1_rm2_mover3_activate.bin",
    "unit3_rm3_race_artifact_intro.bin",
    "unit1_c5_artifact_intro.bin",
    "unit1_rm3_key_intro.bin",
    "unit1_rm3_artifact_intro.bin",
    "unit1_c3_artifact_intro.bin",
    "unit4_rm1_forcefield_unlock.bin",
    "unit4_rm1_wasp_outro.bin",
    "unit4_rm4_artifact_intro.bin",
    "unit4_rm4_artifact_outro.bin",
    "unit3_rm2_artifact_intro.bin",
    "unit3_rm1_ship_intro.bin",
    "bigeye1_intro.bin",
    "unit4_rm2_key_intro.bin",
    "unit2_rm1_bit_intro.bin",
    "unit1_rm1_spire_escape.bin",
    "bigeye_morphball.bin",
    "unit3_rm3_key_outro.bin",
    "unit3_rm4_key_intro.bin",
    "unit3_rm4_key_outro.bin",
    "unit1_c0_key_intro.bin",
    "unit1_rm6_key_intro.bin",
    "unit1_rm6_morphball.bin",
    "unit3_rm1_bottomfloorkey_intro.bin",
    "unit4_rm4_key_intro.bin",
    "unit4_rm4_guardian_outro.bin",
    "unit4_rm5_forcefield_outro.bin",
    "unit4_rm5_quadtroid_outro.bin",
    "unit4_rm2_key_outro.bin",
    "unit3_rm4_guardian_intro.bin",
    "unit3_rm4_morphballdoor_unlock.bin",
    "unit2_rm5_key_intro.bin",
    "unit3_rm4_item_intro.bin",
    "unit2_c7_key_intro.bin",
    "unit2_rm4_forcefield_unlock_1.bin",
    "unit2_rm4_forcefield_unlock_2.bin",
    "unit2_rm4_forcefield_unlock_3.bin",
    "unit3_c2_battlehammer_intro.bin",
    "unit4_rm3_morphball_cam.bin",
    "unit4_rm1_door_open.bin",
    "gorea_b2_gun_intro.bin",
    "gorea_land_intro.bin",
    "gorea_land_cockpit.bin",
    "gorea_land_cockpit_land.bin",
    "gorea_land_cockpit_takeoff.bin",
    "unit4_rm1_puzzle_intro.bin",
    "mp00_intro.bin",
    "mp01_intro.bin",
    "mp02_intro.bin",
    "mp03_intro.bin",
    "mp04_intro.bin",
    "mp05_intro.bin",
    "mp06_intro.bin",
    "mp07_intro.bin",
    "mp08_intro.bin",
    "mp09_intro.bin",
    "mp10_intro.bin",
    "mp11_intro.bin",
    "mp12_intro.bin",
    "mp13_intro.bin",
    "mp14_intro.bin",
    "mp15_intro.bin",
    "mp16_intro.bin",
    "mp17_intro.bin",
    "mp18_intro.bin",
    "mp19_intro.bin",
    "mp20_intro.bin",
    "mp21_intro.bin",
    "mp22_intro.bin",
    "mp23_intro.bin",
    "mp24_intro.bin",
    "mp25_intro.bin",
    "mp26_intro.bin",
};

static_assert(kAssetNames.size() == 199);

[[nodiscard]] std::vector<std::uint8_t> read_all(
    const std::filesystem::path& path) {
    std::ifstream input(path, std::ios::binary | std::ios::ate);
    if (!input) {
        throw std::runtime_error("could not open camera sequence "
                                 + path.string());
    }
    const std::streamoff length = input.tellg();
    if (length < 0 || static_cast<std::uintmax_t>(length)
        > std::numeric_limits<std::size_t>::max()) {
        throw std::runtime_error("camera sequence is too large: "
                                 + path.string());
    }
    std::vector<std::uint8_t> bytes(static_cast<std::size_t>(length));
    input.seekg(0);
    if (!bytes.empty()) {
        input.read(reinterpret_cast<char*>(bytes.data()),
                   static_cast<std::streamsize>(bytes.size()));
        if (!input) {
            throw std::runtime_error("could not read camera sequence "
                                     + path.string());
        }
    }
    return bytes;
}

[[nodiscard]] formats::Vector3 read_vector3(core::BinaryReader& reader) {
    return {reader.read_i32_le() / 4096.0F,
            reader.read_i32_le() / 4096.0F,
            reader.read_i32_le() / 4096.0F};
}

[[nodiscard]] float read_fixed(core::BinaryReader& reader) {
    return static_cast<float>(reader.read_i32_le()) / 4096.0F;
}

[[nodiscard]] formats::Vector3 add(formats::Vector3 left,
                                    formats::Vector3 right) noexcept {
    return left + right;
}

[[nodiscard]] formats::Vector3 multiply(formats::Vector3 value,
                                         float factor) noexcept {
    return value * factor;
}

[[nodiscard]] formats::Vector3 bezier(formats::Vector3 one,
                                       formats::Vector3 two,
                                       formats::Vector3 three,
                                       formats::Vector3 four,
                                       float first, float second, float third,
                                       float fourth) noexcept {
    return multiply(one, first) + multiply(two, second)
        + multiply(three, third) + multiply(four, fourth);
}

[[nodiscard]] formats::Vector4 bezier_weights(float percent) noexcept {
    const float pct_sqr = percent * percent;
    const float inverse = 1.0F - percent;
    const float inv_sqr = inverse * inverse;
    return {inv_sqr * inverse,
            3.0F * percent * inv_sqr,
            3.0F * pct_sqr * inverse,
            pct_sqr * percent};
}

[[nodiscard]] float dot(formats::Vector4 left,
                        formats::Vector4 right) noexcept {
    return left.x * right.x + left.y * right.y + left.z * right.z
        + left.w * right.w;
}

[[nodiscard]] formats::Vector3 normalized(formats::Vector3 value) noexcept {
    return value.normalized();
}

[[nodiscard]] formats::Vector3 lerp(formats::Vector3 left,
                                     formats::Vector3 right,
                                     float percent) noexcept {
    return left * (1.0F - percent) + right * percent;
}

[[nodiscard]] formats::Vector3 authored_value(
    const std::vector<Keyframe>& keyframes, std::size_t index,
    bool target) noexcept {
    return target ? keyframes[index].to_target : keyframes[index].position;
}

[[nodiscard]] formats::Vector3 cubic_value(
    const std::vector<Keyframe>& keyframes, std::size_t index,
    float factor_dot, bool target) noexcept {
    const Keyframe& current = keyframes[index];
    const Keyframe& next = keyframes[index + 1];
    const formats::Vector3 cur = authored_value(keyframes, index, target);
    const formats::Vector3 next_value = authored_value(keyframes, index + 1,
                                                        target);
    const float move_time = current.move_time;
    if (move_time <= 0.0F) {
        return cur;
    }
    const formats::Vector3 cur_to_next =
        (next_value - cur) / move_time;

    formats::Vector3 previous;
    if (index > 0 && (current.prev_frame_influence & 2U) != 0
        && keyframes[index - 1].move_time > 0.0F) {
        const formats::Vector3 previous_value =
            authored_value(keyframes, index - 1, target);
        const formats::Vector3 previous_to_cur =
            (cur - previous_value) / keyframes[index - 1].move_time;
        const formats::Vector3 previous_to_next =
            cur_to_next + previous_to_cur;
        const float easing = (1.0F / 6.0F) * move_time * current.easing;
        previous = previous_to_next * easing + cur;
    } else {
        const float easing = (1.0F / 3.0F) * move_time * current.easing;
        previous = cur_to_next * easing + cur;
    }

    formats::Vector3 after;
    if (index + 2 < keyframes.size()
        && (current.after_frame_influence & 2U) != 0
        && next.move_time > 0.0F) {
        const formats::Vector3 after_value =
            authored_value(keyframes, index + 2, target);
        const formats::Vector3 next_to_after =
            (after_value - next_value) / next.move_time;
        const formats::Vector3 current_to_after = cur_to_next + next_to_after;
        const float easing = -(1.0F / 6.0F) * move_time * next.easing;
        after = current_to_after * easing + next_value;
    } else {
        const float easing = -(1.0F / 3.0F) * move_time * current.easing;
        after = cur_to_next * easing + next_value;
    }

    const formats::Vector4 weights = bezier_weights(factor_dot);
    return bezier(cur, previous, after, next_value,
                  weights.x, weights.y, weights.z, weights.w);
}

[[nodiscard]] formats::Vector3 resolved_authored_value(
    const std::vector<ResolvedKeyframe>& keyframes, std::size_t index,
    bool target) noexcept {
    return target ? keyframes[index].to_target : keyframes[index].position;
}

[[nodiscard]] formats::Vector3 resolved_cubic_value(
    const std::vector<ResolvedKeyframe>& resolved,
    const std::vector<Keyframe>& keyframes, std::size_t index,
    float factor_dot, bool target) noexcept {
    const Keyframe& current = keyframes[index];
    const Keyframe& next = keyframes[index + 1];
    const formats::Vector3 cur = resolved_authored_value(resolved, index,
                                                          target);
    const formats::Vector3 next_value = resolved_authored_value(
        resolved, index + 1, target);
    const float move_time = current.move_time;
    if (move_time <= 0.0F) {
        return cur;
    }
    const formats::Vector3 cur_to_next = (next_value - cur) / move_time;

    formats::Vector3 previous;
    if (index > 0 && (current.prev_frame_influence & 2U) != 0
        && keyframes[index - 1].move_time > 0.0F) {
        const formats::Vector3 previous_value = resolved_authored_value(
            resolved, index - 1, target);
        const formats::Vector3 previous_to_cur =
            (cur - previous_value) / keyframes[index - 1].move_time;
        const formats::Vector3 previous_to_next =
            cur_to_next + previous_to_cur;
        const float easing = (1.0F / 6.0F) * move_time * current.easing;
        previous = previous_to_next * easing + cur;
    } else {
        const float easing = (1.0F / 3.0F) * move_time * current.easing;
        previous = cur_to_next * easing + cur;
    }

    formats::Vector3 after;
    if (index + 2 < keyframes.size()
        && (current.after_frame_influence & 2U) != 0
        && next.move_time > 0.0F) {
        const formats::Vector3 after_value = resolved_authored_value(
            resolved, index + 2, target);
        const formats::Vector3 next_to_after =
            (after_value - next_value) / next.move_time;
        const formats::Vector3 current_to_after = cur_to_next + next_to_after;
        const float easing = -(1.0F / 6.0F) * move_time * next.easing;
        after = current_to_after * easing + next_value;
    } else {
        const float easing = -(1.0F / 3.0F) * move_time * current.easing;
        after = cur_to_next * easing + next_value;
    }

    const formats::Vector4 weights = bezier_weights(factor_dot);
    return bezier(cur, previous, after, next_value,
                  weights.x, weights.y, weights.z, weights.w);
}

void apply_camera_roll(formats::Vector3& up_vector,
                       formats::Vector3 to_target, float roll) noexcept {
    if (std::fabs(roll) < 1.0F / 4096.0F) {
        return;
    }
    const formats::Vector3 facing = normalized(to_target);
    if (facing.length_squared() <= std::numeric_limits<float>::epsilon()) {
        return;
    }
    const formats::Vector3 world_up{0.0F, 1.0F, 0.0F};
    formats::Vector3 cross_value = normalized(formats::cross(world_up, facing));
    if (cross_value.length_squared() <= std::numeric_limits<float>::epsilon()) {
        cross_value = normalized(formats::cross(formats::Vector3{1.0F, 0.0F, 0.0F},
                                                 facing));
    }
    if (cross_value.length_squared() <= std::numeric_limits<float>::epsilon()) {
        return;
    }
    const formats::Vector3 perpendicular = normalized(
        formats::cross(facing, cross_value));
    const float angle = (roll + 90.0F) * std::numbers::pi_v<float> / 180.0F;
    up_vector = {
        cross_value.x * std::cos(angle)
            + perpendicular.x * std::sin(angle),
        cross_value.y * std::cos(angle)
            + perpendicular.y * std::sin(angle),
        cross_value.z * std::cos(angle)
            + perpendicular.z * std::sin(angle)
    };
    up_vector = normalized(up_vector);
}

} // namespace

std::string_view asset_name(int sequence_id) noexcept {
    if (sequence_id < 0
        || static_cast<std::size_t>(sequence_id) >= kAssetNames.size()) {
        return {};
    }
    return kAssetNames[static_cast<std::size_t>(sequence_id)];
}

std::string asset_path(int sequence_id) {
    const std::string_view name = asset_name(sequence_id);
    if (name.empty()) {
        return {};
    }
    return "cameraEditor/" + std::string(name);
}

File File::read_file(const std::filesystem::path& path, int id,
                     std::string name) {
    if (name.empty()) {
        name = path.filename().string();
    }
    return File(read_all(path), id, std::move(name));
}

File File::from_bytes(std::vector<std::uint8_t> bytes, int id,
                      std::string name) {
    return File(std::move(bytes), id, std::move(name));
}

File::File(std::vector<std::uint8_t> bytes, int id, std::string name)
    : bytes_(std::move(bytes)), id_(id), name_(std::move(name)) {
    parse();
}

void File::parse() {
    if (bytes_.size() < Header::Size) {
        throw std::runtime_error("camera sequence is smaller than its header");
    }
    const Bytes bytes(bytes_);
    core::BinaryReader header_reader(bytes.subspan(0, Header::Size));
    header_.count = header_reader.read_u16_le();
    header_.version = header_reader.read_u8();
    header_.padding_3 = header_reader.read_u8();
    header_.padding_4 = header_reader.read_u32_le();
    if (header_.padding_3 != 0 || header_.padding_4 != 0) {
        throw std::runtime_error("camera sequence header padding is nonzero");
    }
    if (header_.count > (bytes.size() - Header::Size) / Keyframe::Size) {
        throw std::runtime_error("camera sequence has too many keyframes");
    }

    keyframes_.reserve(header_.count);
    for (std::size_t i = 0; i < header_.count; ++i) {
        core::BinaryReader reader(bytes.subspan(
            Header::Size + i * Keyframe::Size, Keyframe::Size));
        Keyframe keyframe;
        keyframe.position = read_vector3(reader);
        keyframe.to_target = read_vector3(reader);
        keyframe.roll = read_fixed(reader);
        keyframe.fov = read_fixed(reader);
        keyframe.move_time = read_fixed(reader);
        keyframe.hold_time = read_fixed(reader);
        keyframe.fade_in_time = read_fixed(reader);
        keyframe.fade_out_time = read_fixed(reader);
        keyframe.fade_in_type = static_cast<formats::FadeType>(reader.read_u8());
        keyframe.fade_out_type = static_cast<formats::FadeType>(reader.read_u8());
        keyframe.prev_frame_influence = reader.read_u8();
        keyframe.after_frame_influence = reader.read_u8();
        keyframe.use_entity_transform = reader.read_u8() != 0;
        (void)reader.read_u8();
        (void)reader.read_u16_le();
        keyframe.pos_entity_type = reader.read_i16_le();
        keyframe.pos_entity_id = reader.read_i16_le();
        keyframe.target_entity_type = reader.read_i16_le();
        keyframe.target_entity_id = reader.read_i16_le();
        keyframe.message_target_type = reader.read_i16_le();
        keyframe.message_target_id = reader.read_i16_le();
        keyframe.message_id = reader.read_u16_le();
        keyframe.message_param = reader.read_u16_le();
        keyframe.easing = read_fixed(reader);
        (void)reader.read_u32_le();
        (void)reader.read_u32_le();
        keyframe.node_name = reader.read_raw_string(16);
        keyframes_.push_back(std::move(keyframe));
    }
}

float File::duration() const noexcept {
    float result = 0.0F;
    for (const Keyframe& keyframe : keyframes_) {
        result += std::max(0.0F, keyframe.move_time)
            + std::max(0.0F, keyframe.hold_time);
    }
    return result;
}

Sample File::sample(std::size_t keyframe_index, float elapsed) const {
    if (keyframe_index >= keyframes_.size()) {
        throw std::out_of_range("camera keyframe index is out of range");
    }
    const Keyframe& current = keyframes_[keyframe_index];
    const float safe_elapsed = std::max(0.0F, elapsed);
    float move_percent = 0.0F;
    if (current.move_time > 0.0F
        && safe_elapsed > current.hold_time) {
        move_percent = std::clamp(
            (safe_elapsed - current.hold_time) / current.move_time,
            0.0F, 1.0F);
    }
    const formats::Vector4 move_weights = bezier_weights(move_percent);
    const formats::Vector4 factor_weights{
        0.0F,
        (current.prev_frame_influence & 1U) == 0 ? 1.0F / 3.0F : 0.0F,
        (current.after_frame_influence & 1U) == 0 ? 2.0F / 3.0F : 1.0F,
        1.0F};
    const float factor_dot = dot(factor_weights, move_weights);

    formats::Vector3 position = current.position;
    formats::Vector3 to_target = current.to_target;
    float roll = current.roll;
    float fov = current.fov;
    if (keyframe_index + 1 < keyframes_.size()) {
        const Keyframe& next = keyframes_[keyframe_index + 1];
        if (((current.prev_frame_influence | current.after_frame_influence) & 2U)
                != 0
            && current.move_time > 0.0F) {
            position = cubic_value(keyframes_, keyframe_index, factor_dot,
                                   false);
            to_target = cubic_value(keyframes_, keyframe_index, factor_dot,
                                    true);
        } else {
            position = lerp(current.position, next.position, factor_dot);
            to_target = lerp(current.to_target, next.to_target, factor_dot);
        }
        roll = current.roll * (1.0F - factor_dot) + next.roll * factor_dot;
        fov = current.fov * (1.0F - factor_dot) + next.fov * factor_dot;
    }

    Sample result;
    result.position = position;
    result.target = add(position, to_target);
    result.roll = roll;
    result.fov = fov * 2.0F;
    if (std::fabs(roll) < 1.0F / 4096.0F) {
        return result;
    }

    const formats::Vector3 facing = normalized(to_target);
    if (facing.length_squared() <= std::numeric_limits<float>::epsilon()) {
        return result;
    }
    const formats::Vector3 world_up{0.0F, 1.0F, 0.0F};
    const formats::Vector3 cross_value = normalized(
        formats::cross(world_up, facing));
    if (cross_value.length_squared()
        <= std::numeric_limits<float>::epsilon()) {
        return result;
    }
    const formats::Vector3 perpendicular = normalized(
        formats::cross(facing, cross_value));
    const float angle = (roll + 90.0F) * std::numbers::pi_v<float> / 180.0F;
    result.up_vector = {
        cross_value.x * std::cos(angle),
        perpendicular.y * std::sin(angle),
        cross_value.z * std::cos(angle)
    };
    return result;
}

Playback::Playback(int sequence_id, const File& file)
    : sequence_id_(sequence_id), file_(file) {
    // These are the same sequences the managed loader marks as looping
    // before CamSeqEntity applies its per-entity flags.
    if (sequence_id_ > 171
        || sequence_id_ == 102 || sequence_id_ == 103
        || sequence_id_ == 104 || sequence_id_ == 105
        || sequence_id_ == 106 || sequence_id_ == 168) {
        flags_ |= Flags::Loop;
    }
}

ResolvedKeyframe Playback::resolve_keyframe(
    const Keyframe& keyframe) const {
    ResolvedKeyframe result{keyframe.position, keyframe.to_target};
    std::optional<EntityPose> position_entity;
    if (entity_resolver_ && keyframe.pos_entity_type != -1
        && keyframe.pos_entity_id != -1) {
        position_entity = entity_resolver_(keyframe.pos_entity_type,
                                           keyframe.pos_entity_id);
    }
    if (position_entity.has_value()) {
        if (keyframe.use_entity_transform) {
            const formats::Vector3 entity_facing = normalized(
                position_entity->facing);
            formats::Vector3 entity_up = normalized(position_entity->up);
            formats::Vector3 entity_right = normalized(
                formats::cross(entity_up, entity_facing));
            entity_up = normalized(formats::cross(entity_facing,
                                                   entity_right));
            result.position = position_entity->position
                + entity_right * keyframe.position.x
                + entity_up * keyframe.position.y
                + entity_facing * keyframe.position.z;
        } else {
            result.position += position_entity->position;
        }
    }

    if (entity_resolver_ && keyframe.target_entity_type != -1
        && keyframe.target_entity_id != -1) {
        const auto target_entity = entity_resolver_(
            keyframe.target_entity_type, keyframe.target_entity_id);
        if (target_entity.has_value()) {
            const formats::Vector3 between = normalized(
                target_entity->position - result.position);
            const formats::Vector3 cross_one = normalized(formats::cross(
                formats::Vector3{0.0F, 1.0F, 0.0F}, between));
            const formats::Vector3 cross_two = formats::cross(
                between, cross_one);
            result.to_target = cross_one * keyframe.to_target.x
                + cross_two * keyframe.to_target.y
                + between * keyframe.to_target.z;
        }
    }
    return result;
}

Sample Playback::calculate_sample(float elapsed) const {
    Sample result;
    const auto& keyframes = file_.keyframes();
    if (keyframe_index_ >= keyframes.size()) {
        return result;
    }

    std::vector<ResolvedKeyframe> resolved;
    resolved.reserve(keyframes.size());
    for (const Keyframe& keyframe : keyframes) {
        resolved.push_back(resolve_keyframe(keyframe));
    }

    const Keyframe& current = keyframes[keyframe_index_];
    const float safe_elapsed = std::max(0.0F, elapsed);
    float move_percent = 0.0F;
    if (current.move_time > 0.0F && safe_elapsed >= current.hold_time) {
        move_percent = (safe_elapsed - current.hold_time)
            / current.move_time;
    }
    const formats::Vector4 move_weights = bezier_weights(move_percent);
    const formats::Vector4 factor_weights{
        0.0F,
        (current.prev_frame_influence & 1U) == 0 ? 1.0F / 3.0F : 0.0F,
        (current.after_frame_influence & 1U) == 0 ? 2.0F / 3.0F : 1.0F,
        1.0F
    };
    const float factor_dot = dot(factor_weights, move_weights);
    formats::Vector3 position = resolved[keyframe_index_].position;
    formats::Vector3 to_target = resolved[keyframe_index_].to_target;
    float roll = current.roll;
    float fov = current.fov;
    if (keyframe_index_ + 1 < keyframes.size()) {
        const Keyframe& next = keyframes[keyframe_index_ + 1];
        if (((current.prev_frame_influence | current.after_frame_influence) & 2U)
                != 0
            && current.move_time > 0.0F) {
            position = resolved_cubic_value(resolved, keyframes,
                                            keyframe_index_, factor_dot, false);
            to_target = resolved_cubic_value(resolved, keyframes,
                                             keyframe_index_, factor_dot, true);
        } else {
            position = resolved[keyframe_index_].position * (1.0F - factor_dot)
                + resolved[keyframe_index_ + 1].position * factor_dot;
            to_target = resolved[keyframe_index_].to_target * (1.0F - factor_dot)
                + resolved[keyframe_index_ + 1].to_target * factor_dot;
        }
        roll = current.roll * (1.0F - factor_dot)
            + next.roll * factor_dot;
        fov = current.fov * (1.0F - factor_dot) + next.fov * factor_dot;
    }

    result.position = position;
    result.target = position + to_target;
    result.roll = roll;
    result.fov = fov * 2.0F;
    apply_camera_roll(result.up_vector, to_target, roll);
    return result;
}

void Playback::calculate_frame_values() {
    const auto& keyframes = file_.keyframes();
    if (keyframes.empty() || keyframe_index_ >= keyframes.size()) {
        return;
    }
    const Sample final = calculate_sample(keyframe_elapsed_);
    const formats::Vector3 final_to_target = final.target - final.position;
    const bool transitioned = transition_time_ == 0
        || transition_timer_ >= transition_time_;
    if (transitioned) {
        camera_.position = final.position;
        camera_.target = final.target;
        camera_.up_vector = final.up_vector;
        camera_.fov = final.fov;
    } else {
        const float percent = static_cast<float>(transition_timer_)
            / static_cast<float>(transition_time_);
        camera_.fov += (final.fov - camera_.fov) * percent;
        camera_.position += (final.position - camera_.position) * percent;
        const formats::Vector3 current_facing = camera_.facing.length_squared()
            > std::numeric_limits<float>::epsilon()
            ? camera_.facing : camera_.target - camera_.position;
        const formats::Vector3 blended_target = current_facing
            + (final_to_target - current_facing) * percent;
        camera_.target = camera_.position + blended_target;
        camera_.up_vector = normalized(camera_.up_vector
            + (final.up_vector - camera_.up_vector) * percent);
    }
    camera_.facing = normalized(camera_.target - camera_.position);
    camera_.node_name = keyframes[keyframe_index_].node_name;
}

void Playback::emit_fade(const Keyframe& keyframe, float frame_length,
                         bool first_frame) const {
    if (!fade_sink_) {
        return;
    }
    formats::FadeType type = formats::FadeType::None;
    float seconds = 0.0F;
    const float fade_out_start = frame_length - keyframe.fade_out_time;
    if (keyframe.fade_in_type != formats::FadeType::None
        && keyframe_elapsed_ <= 2.0F / 30.0F) {
        type = keyframe.fade_in_type;
        seconds = keyframe.fade_in_time;
    } else if (keyframe.fade_out_type != formats::FadeType::None
               && keyframe_elapsed_ >= fade_out_start
               && keyframe_elapsed_ <= fade_out_start + 2.0F / 30.0F) {
        type = keyframe.fade_out_type;
        seconds = keyframe.fade_out_time;
    } else if (first_frame && keyframe_index_ == 0
               && keyframe_elapsed_ == 0.0F
               && (sequence_id_ == 0 || sequence_id_ == 3
                   || sequence_id_ == 167)) {
        type = formats::FadeType::FadeInWhite;
        seconds = 5.0F / 30.0F;
    }
    if (type == formats::FadeType::None) {
        return;
    }
    const bool overwrite = keyframe_index_ == 0 && keyframe_elapsed_ == 0.0F
        && (sequence_id_ == 0 || sequence_id_ == 1 || sequence_id_ == 2
            || sequence_id_ == 3 || sequence_id_ == 167);
    fade_sink_(type, seconds, overwrite);
}

void Playback::set_up(CameraState camera, std::uint16_t transition_time) {
    camera_ = std::move(camera);
    camera_.previous_position = camera_.position;
    initial_camera_ = camera_;
    flags_ &= static_cast<Flags>(~static_cast<std::uint8_t>(
        Flags::Complete) & ~static_cast<std::uint8_t>(Flags::CanEnd));
    transition_timer_ = 0;
    transition_time_ = transition_time;
    keyframe_index_ = 0;
    keyframe_elapsed_ = 0.0F;
    if (!file_.keyframes().empty()) {
        calculate_frame_values();
    }
}

void Playback::restart(std::uint16_t transition_timer,
                       std::uint16_t transition_time) {
    flags_ &= static_cast<Flags>(~static_cast<std::uint8_t>(
        Flags::Complete) & ~static_cast<std::uint8_t>(Flags::CanEnd));
    transition_timer_ = transition_timer;
    transition_time_ = transition_time;
    keyframe_index_ = 0;
    keyframe_elapsed_ = 0.0F;
    camera_.previous_position = camera_.position;
    if (!file_.keyframes().empty()) {
        calculate_frame_values();
    }
}

void Playback::process(float frame_seconds) {
    if (complete() || file_.keyframes().empty()) {
        return;
    }
    if (!std::isfinite(frame_seconds) || frame_seconds < 0.0F) {
        frame_seconds = 0.0F;
    }
    const Keyframe& current = file_.keyframes()[keyframe_index_];
    const float frame_length = std::max(0.0F, current.hold_time)
        + std::max(0.0F, current.move_time);
    camera_.shake = 0.0F;
    camera_.previous_position = camera_.position;
    calculate_frame_values();
    const bool first_frame = keyframe_elapsed_ < 1.0F / 60.0F;
    if (first_frame && current.message_id != 0 && message_sink_) {
        message_sink_(MessageEvent{current.message_id,
                                   current.message_target_type,
                                   current.message_target_id,
                                   current.message_param});
    }
    emit_fade(current, frame_length, first_frame);

    keyframe_elapsed_ += frame_seconds;
    if (frame_length <= std::numeric_limits<float>::epsilon()
        || keyframe_elapsed_ >= frame_length) {
        keyframe_elapsed_ = std::max(0.0F, keyframe_elapsed_ - frame_length);
        if (keyframe_elapsed_ >= 1.0F / 60.0F) {
            keyframe_elapsed_ = 1.0F / 60.0F - 1.0F / 4096.0F;
        }
        ++keyframe_index_;
        if (keyframe_index_ >= file_.keyframes().size()) {
            if (has_flag(flags_, Flags::Loop)) {
                restart(transition_timer_, transition_time_);
            } else {
                flags_ |= Flags::CanEnd;
                flags_ |= Flags::Complete;
                keyframe_index_ = file_.keyframes().size() - 1;
                keyframe_elapsed_ = frame_length;
            }
        }
    }
    if (transition_timer_ < transition_time_) {
        ++transition_timer_;
    }
    if (!complete() && keyframe_index_ < file_.keyframes().size()) {
        // The current frame values were calculated before advancing timing,
        // matching Scene.Process's one-frame camera update semantics.
        camera_.node_name = file_.keyframes()[keyframe_index_].node_name;
    }
    if (form_lock_sink_) {
        if (force_alt()) {
            form_lock_sink_(true);
        } else if (force_biped()) {
            form_lock_sink_(false);
        }
    }
}

void Playback::end() noexcept {
    flags_ |= Flags::CanEnd;
    flags_ |= Flags::Complete;
    transition_timer_ = 0;
    transition_time_ = 0;
    keyframe_index_ = 0;
    keyframe_elapsed_ = 0.0F;
    camera_ = initial_camera_;
}

} // namespace fruityprime::camera
