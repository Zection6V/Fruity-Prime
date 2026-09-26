#pragma once

#include <cstdint>
#include <utility>

namespace MphRead::Mods::Input
{
    class WeaponSelectionDirection final
    {
    public:
        WeaponSelectionDirection() = delete;

        [[nodiscard]] static std::int32_t Resolve(float x, float y) noexcept;
        [[nodiscard]] static std::int32_t ControllerSlot(float x, float y);
        [[nodiscard]] static std::pair<float, float> FromStick(float x, float y) noexcept;
    };
}
