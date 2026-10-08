#pragma once
#include "AimTrace.hpp"
namespace MphRead::Mods::Input
{
    struct AimFrame final
    {
        AimOwner Owner = AimOwner::Local;
        AimSource Source = AimSource::None;
        bool Native = false;
        bool Exact = true;
        float Yaw = 0, Pitch = 0; // selected source angles; also drive turn animation
    };
}
