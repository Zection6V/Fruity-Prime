#pragma once

#include <cstdint>

namespace MphRead
{
    class RenderWindow;
}

namespace MphRead::Mods::Render
{
    class UiOverlay final
    {
    public:
        UiOverlay() = delete;

        [[nodiscard]] static bool Visible() noexcept;
        static void Visible(bool value) noexcept;
        [[nodiscard]] static bool HasFrame() noexcept;
        static void Upload(const void* pixels, std::int32_t width, std::int32_t height);
        static void UseTexture(std::int32_t texture, std::int32_t width, std::int32_t height);
        static void Draw(std::int32_t width, std::int32_t height);
        static void DrawAlone(::MphRead::RenderWindow& window, std::int32_t width, std::int32_t height);
        static void Release();

    private:
        static constexpr std::int32_t Name = 1'000'000;
        static std::int32_t _texture;
        static std::int32_t _vertexBuffer;
        static std::int32_t _indexBuffer;
        static std::int32_t _width;
        static std::int32_t _height;
        static bool _hasFrame;
        static bool _visible;
        static bool _ownsTexture;
        static bool _topRowAtTextureZero;
    };
}
