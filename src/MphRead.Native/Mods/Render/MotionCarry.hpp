#pragma once

#include "../../NativeRuntime/OpenTK/Mathematics.hpp"

#include <optional>

namespace MphRead
{
    class RenderItem;
}

namespace MphRead::Mods::Render
{
    // A pose carried on from where the last simulation step left it, by
    // that step's own motion, to an instant part of the way to the next
    // step: what a picture between steps shows of anything that moves.
    // Nothing is behind the step and nothing is held back; the next step
    // puts everything where it really goes.
    class MotionCarry final
    {
    public:
        MotionCarry() = delete;

        // Further than this in one step is not motion but a jump -- a
        // teleporter, a respawn, a jump pad's launch frame, a pooled entity
        // starting a new life -- and is never slid across.
        static constexpr float JumpDistance = 4.0F;
        // A turn this large in one step is not carried either.
        static constexpr float MaxTurnDegrees = 90.0F;

        // The world-space motion, row-vector like every engine transform,
        // that takes the pose `current` (one step after `previous`) on by
        // `fraction` of a step more: translation and rotation, the rotation
        // only while the pose keeps its scale. Nothing when there is nothing
        // to carry or the motion is a jump.
        [[nodiscard]] static std::optional<OpenTK::Mathematics::Matrix4> Between(
            const OpenTK::Mathematics::Matrix4& previous, const OpenTK::Mathematics::Matrix4& current,
            float fraction);
        // Part of the way from `current` to `next`, a pose the game itself
        // says comes next: the same carry, toward it rather than on past.
        [[nodiscard]] static std::optional<OpenTK::Mathematics::Matrix4> Toward(
            const OpenTK::Mathematics::Matrix4& current, const OpenTK::Mathematics::Matrix4& next, float fraction);
        // What takes a ray from `from` along `fromDirection` onto one from `to`
        // along `toDirection`: a beam drawn out of a gun, kept on the gun.
        [[nodiscard]] static OpenTK::Mathematics::Matrix4 Anchor(OpenTK::Mathematics::Vector3 from,
            OpenTK::Mathematics::Vector3 fromDirection, OpenTK::Mathematics::Vector3 to,
            OpenTK::Mathematics::Vector3 toDirection);
        // What takes the segment `fromA`-`fromB` onto `toA`-`toB`: the same turn,
        // then a uniform stretch to the new length. A beam held between a gun
        // and what it is locked on, each end where the picture has it.
        [[nodiscard]] static OpenTK::Mathematics::Matrix4 Span(OpenTK::Mathematics::Vector3 fromA,
            OpenTK::Mathematics::Vector3 fromB, OpenTK::Mathematics::Vector3 toA, OpenTK::Mathematics::Vector3 toB);
        // A translation alone, when there is one worth carrying.
        [[nodiscard]] static std::optional<OpenTK::Mathematics::Matrix4> By(OpenTK::Mathematics::Vector3 shift);

        // A drawn item, moved by `carry`: its matrix stack when it has one,
        // its transform otherwise -- every kind of item is drawn through one
        // of the two. A billboard keeps facing the camera and only moves.
        static void Apply(RenderItem& item, const OpenTK::Mathematics::Matrix4& carry);
        [[nodiscard]] static OpenTK::Mathematics::Vector3 Point(
            OpenTK::Mathematics::Vector3 point, const OpenTK::Mathematics::Matrix4& carry) noexcept;

    private:
        // `from`, moved by `fraction` of the motion that takes `start` to `end`.
        [[nodiscard]] static std::optional<OpenTK::Mathematics::Matrix4> Along(const OpenTK::Mathematics::Matrix4& from,
            const OpenTK::Mathematics::Matrix4& start, const OpenTK::Mathematics::Matrix4& end, float fraction);
    };
}
