#pragma once

#include "../../NativeRuntime/OpenTK/Mathematics.hpp"

#include <cstdint>

namespace MphRead
{
    enum class ItemType : std::int32_t;
}

namespace MphRead::Mods::Render
{
    // The round motion-tracker overlay's settings and colours. Nothing here is
    // cut from a DS sprite: every shape is drawn flat.
    class Radar final
    {
    public:
        Radar() = delete;

        inline static bool Enabled = true;
        inline static bool ShowBackground = false;
        inline static bool ShowOutlines = true;
        static constexpr float Range = 24;

        [[nodiscard]] static bool IsWeaponItem(ItemType type) noexcept;

        struct Palette
        {
            OpenTK::Mathematics::Vector4 Background;
            OpenTK::Mathematics::Vector4 Ring;
            OpenTK::Mathematics::Vector4 Cone;
            OpenTK::Mathematics::Vector4 Player;
            OpenTK::Mathematics::Vector4 Hunter;
            OpenTK::Mathematics::Vector4 Weapon;
            OpenTK::Mathematics::Vector4 Powerup;
        };

        static const Palette PaletteOf;
    };
}
