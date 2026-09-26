#pragma once

#include <cstdint>
#include <string>
#include <string_view>

namespace MphRead::Mods::Input
{
    enum class PadAction : std::int32_t
    {
        Shoot,
        Zoom,
        Jump,
        Morph,
        Scan,
        ScanVisor,
        Scoreboard,
        NextWeapon,
        PrevWeapon,
        Missile,
        PowerBeam,
        Menu,
        Chat,
        WeaponWheel,
        VoltDriver, Battlehammer, Imperialist, Judicator, Magmaul, ShockCoil,
        OmegaCannon, AffinitySlot, LastWeapon
    };

    inline constexpr std::int32_t PadActionCount = 23;

    // PadAction.ToString().
    [[nodiscard]] std::string ToString(PadAction value);
    // Enum.TryParse<PadAction>(text, out value).
    [[nodiscard]] bool TryParse(std::string_view text, PadAction& value);
    // Enum.IsDefined(value).
    [[nodiscard]] constexpr bool IsDefined(PadAction value) noexcept
    {
        return static_cast<std::int32_t>(value) >= 0 && static_cast<std::int32_t>(value) < PadActionCount;
    }
}
