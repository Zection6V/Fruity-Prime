#pragma once

#include <cstdint>
#include <optional>
#include <string_view>

namespace MphRead::Mods
{
    class RenderOptions final
    {
    public:
        [[nodiscard]] static std::int32_t ResolutionScale() noexcept;
        static void ResolutionScale(std::int32_t value) noexcept;

        inline static constexpr std::int32_t MinScale = 25;

        [[nodiscard]] static bool Lighting() noexcept;
        static void Lighting(bool value) noexcept;

        [[nodiscard]] static bool CelShading() noexcept;
        static void CelShading(bool value) noexcept;

        [[nodiscard]] static bool ShowFps() noexcept;
        static void ShowFps(bool value) noexcept;

        [[nodiscard]] static std::int32_t CelBands() noexcept;
        static void CelBands(std::int32_t value) noexcept;

        [[nodiscard]] static float CelEdge() noexcept;
        static void CelEdge(float value) noexcept;

        [[nodiscard]] static bool Fog() noexcept;
        static void Fog(bool value) noexcept;

        [[nodiscard]] static bool TextureFiltering() noexcept;
        static void TextureFiltering(bool value) noexcept;

        [[nodiscard]] static std::int32_t Scaled(std::int32_t pixels) noexcept;

        [[nodiscard]] static bool ParseOnOff(
            std::optional<std::string_view> value, bool fallback) noexcept;

        [[nodiscard]] static std::string_view OnOff(bool value) noexcept;

        [[nodiscard]] static std::int32_t ParseScale(
            std::optional<std::string_view> value, std::int32_t fallback) noexcept;

        [[nodiscard]] static std::int32_t ParseInt(
            std::optional<std::string_view> value, std::int32_t fallback) noexcept;

        RenderOptions() = delete;
        RenderOptions(const RenderOptions&) = delete;
        RenderOptions& operator=(const RenderOptions&) = delete;

    private:
        static std::int32_t _resolutionScale;
        static bool _lighting;
        static bool _celShading;
        static bool _showFps;
        static std::int32_t _celBands;
        static float _celEdge;
        static bool _fog;
        static bool _textureFiltering;
    };
}
