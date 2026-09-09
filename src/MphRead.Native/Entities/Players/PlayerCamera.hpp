#pragma once

#include "Entities/entity_records.hpp"
#include "Mods/Network/net_protocol.hpp"
#include "player_profile.hpp"

#include <cstdint>
#include <functional>
#include <optional>

namespace fruityprime::players {

enum class CameraType : std::uint8_t {
    First,
    Third1,
    Third2,
    Free,
    Spectator
};

struct CameraInfo {
    net::Vec3 position;
    net::Vec3 previous_position;
    net::Vec3 target;
    net::Vec3 up{0.0F, 1.0F, 0.0F};
    net::Vec3 true_up{};
    net::Vec3 facing{};
    float fov_degrees = 78.0F;
    float shake = 0.0F;
    // CameraInfo.Field48/4C/50/54: the horizontal basis the camera
    // was last built from -- the cosine and sine of its yaw, and the
    // same pair one frame behind.  The cartridge never named them,
    // and the match-end camera is the one place they are read back.
    float field48 = 0.0F;
    float field4c = 0.0F;
    float field50 = 0.0F;
    float field54 = 0.0F;

    void reset() noexcept;
    void update() noexcept;
    void set_shake(float value) noexcept;

private:
    bool shake_frame_ = true;
};

// What SetUpMatchEndCamera hands back to the player it belongs to.
// The managed code writes these straight into the player's own
// fields; returning them keeps the camera from having to reach into
// a PlayerEntity.
struct MatchEndBasis {
    float field70 = 0.0F;
    float field74 = 0.0F;
    net::Vec3 gun_vec2{};
    net::Vec3 facing{};
};

// What UpdateMatchEndCamera reads about the player it is orbiting.
struct MatchEndWinner {
    net::Vec3 position{};
    net::Vec3 facing{0.0F, 0.0F, 1.0F};
    // Alt form, morphing and unmorphing are one case: the camera
    // pulls back and up rather than sitting over a shoulder.
    bool alt_form = false;
    float field70 = 0.0F;
    float field74 = 0.0F;
    net::Vec3 gun_vec2{};
};

// A line-of-sight test for the match-end camera.  True when
// something is in the way; `distance` is the fraction of the line
// walked before the hit and `plane` the surface normal, matching
// CollisionResult.
using CameraBlockedQuery = std::function<bool(
    net::Vec3 from, net::Vec3 to, float& distance,
    net::Vec3& plane)>;

// Native counterpart of PlayerCamera.cs.  This is the renderer-independent
// camera state consumed by the desktop host and the headless tests.
class PlayerCamera final {
public:
    void reset() noexcept;
    void switch_camera(CameraType type, net::Vec3 facing) noexcept;
    void update(const net::PlayerState& player, const Profile& profile,
                float frame_seconds, bool camera_sequence_active = false,
                std::optional<net::Vec3> morph_camera_position = std::nullopt)
        noexcept;
    // Recompute Field48/4C/50/54 from the current target.
    void update_horizontal_basis() noexcept;
    void rotate(float horizontal_radians, float vertical_radians) noexcept;

    // PlayerCamera.SetUpMatchEndCamera: the camera's cached basis becomes
    // the player's, so the match-end orbit starts from where they were
    // looking rather than snapping.
    [[nodiscard]] MatchEndBasis set_up_match_end_camera() const noexcept;

    // PlayerCamera.UpdateMatchEndCamera: a third-person orbit of the
    // winner that drifts sideways as the seconds pass, pulled in to the
    // first wall it would otherwise pass through.
    void update_match_end_camera(
        const MatchEndWinner& winner, float time_since_match_end,
        const entities::PlayerValues& values,
        const CameraBlockedQuery& blocked = {}) noexcept;

    // PlayerCamera.RefreshExternalCamera: a camera driven from outside
    // (a sequence, a morph camera) has just taken over, so the alt-form
    // direction is held where it is and the morph blend restarts.
    void refresh_external_camera() noexcept;
    [[nodiscard]] bool alt_dir_override() const noexcept {
        return alt_dir_override_;
    }
    [[nodiscard]] float time_since_morph_camera() const noexcept {
        return time_since_morph_camera_;
    }

    // PlayerCamera.ResumeOwnCamera: put the camera back on the player it
    // belongs to.  `field80`/`field84` are the player's own horizontal
    // facing pair, which the third-person branch backs along.
    void resume_own_camera(net::Vec3 position, net::Vec3 facing,
                           float field80, float field84,
                           const entities::PlayerValues& values) noexcept;
    [[nodiscard]] net::Vec3 saved_position() const noexcept {
        return saved_position_;
    }

    [[nodiscard]] CameraType type() const noexcept { return type_; }
    [[nodiscard]] CameraInfo& info() noexcept { return info_; }
    [[nodiscard]] const CameraInfo& info() const noexcept { return info_; }

private:
    CameraType type_ = CameraType::First;
    CameraInfo info_;
    net::Vec3 saved_position_;
    float switch_seconds_ = 0.0F;
    // PlayerFlags1.AltDirOverride, kept here because the camera is what
    // sets and clears it.
    bool alt_dir_override_ = false;
    float time_since_morph_camera_ = 0.0F;
};

} // namespace fruityprime::players
