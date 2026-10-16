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
            if (fromLength <= 0 || toLength <= 0) return up;
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
        const auto shift = player->ModFrameFacingShift();
        if (!shift) return view;
        // The camera looks along the aim; its target carries the tilt and
        // shake the step gave it, so it moves by what the aim has moved by.
        // Up is the camera's own, not TrueUp: CameraInfo::Update makes TrueUp
        // perpendicular to the step's sight line, which near straight up or
        // down is that sight line's horizontal heading -- kept while the aim
        // turned, it rolled the picture until the next step put it back.
        // LookAt makes from UpVector the up the step makes for this target,
        // once its strafing lean is turned with the heading too.
        return Moved(camera, Vector3{}, *shift);
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
