#include "AimAssistTarget.hpp"

namespace MphRead::Mods::Input::AimAssist
{
    const char* ToString(AimAssistPointType value) noexcept
    {
        switch (value)
        {
        case AimAssistPointType::CenterMass: return "CenterMass";
        case AimAssistPointType::UpperChest: return "UpperChest";
        case AimAssistPointType::Head: return "Head";
        }
        return "";
    }
}
