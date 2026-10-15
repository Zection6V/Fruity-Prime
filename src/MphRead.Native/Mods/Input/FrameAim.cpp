#include "FrameAim.hpp"

#include "../../Entities/Players/PlayerEntity.hpp"

namespace MphRead::Mods::Input
{
    namespace
    {
        bool enabled = true;
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
        // LookAt makes from UpVector the up the step makes for this target.
        return OpenTK::Mathematics::Matrix4::LookAt(camera.Position, camera.Target + *shift, camera.UpVector);
    }
}
