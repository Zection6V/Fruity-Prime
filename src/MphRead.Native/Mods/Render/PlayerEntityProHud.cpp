#include "PlayerEntityProHud.hpp"

#include "../../Entities/Players/PlayerEntity.hpp"
#include "../../Features.hpp"
#include "../../GameState.hpp"
#include "../../Metadata/Metadata.hpp"
#include "../../Scene.hpp"
#include "../../Strings.hpp"
#include "SmoothHudIcon.hpp"
#include "../../NativeRuntime/System/Managed.hpp"

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <memory>
#include <optional>
#include <stdexcept>
#include <string>

using ::MphRead::NativeRuntime::ManagedAt;
using ::MphRead::NativeRuntime::RequireReference;

namespace
{
}


namespace MphRead::Entities
{
    const OpenTK::Mathematics::Vector4 PlayerEntity::ProGood(0.24F, 0.85F, 0.32F, 1.0F);
    const OpenTK::Mathematics::Vector4 PlayerEntity::ProWarn(1.0F, 0.68F, 0.1F, 1.0F);
    const OpenTK::Mathematics::Vector4 PlayerEntity::ProDanger(0.95F, 0.18F, 0.18F, 1.0F);

    const OpenTK::Mathematics::Vector4 PlayerEntity::ProHudPanel(0.0F, 0.0F, 0.0F, 0.5F);
    const OpenTK::Mathematics::Vector4 PlayerEntity::ProHudShade(0.0F, 0.0F, 0.0F, 0.55F);
    const OpenTK::Mathematics::Vector4 PlayerEntity::ProHudTrack(1.0F, 1.0F, 1.0F, 0.16F);
    const ColorRgba PlayerEntity::ProHudInk(235, 238, 245, 255);
    const ColorRgba PlayerEntity::ProHudDim(178, 186, 200, 255);
    const ColorRgba PlayerEntity::ProHudShadow(0, 0, 0, 255);

    void PlayerEntity::DrawProHud()
    {
        const float aspect = HudAspectFix();
        if (ModHudHealthVisible())
        {
            const OpenTK::Mathematics::Vector4 health = ProHealthColor();
            // The left foot of the screen, under the weapon list and the same
            // width as it: score, weapons and energy then read as one column.
            RequireReference(_scene).DrawHudFlatBox(
                2.0F * aspect, 170.0F, 46.0F * aspect, 190.0F, ProHudPanel);
            ProNumber(6.0F * aspect, 172.0F, Hud::Align::Left,
                std::to_string(ModHudHealth()), ProInk(health), 1.5F);
            ProBar(4.0F * aspect, 186.0F, 40.0F, 3.0F, ProHealthFraction(), health);
        }
        DrawProAmmo();
        ProScore(4.0F * aspect, 12.0F, Hud::Align::Left, 1.1F);
    }

    void PlayerEntity::DrawProAmmo()
    {
        const std::optional<std::string> ammo = ProAmmoText();
        if (!ammo.has_value())
        {
            return;
        }
        const float aspect = HudAspectFix();
        const OpenTK::Mathematics::Vector4 color = ProAmmoColor();
        const float right = 256.0F - 2.0F * aspect;
        const float left = right - ProAmmoPanelWidth * aspect;
        RequireReference(_scene).DrawHudFlatBox(left, 170.0F, right, 190.0F, ProHudPanel);
        DrawProAmmoIcon(left + 2.0F * aspect, 172.0F);
        ProNumber(right - 4.0F * aspect, 172.0F, Hud::Align::Right,
            *ammo, ProInk(color), ProAmmoNumberScale);
        ProBar(left + 2.0F * aspect, 186.0F, ProAmmoPanelWidth - 4.0F, 3.0F,
            ProAmmoFraction(), color);
    }

    void PlayerEntity::DrawProAmmoIcon(float x, float y)
    {
        const std::int32_t index = static_cast<std::int32_t>(CurrentWeapon());
        if (index < 0 || static_cast<std::size_t>(index) >= _weaponListIcons.size())
        {
            return;
        }
        const std::shared_ptr<Hud::HudObjectInstance> iconValue
            = _weaponListIcons[static_cast<std::size_t>(index)];
        if (!iconValue)
        {
            return;
        }
        Hud::HudObjectInstance& icon = *iconValue;
        const float side = 8.0F * ProAmmoNumberScale;
        const float aspect = HudAspectFix();
        const IconBounds bounds = ManagedAt(_weaponListIconBounds, index);
        const float scale = side / static_cast<float>(std::max(bounds.Width(), bounds.Height()));
        const auto& weaponColor = ManagedAt(_weaponListColors, index);
        Mods::Render::SmoothHudIcon::Tint(iconValue, _weaponListSheetData, index,
            weaponColor, RequireReference(_scene));
        icon.Alpha = Features::HudOpacity();
        icon.PositionX = (x + side * aspect / 2.0F - bounds.CentreX() * scale * aspect) / 256.0F;
        icon.PositionY = (y + side / 2.0F - bounds.CentreY() * scale) / 192.0F;
        RequireReference(_scene).DrawHudObject(iconValue, 1, scale);
    }

