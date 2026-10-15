#pragma once

#include "../../NativeRuntime/OpenTK/Mathematics.hpp"

#include <cstdint>

namespace MphRead::Entities
{
    class PlayerEntity;
    class CameraInfo;
}

namespace MphRead::Mods::Input
{
    // What the frames since the last simulation step have already turned the
    // local aim by, and against which mouse snapshot. Kept on the player.
    struct FrameAimState final
    {
        bool Active = false;
        // The simulation step the frames belong to: any step since ends them.
        double Step = 0;
        // The snapshot the last step consumed, which every delta here and the
        // next step's own delta are measured from.
        float BaselineX = 0;
        float BaselineY = 0;
        std::uint64_t BaselineEpoch = 0;
        // Mouse motion since that snapshot already applied to the aim.
        float AppliedX = 0;
        float AppliedY = 0;
        // The aim the last step's camera was built from.
        OpenTK::Mathematics::Vector3 CameraFacing{};
    };

    // Mouse look at the display's rate, for real. The mouse is read once a
    // frame at one fixed instant (the frame's input pump, after the latency
    // wait), and the motion since the last application turns the actual aim
    // there and then, through the game's own UpdateAimY/UpdateAimX; the gun,
    // muzzle and effects are rebuilt from it the way a step builds them. A
    // simulation step then applies only what the frames have not, so the
    // total turn is the mouse's and the step fires along the aim on screen.
    //
    // Only where turning in pieces is the same as turning at once: first
    // person, mouse aim, the game's exact rules and a fixed crosshair (Pro
    // mode HUD). The DS-style drift eases the view toward the aim on every
    // call, and calling it at the display's rate would change how it feels.
    class FrameAim final
    {
    public:
        FrameAim() = delete;

        [[nodiscard]] static bool Enabled() noexcept;
        static void Enabled(bool value) noexcept;

        // Hook: Scene::TransformCamera, player camera. The last step built the
        // camera; this turns it to the aim the frames have reached since.
        [[nodiscard]] static OpenTK::Mathematics::Matrix4 View(const Entities::PlayerEntity* player,
            const Entities::CameraInfo& camera, OpenTK::Mathematics::Matrix4 view);
    };
}
