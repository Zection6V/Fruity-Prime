#include "AimCheck.hpp"
#include "../../Entities/Players/PlayerEntity.hpp"
#include "../../Features.hpp"
#include <cmath>
#include <iostream>
#include <stdexcept>

namespace MphRead::Mods::Diagnostics
{
    int AimCheck::CheckModes(Entities::PlayerEntity& player)
    {
        using OpenTK::Mathematics::Vector3;
        int checks = 0;
        const auto check = [&](bool passed, const char* name)
        {
            if (!passed) throw std::runtime_error(name);
            ++checks; std::cout << "AIM MODE PASS " << name << '\n';
        };
        const bool fixed = Features::FixedCrosshair();
        Features::FixedCrosshair(true);
        player._aimFrame = {};
        player._gunVec1 = Vector3(0, 0, 1);
        player._facingVector = Vector3(0.1F, 0, 1).Normalized();
        player.UpdateAimY(0);
        check(OpenTK::Mathematics::Equal(player._gunVec1, player._facingVector), "FixedCrosshair zero Pitch retains immediate follow policy");
        Features::FixedCrosshair(fixed);
        player.Controls().ClearAll(); player.ModForgetInputDeltas();
        player.Controls().SetNativeAim(true);
        player.Controls().NativeControl().Flags = 0x28;
        player.Controls().NativeControl().RomCadence = true;
        player._timeSinceInput = 0; player._aimY = 10;
        player.PrepareAimInput(); player._aimTrace.Clear(); player.ApplyLocalAim(false);
        check(player._aimTrace.Get(Input::AimOperation::Pitch) == 2 && player._aimTrace.Get(Input::AimOperation::Yaw) == 1,
            "Native AutoPitch is an intentional additional consumer");
        player.Controls().NativeControl().Flags |= 0x40;
        player._aimTrace.Clear(); player.ApplyLocalAim(false);
        check(player._aimTrace.Get(Input::AimOperation::Pitch) == 1,
            "Control40 inhibits only additional AutoPitch");
        player._flags1 |= Entities::PlayerFlags1::NoAimInput;
        player._nativeDual.X = 1; player._nativeDual.Y = 2;
        player.Controls().NativeControl().Flag84E = 1;
        player.PrepareAimInput(); player._aimTrace.Clear(); player.ApplyLocalAim(true);
        check(player._aimTrace.Get(Input::AimOperation::Pitch) == 1 && player._aimTrace.Get(Input::AimOperation::Yaw) == 1
            && player._nativeDual.X == 1 && player._nativeDual.Y == 2,
            "NoAimInput and84E retain old Native Dual consumers in Alt driver");
        player.Controls().NativeControl().Flags = 0x22;
        player.PrepareAimInput(); player._aimTrace.Clear(); player.ApplyLocalAim(false);
        check(player._aimTrace.Get(Input::AimOperation::Pitch) == 0 && player._aimTrace.Get(Input::AimOperation::Yaw) == 0,
            "NoAimInput blocks Touch independently of Dual policy");
        player._flags1 &= ~Entities::PlayerFlags1::NoAimInput;
        player.Controls().SetNativeAim(false); player.Controls().NativeControl() = {};
        player._nativeInputSlot.Produce(0x15, true, 0);
        player._nativeInputShadow = player._nativeInputSlot;
        const auto producer = player._nativeInputSlot.Bytes;
        player.ModForgetInputDeltas(); player._aimFrame = {};
        check(player._nativeInputSlot.Bytes == producer && player._nativeInputShadow.Read(0) == 0,
            "suspending a player clears only the copied input shadow, preserving producer history");
        return checks;
    }
}
