// Native counterpart of src/MphRead/Entities/Players/PlayerCamera.cs.
#include "PlayerCamera.hpp"
#include "Utility/rng.hpp"

#include <algorithm>
#include <cmath>

namespace fruityprime::players {
namespace {

constexpr float Pi = 3.14159265358979323846F;

[[nodiscard]] net::Vec3 add(net::Vec3 a, net::Vec3 b) noexcept {
    return {a.x + b.x, a.y + b.y, a.z + b.z};
}

[[nodiscard]] net::Vec3 multiply(net::Vec3 value, float factor) noexcept {
    return {value.x * factor, value.y * factor, value.z * factor};
}

[[nodiscard]] net::Vec3 normalized_or(net::Vec3 value,
                                      net::Vec3 fallback) noexcept {
    const float length_squared = value.x * value.x + value.y * value.y
        + value.z * value.z;
    if (length_squared <= 0.0000001F) {
        return fallback;
    }
    const float inverse = 1.0F / std::sqrt(length_squared);
    return multiply(value, inverse);
}

[[nodiscard]] net::Vec3 rotate_x(net::Vec3 value, float angle) noexcept {
    const float sine = std::sin(angle);
    const float cosine = std::cos(angle);
    return {value.x, value.y * cosine - value.z * sine,
            value.y * sine + value.z * cosine};
}

[[nodiscard]] net::Vec3 rotate_y(net::Vec3 value, float angle) noexcept {
    const float sine = std::sin(angle);
    const float cosine = std::cos(angle);
    return {value.x * cosine + value.z * sine, value.y,
            -value.x * sine + value.z * cosine};
}

[[nodiscard]] net::Vec3 subtract(net::Vec3 a, net::Vec3 b) noexcept {
    return {a.x - b.x, a.y - b.y, a.z - b.z};
}

[[nodiscard]] net::Vec3 cross(net::Vec3 a, net::Vec3 b) noexcept {
    return {a.y * b.z - a.z * b.y,
            a.z * b.x - a.x * b.z,
            a.x * b.y - a.y * b.x};
}

} // namespace

void CameraInfo::reset() noexcept {
    previous_position = {0.0F, 0.0F, 1.0F};
    position = previous_position;
    target = {};
    up = {0.0F, 1.0F, 0.0F};
    true_up = {};
    facing = {};
    fov_degrees = 78.0F;
    shake = 0.0F;
    field48 = field4c = field50 = field54 = 0.0F;
    shake_frame_ = true;
}

void CameraInfo::update() noexcept {
    const net::Vec3 original_to_target = subtract(target, position);
    true_up = cross(original_to_target, cross(up, original_to_target));
    if (shake > 0.0F && shake_frame_) {
        const auto random_offset = [this]() noexcept {
            const auto fixed_shake = static_cast<std::int32_t>(shake * 4096.0F);
            return static_cast<float>(utility::get_random_int2(fixed_shake))
                    / 4096.0F
                - shake / 2.0F;
        };
        target.x += random_offset();
        target.y += random_offset();
        target.z += random_offset();
        if (original_to_target.x * (target.x - position.x)
                + original_to_target.z * (target.z - position.z) < 0.0F) {
            target.x = position.x + original_to_target.x / 2.0F;
            target.z = position.z + original_to_target.z / 2.0F;
        }
        shake *= 0.85F;
        if (shake < 0.01F) {
            shake = 0.0F;
        }
    }
    shake_frame_ = !shake_frame_;
    const net::Vec3 to_target = subtract(target, position);
    facing = normalized_or(to_target, facing);
    const float horizontal = std::sqrt(
        to_target.x * to_target.x + to_target.z * to_target.z);
    if (horizontal > 0.0F) {
        field48 = to_target.x / horizontal;
        field4c = to_target.z / horizontal;
        field50 = field4c;
        field54 = -field48;
    }
}

void CameraInfo::set_shake(float value) noexcept {
    if (shake < value) {
        shake = value;
    }
}

void PlayerCamera::reset() noexcept {
    type_ = CameraType::First;
    info_.reset();
    saved_position_ = {};
    switch_seconds_ = 0.0F;
}

void PlayerCamera::switch_camera(CameraType type, net::Vec3 facing) noexcept {
    facing = normalized_or(facing, info_.facing);
    if (type == CameraType::Third1) {
        info_.target = add(info_.position, facing);
        info_.position = add(info_.position, multiply(facing, -1.0F / 64.0F));
    } else if (type == CameraType::Free) {
        info_.target = facing;
        if (type_ == CameraType::First) {
            const net::Vec3 vector = normalized_or(
                {info_.position.x - info_.target.x,
                 info_.position.y - info_.target.y,
                 info_.position.z - info_.target.z}, {1.0F, 0.0F, 0.0F});
            info_.position = add(info_.position, multiply(vector, 1.0F / 64.0F));
        }
    }
    type_ = type;
    saved_position_ = info_.position;
    switch_seconds_ = 0.25F;
    info_.shake = 0.0F;
}

void PlayerCamera::update(const net::PlayerState& player,
                          const Profile& profile, float frame_seconds,
                          bool camera_sequence_active,
                          std::optional<net::Vec3> morph_camera_position)
    noexcept {
    const float seconds = std::max(0.0F, frame_seconds);
    info_.previous_position = info_.position;
    info_.facing = normalized_or(player.facing, info_.facing);
    // PlayerValues.NormalFov is the half-angle fixed-point value. The
    // managed CameraInfo stores the full field of view (NormalFov * 2), and
    // Renderer.GetPerspectiveMatrix consumes that full angle.
    info_.fov_degrees = (player.flags & net::PlayerState::FlagZoomed) != 0
        ? 30.0F : Profile::fixed_to_float(profile.normal_fov) * 2.0F;
    if (info_.fov_degrees <= 0.0F) {
        info_.fov_degrees = 78.0F;
    }
    if (camera_sequence_active) {
        return;
    }
    const bool alt_form = (player.flags & net::PlayerState::FlagAltForm) != 0;
    // Matches PlayerCamera.UpdateCameraFirst's Values.AimYOffset (3686 /
    // 4096 = 0.89990234375 for every hunter in the C# table).
    const net::Vec3 player_eye = add(
        player.position,
        alt_form ? net::Vec3{0.0F, 0.5F, 0.0F}
                 : net::Vec3{0.0F, 3686.0F / 4096.0F, 0.0F});
    if (type_ == CameraType::First) {
        info_.position = player_eye;
        info_.target = add(info_.position, info_.facing);
        info_.up = {0.0F, 1.0F, 0.0F};
    } else if (type_ == CameraType::Third1 || type_ == CameraType::Third2) {
        const net::Vec3 target = add(player.position,
                                     alt_form ? net::Vec3{0.0F, 0.25F, 0.0F}
                                              : net::Vec3{0.0F, 0.7F, 0.0F});
        info_.target = target;
        if (morph_camera_position.has_value()) {
            info_.position = *morph_camera_position;
        } else {
            const net::Vec3 behind = multiply(info_.facing, -2.0F);
            const net::Vec3 desired = add(target, add(behind, {0.0F, 0.8F, 0.0F}));
            const float blend = std::clamp(seconds * 12.0F, 0.0F, 1.0F);
            info_.position = add(info_.position,
                                 multiply({desired.x - info_.position.x,
                                            desired.y - info_.position.y,
                                            desired.z - info_.position.z}, blend));
        }
        info_.up = {0.0F, 1.0F, 0.0F};
    } else if (type_ == CameraType::Free) {
        info_.target = add(info_.position, info_.facing);
    } else {
        info_.target = add(info_.position, info_.facing);
    }
    info_.update();
    if (switch_seconds_ > 0.0F) {
        switch_seconds_ = std::max(0.0F, switch_seconds_ - seconds);
    }
}

// PlayerCamera.UpdateCamera's tail: the camera's own horizontal facing, and
// the right-hand vector at ninety degrees to it.  The player reads the second
// one back as _gunVec2, which is what holds the arm cannon out to one side --
// so leaving these at zero puts the cannon in the middle of the view.
void PlayerCamera::update_horizontal_basis() noexcept {
    const net::Vec3 to_target{info_.target.x - info_.position.x,
                              info_.target.y - info_.position.y,
                              info_.target.z - info_.position.z};
    const float horizontal = std::sqrt(
        to_target.x * to_target.x + to_target.z * to_target.z);
    if (!std::isfinite(horizontal) || horizontal <= 0.000001F) {
        return;
    }
    info_.field48 = to_target.x / horizontal;
    info_.field4c = to_target.z / horizontal;
    info_.field50 = info_.field4c;
    info_.field54 = -info_.field48;
}

void PlayerCamera::rotate(float horizontal_radians,
                          float vertical_radians) noexcept {
    info_.facing = normalized_or(
        rotate_x(rotate_y(info_.facing, horizontal_radians),
                 vertical_radians), info_.facing);
    info_.target = add(info_.position, info_.facing);
}

MatchEndBasis PlayerCamera::set_up_match_end_camera() const noexcept {
    // The cached basis is copied wholesale rather than recomputed: the match
    // ends mid-frame, and recomputing it from the facing vector would drop the
    // sub-frame smoothing the camera was already carrying.
    MatchEndBasis basis;
    basis.field70 = info_.field48;
    basis.field74 = info_.field4c;
    basis.gun_vec2 = {info_.field50, 0.0F, info_.field54};
    basis.facing = info_.facing;
    return basis;
}

void PlayerCamera::update_match_end_camera(
    const MatchEndWinner& winner, float time_since_match_end,
    const entities::PlayerValues& values,
    const CameraBlockedQuery& blocked) noexcept {
    type_ = CameraType::Third2;
    info_.target = winner.position;
    if (winner.alt_form) {
        // A ball has no shoulder to look over, so the camera sits further back
        // and higher.
        info_.position = {
            winner.position.x - 2.75F * winner.field70,
            winner.position.y + 3.0F,
            winner.position.z - 2.75F * winner.field74
        };
    } else {
        info_.target = add(info_.target, multiply(winner.facing, 10.0F));
        // Both axes are offset by gun_vec2.x, not x and z: that is what the
        // managed code does, and changing it moves the winner off centre.
        info_.position = {
            winner.position.x
                - (1.5F * winner.field70 + winner.gun_vec2.x / 2.0F),
            winner.position.y + 1.75F,
            winner.position.z
                - (1.5F * winner.field74 + winner.gun_vec2.x / 2.0F)
        };
    }
    if (winner.facing.y < 0.0F) {
        // Looking down pushes the camera back and up; looking up pulls it in
        // and down, by different factors.
        info_.position = {
            info_.position.x - 2.15F * winner.facing.y * winner.field70,
            info_.position.y - winner.facing.y / 2.0F,
            info_.position.z - 2.15F * winner.facing.y * winner.field74
        };
    } else {
        info_.position = {
            info_.position.x + 0.75F * winner.facing.y * winner.field70,
            info_.position.y - winner.facing.y * 2.0F,
            info_.position.z + 0.75F * winner.facing.y * winner.field74
        };
    }
    // 15/4096 of a unit per frame, so the shot drifts sideways for as long as
    // the results screen is up.
    const float factor = 15.0F / 4096.0F * time_since_match_end * 30.0F;
    info_.position.x += winner.gun_vec2.x * factor;
    info_.position.z += winner.gun_vec2.z * factor;
    if (blocked) {
        float distance = 0.0F;
        net::Vec3 plane{};
        if (blocked(winner.position, info_.position, distance, plane)) {
            // Pulled in to the hit, then nudged off the surface so the near
            // plane does not clip into it.
            const net::Vec3 between = {
                info_.position.x - winner.position.x,
                info_.position.y - winner.position.y,
                info_.position.z - winner.position.z
            };
            info_.position = {
                winner.position.x + between.x * distance + plane.x * 0.05F,
                winner.position.y + between.y * distance + plane.y * 0.05F,
                winner.position.z + between.z * distance + plane.z * 0.05F
            };
        }
    }
    info_.up = {0.0F, 1.0F, 0.0F};
    info_.shake = 0.0F;
    info_.fov_degrees = static_cast<float>(values.NormalFov) / 4096.0F * 2.0F;
    info_.facing = normalized_or(
        {info_.target.x - info_.position.x, info_.target.y - info_.position.y,
         info_.target.z - info_.position.z},
        info_.facing);
    info_.update();
}

void PlayerCamera::refresh_external_camera() noexcept {
    alt_dir_override_ = true;
    time_since_morph_camera_ = 0.0F;
}

void PlayerCamera::resume_own_camera(
    net::Vec3 position, net::Vec3 facing, float field80, float field84,
    const entities::PlayerValues& values) noexcept {
    const float back_distance = static_cast<float>(values.Field78) / 4096.0F;
    const float eye_height = static_cast<float>(values.Field80) / 4096.0F;
    if (type_ == CameraType::Third1) {
        // Third1 backs along the player's cached horizontal pair and only then
        // raises the target, so the camera stays level with where they are
        // looking rather than tilting.
        info_.target = position;
        info_.position = info_.target;
        info_.position.x -= field80 * back_distance;
        info_.position.z -= field84 * back_distance;
        saved_position_ = info_.position;
        info_.target.y += eye_height;
    } else {
        // Everything else backs straight along the facing vector from a point
        // above the alt-form collision centre.
        const float alt_y = static_cast<float>(values.AltColYPos) / 4096.0F;
        info_.target = {position.x, position.y + eye_height + alt_y,
                        position.z};
        info_.position = {
            info_.target.x - facing.x * back_distance,
            info_.target.y - facing.y * back_distance,
            info_.target.z - facing.z * back_distance
        };
        saved_position_ = info_.position;
    }
}

} // namespace fruityprime::players
