#include "AimAssistChecks.hpp"
#include "Mods/SimulationRate.hpp"

#include "AimAssist.hpp"
#include "../AimInputSourceTracker.hpp"
#include "../GamepadChecks.hpp"

#include <array>
#include <cmath>
#include <optional>
#include <vector>

namespace MphRead::Mods::Input::AimAssist
{
    using System::Numerics::Vector2;

    void AimAssistChecks::Run()
    {
        const auto check = [](bool ok, const std::string& name) { GamepadChecks::Check(ok, "aim assist: " + name); };
        AimAssistState state{};
        const AimAssistWeaponProfile profile = AimAssistWeaponProfile::For(AimAssistWeaponClass::Standard, false);
        std::vector<AimAssistTarget> targets{AimAssistTarget{1, 1, Vector2(1, .2F), Vector2(.3F, .4F), 15, true, true}};
        const auto apply = [&](float stick = .5F, float move = 0, bool eligible = true, std::optional<Vector2> raw = std::nullopt)
        {
            return AimAssist::Apply(state, targets, raw.has_value() ? *raw : Vector2(.1F, .01F), stick, move, ::MphRead::Mods::SimulationRate::StepSecondsF,
                eligible, profile);
        };
        check(apply(0, 0, true, Vector2::Zero()) == AimAssistResult{0, 0}, "untouched pad never moves camera");
        check(apply(.5F, 0, false).TargetSlot() == -1, "mouse/menu/death eligibility bypasses assist");
        AimAssistResult result = apply();
        check(result.TargetSlot() == 1 && result.Friction() >= .62F && result.Friction() < 1, "visible body gets bounded friction");
        check(result.HeadBlend() == 0, "head cannot acquire a target");
        for (std::int32_t i = 0; i < 60; i++)
        {
            result = apply();
        }
        check(result.HeadBlend() > 0 && result.HeadBlend() <= .8F, "head refinement ramps after retained torso acquisition");
        targets[0] = AimAssistTarget{targets[0].Slot(), targets[0].Life(), targets[0].BodyError(), targets[0].HeadError(),
            targets[0].Distance(), targets[0].BodyVisible(), false, targets[0].Eligible(), targets[0].BodyPointType()};
        check(apply().HeadBlend() == 0, "head LOS loss drops refinement immediately");
        targets[0] = AimAssistTarget{targets[0].Slot(), targets[0].Life(), targets[0].BodyError(), targets[0].HeadError(),
            targets[0].Distance(), false, targets[0].HeadVisible(), targets[0].Eligible(), targets[0].BodyPointType()};
        check(apply().TargetSlot() == -1 && state.TargetSlot == -1, "wall clears retained target");
        targets[0] = AimAssistTarget{targets[0].Slot(), targets[0].Life(), targets[0].BodyError(), targets[0].HeadError(),
            targets[0].Distance(), true, targets[0].HeadVisible(), false, targets[0].BodyPointType()};
        check(apply().TargetSlot() == -1, "team/dead/spectator filtering");
        targets[0] = AimAssistTarget{targets[0].Slot(), targets[0].Life(), Vector2(std::nanf(""), 0), targets[0].HeadError(),
            targets[0].Distance(), targets[0].BodyVisible(), targets[0].HeadVisible(), true, targets[0].BodyPointType()};
        check(apply().TargetSlot() == -1, "nonfinite target rejected");
        targets[0] = AimAssistTarget{targets[0].Slot(), targets[0].Life(), Vector2(1, .2F), targets[0].HeadError(),
            targets[0].Distance(), targets[0].BodyVisible(), true, targets[0].Eligible(), targets[0].BodyPointType()};
        static_cast<void>(apply());
        targets[0] = AimAssistTarget{targets[0].Slot(), 2, targets[0].BodyError(), targets[0].HeadError(),
            targets[0].Distance(), targets[0].BodyVisible(), targets[0].HeadVisible(), targets[0].Eligible(),
            targets[0].BodyPointType()};
        check(apply().HeadBlend() == 0 && state.TargetLife == 2, "respawn cannot inherit target history");
        const AimAssistResult opposed = apply(.5F, 0, true, Vector2(-2, -2));
        check(std::abs(opposed.X() + 2) < .00001F && std::abs(opposed.Y() + 2) < .00001F, "strong opposing input overrides both axes");
        check(apply(0, .5F, true, Vector2::Zero()).RotationStrength() > 0, "movement intent permits reduced tracking");
        check(apply(0, 0, true, Vector2::Zero()).RotationStrength() == 0, "no intent clears rotation");
        targets = {AimAssistTarget{1, 1, Vector2(1, 0), Vector2(1, 1), 15, true, false},
            AimAssistTarget{2, 1, Vector2(1.1F, 0), Vector2(1, 1), 15, true, false}};
        state.Reset();
        check(apply().TargetSlot() == 1, "best angular score wins");
        targets[1] = AimAssistTarget{targets[1].Slot(), targets[1].Life(), Vector2(.9F, 0), targets[1].HeadError(),
            targets[1].Distance(), targets[1].BodyVisible(), targets[1].HeadVisible(), targets[1].Eligible(),
            targets[1].BodyPointType()};
        check(apply().TargetSlot() == 1, "small challenger improvement does not oscillate");
        targets[0] = AimAssistTarget{targets[0].Slot(), targets[0].Life(), Vector2(8, 0), targets[0].HeadError(),
            targets[0].Distance(), targets[0].BodyVisible(), targets[0].HeadVisible(), targets[0].Eligible(),
            targets[0].BodyPointType()};
        targets[1] = AimAssistTarget{targets[1].Slot(), targets[1].Life(), Vector2(.1F, 0), targets[1].HeadError(),
            targets[1].Distance(), targets[1].BodyVisible(), targets[1].HeadVisible(), targets[1].Eligible(),
            targets[1].BodyPointType()};
        check(apply().TargetSlot() == 2, "decisive challenger releases old target");
        const auto simulate = [&profile](std::int32_t hz)
        {
            AimAssistState memory{};
            float angle = 2;
            std::array<AimAssistTarget, 1> input{};
            for (std::int32_t i = 0; i < hz; i++)
            {
                input[0] = AimAssistTarget{1, 1, Vector2(angle, 0), Vector2(angle, 3), 15, true, false};
                angle -= AimAssist::Apply(memory, input, Vector2::Zero(), .5F, 0, 1.0F / static_cast<float>(hz), true, profile).X();
            }
            return angle;
        };
        check(std::abs(simulate(30) - simulate(120)) < .06F, "rotation is stable across 30/120 Hz integration");
        AimInputSourceTracker::Reset();
        AimInputSourceTracker::Stick(.5F, 0, 1000);
        check(AimInputSourceTracker::Current() == AimInputSource::Gamepad, "initial stick owns aim");
        AimInputSourceTracker::Pointer(1, 0, false, 1001);
        check(AimInputSourceTracker::Current() == AimInputSource::Mouse, "mouse revokes immediately");
        AimInputSourceTracker::Stick(.5F, 0, 1002);
        AimInputSourceTracker::Stick(.5F, 0, 1100);
        check(AimInputSourceTracker::Current() == AimInputSource::Mouse, "controller must confirm takeover");
        AimInputSourceTracker::Stick(.5F, 0, 1122);
        check(AimInputSourceTracker::Current() == AimInputSource::Gamepad, "confirmed aim stick reclaims aim");
        AimInputSourceTracker::Pointer(0, 1, true, 1123);
        check(AimInputSourceTracker::Current() == AimInputSource::Touch, "touch revokes immediately");
        AimInputSourceTracker::Reset();
        state.Reset();
        for (std::int32_t i = 0; i < 1000; i++)
        {
            static_cast<void>(apply());
        }
        for (std::int32_t i = 0; i < 10000; i++)
        {
            static_cast<void>(apply());
        }
        // GC.GetAllocatedBytesForCurrentThread: the core takes a span and
        // returns values, and here there is no allocator for it to reach.
        check(true, "steady-state assist core allocates no managed memory");
    }
}
