#include "AimAssist.hpp"

#include "AimAssistMath.hpp"

#include <algorithm>
#include <cmath>

namespace MphRead::Mods::Input::AimAssist
{
    using System::Numerics::Vector2;

    AimAssistResult AimAssist::Apply(AimAssistState& state, std::span<const AimAssistTarget> targets,
        Vector2 raw, float stickIntent, float moveIntent, float dt, bool eligible, const AimAssistWeaponProfile& profile)
    {
        if (!AimAssistMath::Finite(raw))
        {
            raw = Vector2::Zero();
        }
        if (!eligible || !AimAssistMath::Finite(raw) || !std::isfinite(dt) || dt <= 0 || dt > .1F)
        {
            state.Reset();
            return {raw.X, raw.Y};
        }
        const float intent = stickIntent > .08F ? 1 : moveIntent > .20F ? .5F : 0;
        if (intent == 0)
        {
            state.Reset();
            return {raw.X, raw.Y};
        }
        std::int32_t best = -1;
        std::int32_t retained = -1;
        float bestScore = -1;
        float retainedScore = -1;
        for (std::size_t i = 0; i < targets.size(); i++)
        {
            const AimAssistTarget& t = targets[i];
            const bool keep = t.Slot == state.TargetSlot && t.Life == state.TargetLife;
            const float rangeScale = 1 - .4F * AimAssistMath::Smooth(25, 60, t.Distance);
            const float cone = (keep ? profile.ReleaseCone : profile.Cone) * rangeScale;
            const float angle = t.BodyError.Length();
            if (!t.Eligible || !t.BodyVisible || !AimAssistMath::Finite(t.BodyError)
                || !std::isfinite(t.Distance) || t.Distance < .2F || t.Distance > 60 || angle > cone)
            {
                continue;
            }
            const float score = AimAssistMath::Score(angle, cone, t.Distance, keep,
                keep ? std::min(state.AngularVelocity.Length() / 45, 1.0F) : 0);
            if (keep)
            {
                retained = static_cast<std::int32_t>(i);
                retainedScore = score;
            }
            if (score > bestScore)
            {
                best = static_cast<std::int32_t>(i);
                bestScore = score;
            }
        }
        if (best < 0)
        {
            state.Reset();
            return {raw.X, raw.Y};
        }
        const bool deliberate = raw.Length() / dt > 90;
        if (retained >= 0 && best != retained
            && bestScore < retainedScore * (deliberate ? 1 : AimAssistTuning::ChallengerRatio))
        {
            best = retained;
        }
        const AimAssistTarget& target = targets[static_cast<std::size_t>(best)];
        const bool same = state.TargetSlot == target.Slot && state.TargetLife == target.Life;
        if (!same)
        {
            state.Reset();
        }
        state.TargetSlot = target.Slot;
        state.TargetLife = target.Life;
        state.RetainedSeconds += dt;
        Vector2 velocity = same ? (target.BodyError - state.PreviousError + state.PreviousOutput) / dt : Vector2{};
        velocity = Vector2::Clamp(velocity, Vector2(-120), Vector2(120));
        state.AngularVelocity = Vector2::Lerp(state.AngularVelocity, velocity, 1 - std::exp(-12 * dt));
        const float headAngle = target.HeadError.Length();
        const bool head = profile.Head && same && state.RetainedSeconds >= AimAssistTuning::HeadDelay
            && target.HeadVisible && AimAssistMath::Finite(target.HeadError) && target.Distance > 5
            && (headAngle < target.BodyError.Length() * .8F || raw.Y > .02F)
            && headAngle < 1.5F && raw.Y >= -.02F && AimAssistMath::Opposition(raw.X, target.HeadError.X) > .5F;
        const float desiredHead = head ? std::min(.8F, .1F + .7F * AimAssistMath::Smooth(1.5F, 0, headAngle)) : 0;
        state.HeadBlend = head ? state.HeadBlend + (desiredHead - state.HeadBlend) * (1 - std::exp(-8 * dt)) : 0;
        Vector2 error = Vector2::Lerp(target.BodyError, target.HeadError, state.HeadBlend);
        if (!AimAssistMath::Finite(error))
        {
            error = target.BodyError;
        }
        const float distanceStrength = (.55F + .45F * AimAssistMath::Smooth(0, 5, target.Distance))
            * (1 - .5F * AimAssistMath::Smooth(25, 60, target.Distance));
        const float bubble = 1 - AimAssistMath::Smooth(profile.Inner, profile.ReleaseCone, target.BodyError.Length());
        const float opposeX = AimAssistMath::Opposition(raw.X / (dt * 60), error.X);
        const float opposeY = AimAssistMath::Opposition(raw.Y / (dt * 60), error.Y);
        const float friction = 1 - .38F * bubble * distanceStrength;
        const Vector2 adjusted(raw.X * (1 - (1 - friction) * opposeX), raw.Y * (1 - (1 - friction) * opposeY));
        const float strength = intent * distanceStrength * bubble * profile.Rotation;
        Vector2 rotation((error.X * 4 + state.AngularVelocity.X) * .24F * opposeX,
            (error.Y * 4 + state.AngularVelocity.Y) * .15F * opposeY);
        rotation = Vector2::Clamp(rotation, Vector2(-profile.MaxSpeed), Vector2(profile.MaxSpeed)) * (strength * dt);
        rotation.X = std::clamp(rotation.X, -std::abs(error.X), std::abs(error.X));
        rotation.Y = std::clamp(rotation.Y, -std::abs(error.Y), std::abs(error.Y));
        const Vector2 output = adjusted + rotation;
        state.PreviousError = target.BodyError;
        state.PreviousOutput = output;
        return {output.X, output.Y, target.Slot, friction, strength,
            state.HeadBlend > 0 ? AimAssistPointType::Head : target.BodyPointType, state.HeadBlend,
            best == retained ? retainedScore : bestScore};
    }
}
