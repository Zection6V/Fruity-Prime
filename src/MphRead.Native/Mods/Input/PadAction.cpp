#include "PadAction.hpp"

#include "../../NativeRuntime/System/Enum.hpp"

namespace MphRead::Mods::Input
{
    namespace
    {
        constexpr ::MphRead::NativeRuntime::EnumNameEntry Names[] = {
            {0, "Shoot"}, {1, "Zoom"}, {2, "Jump"}, {3, "Morph"}, {4, "Scan"}, {5, "ScanVisor"},
            {6, "Scoreboard"}, {7, "NextWeapon"}, {8, "PrevWeapon"}, {9, "Missile"}, {10, "PowerBeam"},
            {11, "Menu"}, {12, "Chat"}, {13, "WeaponWheel"}, {14, "VoltDriver"}, {15, "Battlehammer"},
            {16, "Imperialist"}, {17, "Judicator"}, {18, "Magmaul"}, {19, "ShockCoil"}, {20, "OmegaCannon"},
            {21, "AffinitySlot"}, {22, "LastWeapon"}};
    }

    std::string ToString(PadAction value)
    {
        return ::MphRead::NativeRuntime::ManagedEnumToString(value, Names, std::size(Names), false);
    }

    bool TryParse(std::string_view text, PadAction& value)
    {
        return ::MphRead::NativeRuntime::ManagedEnumTryParse(text, false, Names, std::size(Names), value);
    }
}
