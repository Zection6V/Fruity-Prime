#include "PlayerEntityStylusHud.hpp"

#include "../../Entities/Players/DynamicLightEntity.hpp"
#include "../Input/StylusZone.hpp"

#include <cmath>
#include <cstdint>
#include <limits>
#include <memory>

namespace MphRead
{
    class Scene
    {
    public:
        void DrawHudFlatBox(float left, float top, float right, float bottom,
            OpenTK::Mathematics::Vector4 colour);
    };
}

namespace
{
    float DotNetMax(float val1, float val2) noexcept
    {
        if (val1 != val2)
        {
            if (!std::isnan(val1))
            {
                return val2 < val1 ? val1 : val2;
            }
            return val1;
        }
        return std::signbit(val2) ? val1 : val2;
    }

    float DotNetMin(float val1, float val2) noexcept
    {
        if (val1 != val2)
        {
            if (!std::isnan(val1))
            {
                return val1 < val2 ? val1 : val2;
            }
            return val1;
        }
        return std::signbit(val1) ? val1 : val2;
    }

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

    MphRead::Scene& RequireScene(MphRead::Scene* scene)
    {
        if (scene == nullptr)
        {
            throw System::NullReferenceException();
        }
        return *scene;
    }
}

namespace MphRead::Entities
{
    class PlayerEntity final : public DynamicLightEntityBase,
        public std::enable_shared_from_this<PlayerEntity>
    {
    public:
        [[nodiscard]] bool IsMainPlayer() const;

        MPHREAD_PLAYER_STYLUS_HUD_MEMBERS
    };

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
        float line = DotNetMax(0.5F, height / 96.0F);
        RequireScene(_scene).DrawHudFlatBox(left, top, left + width, top + line, edge);
        RequireScene(_scene).DrawHudFlatBox(left, top + height - line, left + width, top + height, edge);
        RequireScene(_scene).DrawHudFlatBox(left, top, left + line, top + height, edge);
        RequireScene(_scene).DrawHudFlatBox(left + width - line, top, left + width, top + height, edge);

        float scaleX = width / StylusZone::DsWidth;
        float scaleY = height / StylusZone::DsHeight;
        for (StylusZone::Button button : StylusZone::Buttons)
        {
            bool lit = !StylusZone::Placing() && StylusZone::Contact()
                && StylusZone::Region() == button.Region;
            OpenTK::Mathematics::Vector4 colour = lit
                ? OpenTK::Mathematics::Vector4(_stylusLit.Xyz(), DotNetMin(1.0F, alpha * 3.0F))
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
            float half = radiusX * std::sqrt(DotNetMax(0.0F, 1.0F - t * t));
            if (half <= 0.0F)
            {
                continue;
            }

            RequireScene(_scene).DrawHudFlatBox(centreX - half, centreY + y,
                centreX + half, centreY + y + step, colour);
        }
    }
}
