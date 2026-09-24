#include "PlayerEntityStylusHud.hpp"

#include "../../Entities/Players/DynamicLightEntity.hpp"
#include "../../Entities/Players/PlayerEntity.hpp"
#include "../../Scene.hpp"
#include "../Input/StylusZone.hpp"
#include "../../NativeRuntime/System/Managed.hpp"

#include <cmath>
#include <cstdint>
#include <limits>
#include <memory>

using ::MphRead::NativeRuntime::MathMax;
using ::MphRead::NativeRuntime::MathMin;
using ::MphRead::NativeRuntime::RequireReference;

namespace
{
    std::int32_t DotNetToInt32(float value) noexcept
    {
        if (std::isnan(value))
        {
            return 0;
        }

        const double widened = static_cast<double>(value);
        if (widened < static_cast<double>(std::numeric_limits<std::int32_t>::min()))
        {
            return std::numeric_limits<std::int32_t>::min();
        }
        if (widened > static_cast<double>(std::numeric_limits<std::int32_t>::max()))
        {
            return std::numeric_limits<std::int32_t>::max();
        }
        return static_cast<std::int32_t>(value);
    }

}

namespace MphRead::Entities
{
    const OpenTK::Mathematics::Vector4 PlayerEntity::_stylusInk(0.85F, 0.30F, 0.30F, 1.0F);
    const OpenTK::Mathematics::Vector4 PlayerEntity::_stylusFill(0.55F, 0.16F, 0.16F, 1.0F);
    const OpenTK::Mathematics::Vector4 PlayerEntity::_stylusLit(1.0F, 0.72F, 0.35F, 1.0F);

    void PlayerEntity::ModDrawStylusZone()
    {
        using Mods::Input::StylusZone;

        if (!IsMainPlayer() || (!StylusZone::Enabled() && !StylusZone::Placing()))
        {
            return;
        }

        float left = StylusZone::Left() * 256.0F;
        float top = StylusZone::Top() * 192.0F;
        float width = StylusZone::Width() * 256.0F;
        float height = StylusZone::Height() * 192.0F;
        if (width <= 1.0F || height <= 1.0F)
        {
            return;
        }

        float alpha = StylusZone::Placing() ? 0.55F : StylusZone::Opacity();
        OpenTK::Mathematics::Vector4 edge(_stylusInk.Xyz(), alpha);
        OpenTK::Mathematics::Vector4 fill(_stylusFill.Xyz(), alpha * 0.5F);
        float line = MathMax(0.5F, height / 96.0F);
        RequireReference(_scene).DrawHudFlatBox(left, top, left + width, top + line, edge);
        RequireReference(_scene).DrawHudFlatBox(left, top + height - line, left + width, top + height, edge);
        RequireReference(_scene).DrawHudFlatBox(left, top, left + line, top + height, edge);
        RequireReference(_scene).DrawHudFlatBox(left + width - line, top, left + width, top + height, edge);

        float scaleX = width / StylusZone::DsWidth;
        float scaleY = height / StylusZone::DsHeight;
        for (StylusZone::Button button : StylusZone::Buttons)
        {
            bool lit = !StylusZone::Placing() && StylusZone::Contact()
                && StylusZone::Region() == button.Region;
            OpenTK::Mathematics::Vector4 colour = lit
                ? OpenTK::Mathematics::Vector4(_stylusLit.Xyz(), MathMin(1.0F, alpha * 3.0F))
                : fill;
            DrawStylusCircle(left + button.X * scaleX, top + button.Y * scaleY,
                button.Radius * scaleX, button.Radius * scaleY, colour);
        }
    }

    void PlayerEntity::DrawStylusCircle(float centreX, float centreY, float radiusX, float radiusY,
        OpenTK::Mathematics::Vector4 colour)
    {
        std::int32_t rows = DotNetToInt32(std::ceil(radiusY * 2.0F));
        if (rows < 2 || radiusX <= 0.0F)
        {
            return;
        }

        rows = rows < 96 ? rows : 96;
        float step = radiusY * 2.0F / static_cast<float>(rows);
        for (std::int32_t i = 0; i < rows; i++)
        {
            float y = -radiusY + (static_cast<float>(i) + 0.5F) * step;
            float t = y / radiusY;
            float half = radiusX * std::sqrt(MathMax(0.0F, 1.0F - t * t));
            if (half <= 0.0F)
            {
                continue;
            }

            RequireReference(_scene).DrawHudFlatBox(centreX - half, centreY + y,
                centreX + half, centreY + y + step, colour);
        }
    }
}
