#pragma once

#include <cstdint>
#include <string_view>

namespace MphRead::Hud
{
    enum class Align : std::int32_t;
    class HudObjectInstance;
    class HudObjects;
}

namespace MphRead::Text
{
    class Font;
}

namespace MphRead::Entities
{
    class PlayerEntity
    {
    public:
        float ModAmmoTextX(float x, float y, MphRead::Hud::Align align, std::u16string_view text);

    private:
        static constexpr float AmmoIconGap = 2.0F;
        static constexpr float FontLineHeight = 12.0F;

        float ModTextWidth(std::u16string_view text);

        // Declaration-only seams for private members owned by the later PlayerEntity/Text slices.
        // They carry no behavior in this slice.
        [[nodiscard]] MphRead::Hud::HudObjectInstance& WeaponIconInst();
        [[nodiscard]] MphRead::Hud::HudObjects& HudObjectsState();
        [[nodiscard]] float ObjShiftX() const;
        [[nodiscard]] float ObjShiftY() const;
        [[nodiscard]] float HudAspectFix();
        [[nodiscard]] MphRead::Text::Font& SetUpFont(char16_t firstChar, bool set);
        [[nodiscard]] static std::int32_t GlyphIndex(const MphRead::Text::Font& font, std::int32_t ch);
        [[nodiscard]] static std::int32_t FontWidthAt(const MphRead::Text::Font& font, std::int32_t index);
    };
}
