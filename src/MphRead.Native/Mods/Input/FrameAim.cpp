#include "FrameAim.hpp"

#include "../../Entities/Players/PlayerEntity.hpp"

#include <cmath>

namespace MphRead::Mods::Input
{
    namespace
    {
        using OpenTK::Mathematics::Vector3;

        bool enabled = true;

        // UpdateCameraFirst leans the camera's up across the step's heading
        // while strafing (_viewTiltAngleH): the horizontal part of UpVector
        // is that heading's perpendicular. Turn it with the heading, about
        // the vertical, by the turn that takes `from` to `to` on the ground.
        Vector3 TurnedUp(Vector3 up, Vector3 from, Vector3 to)
        {
            const float fromLength = std::sqrt(from.X * from.X + from.Z * from.Z);
            const float toLength = std::sqrt(to.X * to.X + to.Z * to.Z);
            // A sight line all but straight up or down has no heading to speak
            // of: a hair of sideways motion would read as a half turn.
            constexpr float Vertical = 0.05F;
            if (fromLength <= Vertical * std::sqrt(from.LengthSquared())
                || toLength <= Vertical * std::sqrt(to.LengthSquared()))
            {
                return up;
            }
            const float fx = from.X / fromLength, fz = from.Z / fromLength;
            const float tx = to.X / toLength, tz = to.Z / toLength;
            const float c = fx * tx + fz * tz;
            const float s = fx * tz - fz * tx;
            return Vector3(c * up.X - s * up.Z, up.Y, s * up.X + c * up.Z);
        }
    }

    bool FrameAim::Enabled() noexcept
    {
        return enabled;
    }

    void FrameAim::Enabled(bool value) noexcept
    {
        enabled = value;
    }

    OpenTK::Mathematics::Matrix4 FrameAim::View(const Entities::PlayerEntity* player,
        const Entities::CameraInfo& camera, OpenTK::Mathematics::Matrix4 view)
    {
        if (player == nullptr) return view;
        // The camera looks along the aim; its target carries the tilt the
        // step gave it, so it moves by what the aim has moved by. Up is the
        // camera's own, not TrueUp: CameraInfo::Update makes TrueUp
        // perpendicular to the step's sight line, which near straight up or
        // down is that sight line's horizontal heading -- kept while the aim
        // turned, it rolled the picture until the next step put it back.
        // LookAt makes from UpVector the up the step makes for this target,
        // once its strafing lean is turned with the heading too.
        if (const auto shift = player->ModFrameFacingShift()) view = Moved(camera, Vector3{}, *shift);
        if (const auto shake = player->ModFrameShake()) view = Shaken(view, camera, *shake);
        return view;
    }

    OpenTK::Mathematics::Matrix4 FrameAim::Shaken(OpenTK::Mathematics::Matrix4 view,
        const Entities::CameraInfo& camera, OpenTK::Mathematics::Vector3 shake)
    {
        // The finished view's sight line and up (OpenTK row-vector LookAt:
        // forward is the third column negated, up the second). The shake moves
        // the point looked at, as the game's does, while the view keeps an up
        // perpendicular to its unshaken sight line: the picture tilts and does
        // not roll. As a world-space offset on a sight line looking straight
        // down, the game's own shake spins the heading, and the roll with it.
        const Vector3 forward = Vector3(-view.M13, -view.M23, -view.M33).Normalized();
        const Vector3 up(view.M12, view.M22, view.M32);
        const float distance = std::sqrt((camera.Target - camera.Position).LengthSquared());
        const Vector3 eye = camera.Position;
        const Vector3 target = eye + OpenTK::Mathematics::Multiply(forward, distance > 0 ? distance : 1.0F) + shake;
        return OpenTK::Mathematics::Matrix4::LookAt(eye, target, up);
    }

    OpenTK::Mathematics::Matrix4 FrameAim::Moved(const Entities::CameraInfo& camera,
        OpenTK::Mathematics::Vector3 eye, OpenTK::Mathematics::Vector3 turn)
    {
        const Vector3 position = camera.Position + eye;
        const Vector3 target = camera.Target + eye + turn;
        const Vector3 up = TurnedUp(camera.UpVector, camera.Target - camera.Position, target - position);
        return OpenTK::Mathematics::Matrix4::LookAt(position, target, up);
    }

    FrameAimPresentation::FrameAimPresentation(const std::shared_ptr<Entities::PlayerEntity>& player) noexcept
        : _player(player)
    {
    }

    FrameAimPresentation::~FrameAimPresentation()
    {
        if (const auto player = _player.lock()) player->ModRestoreFrameAim();
    }
}
