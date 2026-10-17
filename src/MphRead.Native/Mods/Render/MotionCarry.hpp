#pragma once

#include "../../NativeRuntime/OpenTK/Mathematics.hpp"

#include <array>
#include <functional>
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
        // A step's shift short of a jump: motion that may be carried.
        [[nodiscard]] static bool Within(OpenTK::Mathematics::Vector3 shift) noexcept
        {
            return shift.LengthSquared() < JumpDistance * JumpDistance;
        }
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
        // Between, with the turn on its own fraction of the step's turn: the
        // move carried on by `move`, the pose turned by `turnFraction`, which
        // may be negative (a pose shown at an earlier instant than the step's:
        // -1 is `previous`'s own turn). Nothing when that leaves the pose as is.
        [[nodiscard]] static std::optional<OpenTK::Mathematics::Matrix4> BetweenShown(
            const OpenTK::Mathematics::Matrix4& previous, const OpenTK::Mathematics::Matrix4& current,
            float move, float turnFraction);
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
        // A camera that trails a body, where the last four steps put it
        // against that body (`steps`, oldest first), shown `fraction` of a
        // step on -- axis by axis, since a camera held off a wall moves on
        // one axis while it fights the wall on another:
        //  - going to and fro every step, at the middle of its swing, moving
        //    on as the swing does;
        //  - moving steadily, on by the smaller of its last two steps;
        //  - turning back or changing pace sharply (snapped in off a wall,
        //    then easing out), not carried: on past a snap, the picture dove
        //    through the body and came back.
        [[nodiscard]] static OpenTK::Mathematics::Vector3 Trail(
            const std::array<OpenTK::Mathematics::Vector3, 4>& steps, float fraction) noexcept;
        // A translation alone, when there is one worth carrying.
        [[nodiscard]] static std::optional<OpenTK::Mathematics::Matrix4> By(OpenTK::Mathematics::Vector3 shift);

        // A drawn item, moved by `carry`: its matrix stack when it has one,
        // its transform otherwise -- every kind of item is drawn through one
        // of the two. A billboard keeps facing the camera and only moves.
        static void Apply(RenderItem& item, const OpenTK::Mathematics::Matrix4& carry);
        // Each of the item's matrices by the carry `carryFor` gives that matrix.
        static void Apply(RenderItem& item,
            const std::function<const OpenTK::Mathematics::Matrix4&(const OpenTK::Mathematics::Matrix4& drawn)>& carryFor);
        [[nodiscard]] static OpenTK::Mathematics::Vector3 Point(
            OpenTK::Mathematics::Vector3 point, const OpenTK::Mathematics::Matrix4& carry) noexcept;

    private:
        // `from`, moved by `fraction` of the motion that takes `start` to `end`.
        [[nodiscard]] static std::optional<OpenTK::Mathematics::Matrix4> Along(const OpenTK::Mathematics::Matrix4& from,
            const OpenTK::Mathematics::Matrix4& start, const OpenTK::Mathematics::Matrix4& end, float fraction);
        // Along, with the move and the turn on fractions of their own.
        [[nodiscard]] static std::optional<OpenTK::Mathematics::Matrix4> AlongSplit(
            const OpenTK::Mathematics::Matrix4& from, const OpenTK::Mathematics::Matrix4& start,
            const OpenTK::Mathematics::Matrix4& end, float move, float turnFraction);
    };
}
