#pragma once

#include <array>
#include <cstdint>
#include <new>
#include <optional>
#include <string>
#include <string_view>
#include <tuple>
#include <vector>

namespace MphRead::Mods::Render
{
    enum class CrosshairSize : std::int32_t
    {
        Small = 0,
        Medium = 1,
        Big = 2
    };

    enum class CrosshairStyle : std::int32_t
    {
        Cross = 0,
        Dot = 1,
        CrossDot = 2,
        Circle = 3,
        Brackets = 4
    };

    struct CrosshairBar
    {
        const float X = 0.0F;
        const float Y = 0.0F;
        const float Width = 0.0F;
        const float Height = 0.0F;

        constexpr CrosshairBar() noexcept = default;
        constexpr CrosshairBar(float x, float y, float width, float height) noexcept
            : X(x), Y(y), Width(width), Height(height)
        {
        }

        CrosshairBar(const CrosshairBar&) noexcept = default;

        CrosshairBar& operator=(const CrosshairBar& other) noexcept
        {
            if (this != &other)
            {
                this->~CrosshairBar();
                ::new (static_cast<void*>(this)) CrosshairBar(other);
            }
            return *this;
        }
    };

    class Crosshair final
    {
    public:
        Crosshair() = delete;

        static CrosshairSize Size;
        static CrosshairStyle Style;

        [[nodiscard]] static float ScaleOf(CrosshairSize size);
        [[nodiscard]] static float Scale();

        static std::array<std::string, 3> SizeNames;
        static std::array<std::string, 5> StyleNames;

        [[nodiscard]] static std::tuple<float, float> RingOf(CrosshairStyle style, float scale);
        [[nodiscard]] static std::vector<CrosshairBar> BarsOf(CrosshairStyle style, float scale);
        [[nodiscard]] static std::tuple<float, float, float, float> EdgesOf(CrosshairBar bar);

        [[nodiscard]] static CrosshairSize ParseSize(
            std::optional<std::string_view> value, CrosshairSize fallback);
        [[nodiscard]] static CrosshairStyle ParseStyle(
            std::optional<std::string_view> value, CrosshairStyle fallback);

    private:
        [[nodiscard]] static CrosshairBar Dot(float side, float scale);
        static void AddCross(std::vector<CrosshairBar>& bars, float arm, float thickness,
            float gap, float scale);
        static void AddBrackets(std::vector<CrosshairBar>& bars, float corner, float length,
            float thickness, float scale);
        [[nodiscard]] static float Snap(float value, bool down);
    };
}
