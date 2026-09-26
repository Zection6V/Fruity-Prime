#pragma once

#include <cstdint>
#include <span>

namespace MphRead::Mods::Input
{
    class WeaponWheel final
    {
    public:
        WeaponWheel() = delete;

        [[nodiscard]] static bool Absolute();
        inline static constexpr std::int32_t Slots = 6;
        [[nodiscard]] static std::int32_t Selection() noexcept { return _open ? _index : -1; }
        static void Close() noexcept;
        static std::int32_t Drag(float deltaY, float step, std::span<const bool> available, std::int32_t current) noexcept;

    private:
        [[nodiscard]] static std::int32_t Step(std::int32_t from, std::int32_t direction, std::span<const bool> available) noexcept;
        [[nodiscard]] static bool Has(std::span<const bool> available, std::int32_t index) noexcept;

        inline static float _travel = 0;
        inline static std::int32_t _index = -1;
        inline static bool _open = false;
    };
}
