#include "PlayerEntityAmmoClear.hpp"

#include "../../HUD/HudInfo.hpp"
#include "../../Strings.hpp"
#include "../../NativeRuntime/System/Managed.hpp"

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <memory>
#include <stdexcept>

using ::MphRead::NativeRuntime::ManagedAt;
using ::MphRead::NativeRuntime::RequireReference;

namespace
{
}


namespace MphRead::Entities
{
    float PlayerEntity::ModAmmoTextX(float x, float y, MphRead::Hud::Align align, std::u16string_view text)
    {
        MphRead::Hud::HudObjectInstance& weaponIconInst = RequireReference(_weaponIconInst);
        if (!weaponIconInst.Enabled || text.empty())
        {
            return x;
        }

        const MphRead::Hud::HudObjects& hudObjects = RequireReference(_hudObjects);
        const float iconLeft = static_cast<float>(hudObjects.WeaponIconPosX) + _objShiftX;
        const float iconTop = static_cast<float>(hudObjects.WeaponIconPosY) + _objShiftY;
        const float iconRight = iconLeft + static_cast<float>(weaponIconInst.Width);
        const float iconBottom = iconTop
            + static_cast<float>(weaponIconInst.Height) / std::max(HudAspectFix(), 0.0001F);

        const float textTop = y;
        const float textBottom = y + FontLineHeight;
        if (textBottom <= iconTop || textTop >= iconBottom)
        {
            return x;
        }

        const float width = ModTextWidth(text);
        float left;
        switch (align)
        {
        case MphRead::Hud::Align::Right:
            left = x - width;
            break;
        case MphRead::Hud::Align::Center:
        case MphRead::Hud::Align::PadCenter:
            left = x - width / 2.0F;
            break;
        default:
            left = x;
            break;
        }

        const float right = left + width;
        if (right <= iconLeft || left >= iconRight)
        {
            return x;
        }

        const float shift = right - iconLeft + AmmoIconGap;
        if (left - shift >= AmmoIconGap)
        {
            return x - shift;
        }
        return x + (iconRight - left + AmmoIconGap);
    }

    float PlayerEntity::ModTextWidth(std::u16string_view text)
    {
        if (text.empty())
        {
            return 0.0F;
        }

        const MphRead::Text::Font& font = SetUpFont(text[0], false);
        const float aspectFix = HudAspectFix();
        float width = 0.0F;
        for (std::size_t i = 0; i < text.size(); i++)
        {
            const char16_t ch = text[i];
            if (ch == u'\0')
            {
                break;
            }
            const std::int32_t index = GlyphIndex(font, static_cast<std::int32_t>(ch));
            width += static_cast<float>(ManagedAt(RequireReference(font.Widths()), index)) * aspectFix;
        }
        return width;
    }
}
