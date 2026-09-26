#include "Radar.hpp"

#include "../../Formats/Enums.hpp"

namespace MphRead::Mods::Render
{
    using OpenTK::Mathematics::Vector4;

    const Radar::Palette Radar::PaletteOf{
        Vector4(0.05F, 0.08F, 0.1F, 0.5F),
        Vector4(0.6F, 0.85F, 0.9F, 1.0F),
        Vector4(0.6F, 0.85F, 0.9F, 1.0F),
        Vector4(0.92F, 0.94F, 0.98F, 1.0F),
        Vector4(0.35F, 0.95F, 0.35F, 1.0F),
        Vector4(1.0F, 0.65F, 0.2F, 1.0F),
        Vector4(1.0F, 0.4F, 0.8F, 1.0F)};

    bool Radar::IsWeaponItem(ItemType type) noexcept
    {
        switch (type)
        {
        case ItemType::VoltDriver:
        case ItemType::Battlehammer:
        case ItemType::Imperialist:
        case ItemType::Judicator:
        case ItemType::Magmaul:
        case ItemType::ShockCoil:
        case ItemType::OmegaCannon:
        case ItemType::AffinityWeapon:
        case ItemType::PickWpnMissile:
        case ItemType::MissileExpansion:
        case ItemType::UASmall:
        case ItemType::UABig:
        case ItemType::MissileSmall:
        case ItemType::MissileBig:
        case ItemType::UAExpansion:
            return true;
        default:
            return false;
        }
    }
}
