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

        // How wide the view is, in degrees, measured the way the game
        // measures it.
        //
        // The DS game is a 78: PlayerValues::NormalFov is 39 and every camera
        // doubles it. That is a narrow picture by the standards of anything
        // played with a mouse, and it is the single setting most often asked
        // for in a shooter -- so this is a multiplier on whatever the camera
        // asked for rather than a replacement for it. Zooming with a weapon,
        // the Judicator's scope and every scripted camera all move
        // CameraInfo::Fov themselves, and each of them keeps its proportions:
        // at 100 the zoom is as tight relative to the hip view as it was on
        // the cartridge.
        //
        // Clamped rather than free. Below about 60 the gun fills the screen;
        // above 120 the projection distorts badly enough at the edges that
        // aiming gets worse, not better.
        [[nodiscard]] static std::int32_t FieldOfView() noexcept;
        static void FieldOfView(std::int32_t value) noexcept;

        // What the DS game plays at: NormalFov 39, doubled.
        inline static constexpr std::int32_t DefaultFov = 78;

        inline static constexpr std::int32_t MinFov = 60;
        inline static constexpr std::int32_t MaxFov = 120;

        // The multiplier a camera's own field of view is scaled by.
        [[nodiscard]] static float FovScale() noexcept;

        [[nodiscard]] static std::int32_t ParseFov(
            std::optional<std::string_view> value, std::int32_t fallback) noexcept;

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
        static std::int32_t _fieldOfView;
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