    float PlayerEntity::ProHealthFraction()
    {
        return std::clamp(static_cast<float>(ModHudHealth()) / static_cast<float>(ProHealthSpan()), 0.0F, 1.0F);
    }

    std::int32_t PlayerEntity::ProHealthSpan()
    {
        if (GameState::Multiplayer())
        {
            return std::max(static_cast<std::int32_t>(Values().EnergyTank) - 1, 1);
        }
        return std::max(_healthMax, 1);
    }

    OpenTK::Mathematics::Vector4 PlayerEntity::ProHealthColor()
    {
        const float fraction = ProHealthFraction();
        if (fraction > ProHudWarn)
        {
            return ProGood;
        }
        if (fraction > ProHudDanger)
        {
            return ProWarn;
        }
        return ProDanger;
    }

    ColorRgba PlayerEntity::ProInk(OpenTK::Mathematics::Vector4 color)
    {
        return ColorRgba(
            static_cast<std::uint8_t>(color.X * 255.0F),
            static_cast<std::uint8_t>(color.Y * 255.0F),
            static_cast<std::uint8_t>(color.Z * 255.0F),
            255);
    }

    std::optional<std::string> PlayerEntity::ProAmmoText()
    {
        if (IsAltForm() || IsMorphing() || IsUnmorphing())
        {
            return std::nullopt;
        }
        MphRead::EquipInfo& equipInfo = RequireReference(EquipInfo());
        MphRead::WeaponInfo& info = RequireReference(equipInfo.Weapon);
        if (info.AmmoCost == 0)
        {
            return std::nullopt;
        }
        const std::int32_t amount = ManagedAt(_ammo, static_cast<std::int32_t>(info.AmmoType));
        return amount < 0
            ? std::optional<std::string>("--")
            : std::optional<std::string>(
                std::to_string(amount / static_cast<std::int32_t>(info.AmmoCost)));
    }

    std::int32_t PlayerEntity::ProAmmoFull()
    {
        return GameState::Multiplayer() ? 100 : 250;
    }

    float PlayerEntity::ProAmmoFraction()
    {
        MphRead::EquipInfo& equipInfo = RequireReference(EquipInfo());
        MphRead::WeaponInfo& info = RequireReference(equipInfo.Weapon);
        const std::int32_t amount = ManagedAt(_ammo, static_cast<std::int32_t>(info.AmmoType));
        if (amount < 0)
        {
            return 1.0F;
        }
        return std::clamp(amount / static_cast<float>(ProAmmoFull()), 0.0F, 1.0F);
    }

    OpenTK::Mathematics::Vector4 PlayerEntity::ProAmmoColor()
    {
        MphRead::EquipInfo& equipInfo = RequireReference(EquipInfo());
        MphRead::WeaponInfo& info = RequireReference(equipInfo.Weapon);
        const std::int32_t amount = ManagedAt(_ammo, static_cast<std::int32_t>(info.AmmoType));
        if (amount < 0 || amount >= ProAmmoFull() / 2)
        {
            return ProGood;
        }
        if (amount >= ProAmmoFull() / 5)
        {
            return ProWarn;
        }
        return ProDanger;
    }

    void PlayerEntity::ProBar(float x, float y, float width, float height, float fill,
        OpenTK::Mathematics::Vector4 color)
    {
        const float aspect = HudAspectFix();
        const float pad = 1.0F;
        RequireReference(_scene).DrawHudFlatBox(
            x - pad * aspect, y - pad,
            x + (width + pad) * aspect, y + height + pad, ProHudShade);
        RequireReference(_scene).DrawHudFlatBox(
            x, y, x + width * aspect, y + height, ProHudTrack);
        if (fill > 0.0F)
        {
            RequireReference(_scene).DrawHudFlatBox(
                x, y, x + width * fill * aspect, y + height, color);
        }
    }

    void PlayerEntity::ProNumber(float x, float y, Hud::Align align,
        const std::string& text, ColorRgba color, float scale)
    {
        const float aspect = HudAspectFix();
        static_cast<void>(DrawText2D(
            x + 0.8F * aspect, y + 0.8F, align, 0, text, ProHudShadow,
            1.0F, -1.0F, -1, scale));
        static_cast<void>(DrawText2D(
            x, y, align, 0, text, color, 1.0F, -1.0F, -1, scale));
    }

    void PlayerEntity::ProScore(float x, float y, Hud::Align align, float scale)
    {
        const std::string label = Text::Strings::GetHudMessage(ProScoreMessageId());
        ProNumber(x, y, align, label, ProHudDim, 0.55F);
        ProNumber(x, y + 8.0F, align, FormatModeScore(MainPlayerIndex()), ProHudInk, scale);
    }

    std::int32_t PlayerEntity::ProScoreMessageId()
    {
        switch (GameState::Mode())
        {
        case GameMode::Survival:
        case GameMode::SurvivalTeams:
            return 213;
        case GameMode::PrimeHunter:
            return 214;
        case GameMode::Bounty:
        case GameMode::BountyTeams:
            return 215;
        case GameMode::Capture:
            return 216;
        case GameMode::Defender:
        case GameMode::DefenderTeams:
            return 217;
        case GameMode::Nodes:
        case GameMode::NodesTeams:
            return 218;
        default:
            return 212;
        }
    }
}
