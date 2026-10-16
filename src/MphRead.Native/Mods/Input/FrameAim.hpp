#pragma once

#include "../../NativeRuntime/OpenTK/Mathematics.hpp"
#include "AimTrace.hpp"

#include <array>
#include <cstdint>
#include <memory>

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

    // The DS-style aim as the last step left it, while one picture shows it
    // turned by the mouse motion the next step has not consumed yet.
    struct FrameAimView final
    {
        bool Active = false;
        OpenTK::Mathematics::Vector3 Gun{}, Facing{}, AimPosition{}, GunSide{}, Up{};
        OpenTK::Mathematics::Vector3 GunDrawPos{}, AimVec{}, Muzzle{};
        float AimY = 0;
        std::array<std::uint32_t, static_cast<std::size_t>(AimOperation::Count)> Trace{};
        // What the picture shows, so a respawn or a room load during it is
        // recognised and its own aim kept rather than overwritten.
        OpenTK::Mathematics::Vector3 ShownGun{}, ShownFacing{};
        bool Reticle = false;
        float ReticleX = 0, ReticleY = 0;
        // A watched player's camera, moved to this picture's instant.
        bool Camera = false;
        OpenTK::Mathematics::Vector3 CameraPosition{}, CameraTarget{}, CameraFacing{}, CameraTrueUp{};
        OpenTK::Mathematics::Matrix4 CameraView{};
    };

    // Where a watched player stood and looked when the step built their
    // camera, before the snapshot moved them on (NetPlayerBridge::ApplyState).
    struct CameraSource final
    {
        bool Valid = false;
        double Step = 0;
        OpenTK::Mathematics::Vector3 Position{}, Facing{};
    };

    // Mouse look at the display's rate, for real. The mouse is read once a
    // frame at one fixed instant (the frame's input pump, after the latency
    // wait), and the motion since the last application turns the actual aim
    // there and then, through the game's own UpdateAimY/UpdateAimX; the gun,
    // muzzle and effects are rebuilt from it the way a step builds them. A
    // simulation step then applies only what the frames have not, so the
    // total turn is the mouse's and the step fires along the aim on screen.
    //
    // That is only where turning in pieces is the same as turning at once:
    // first person, mouse aim, the game's exact rules and a fixed crosshair
    // (Pro mode HUD). The DS-style crosshair is presented instead: the view
    // eases toward the aim on every step's turn, so turning it in pieces
    // would change how it feels. Each picture shows the aim the next step
    // will turn to with the motion so far, and the view eased toward it by
    // the time this picture is into that step -- the same ease at any rate,
    // nothing behind the mouse -- and the step's state is put back once the
    // picture is drawn, so every step runs on exactly what it always did.
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
        // The step's camera with its eye moved and its sight line turned by
        // these, its tilt, shake and strafing lean carried along.
        [[nodiscard]] static OpenTK::Mathematics::Matrix4 Moved(const Entities::CameraInfo& camera,
            OpenTK::Mathematics::Vector3 eye, OpenTK::Mathematics::Vector3 turn);
    };

    // One picture's DS-style aim or watched view: the player shows it while
    // this lives, and has the step's state back when it ends.
    class FrameAimPresentation final
    {
    public:
        FrameAimPresentation() = default;
        explicit FrameAimPresentation(const std::shared_ptr<Entities::PlayerEntity>& player) noexcept;
        ~FrameAimPresentation();
        FrameAimPresentation(const FrameAimPresentation&) = delete;
        FrameAimPresentation& operator=(const FrameAimPresentation&) = delete;
        FrameAimPresentation(FrameAimPresentation&&) noexcept = default;
        FrameAimPresentation& operator=(FrameAimPresentation&&) = delete;

    private:
        std::weak_ptr<Entities::PlayerEntity> _player;
    };
}
