#pragma once

#include "../Renderer.hpp"

#include <cstdint>
#include <memory>
#include <string>
#include <vector>

namespace MphRead
{
    class Scene;
}

namespace MphRead::Mods
{
    class ThumbnailCapture final
    {
    public:
        ThumbnailCapture(const ThumbnailCapture&) = delete;
        ThumbnailCapture& operator=(const ThumbnailCapture&) = delete;
        ThumbnailCapture(ThumbnailCapture&&) = delete;
        ThumbnailCapture& operator=(ThumbnailCapture&&) = delete;
        ~ThumbnailCapture();

        [[nodiscard]] MphRead::Scene& Scene() const;
        [[nodiscard]] bool Succeeded() const noexcept;

        [[nodiscard]] static std::int32_t CaptureRooms(
            const std::vector<std::string>& rooms,
            std::int32_t width,
            std::int32_t height);
        [[nodiscard]] static bool CaptureRoom(
            const std::string& roomKey,
            std::int32_t width,
            std::int32_t height);

    private:
        static constexpr std::int32_t SettleFrames = 12;
        static constexpr std::int32_t RetryFrames = 20;
        static constexpr std::int32_t MaxAttempts = 3;

        [[nodiscard]] static RendererPlatform::WindowSettings GameSettings();
        [[nodiscard]] static RendererPlatform::WindowSettings WindowSettings(
            std::int32_t width,
            std::int32_t height);

        ThumbnailCapture(
            const std::string& roomKey,
            std::int32_t width,
            std::int32_t height);

        void OnLoad();
        void ApplyPreviewCamera();
        void OnRenderFrame(const RendererPlatform::FrameEventArgs& args);
        void OnClosing();
        void Run();
        void Close();

        [[nodiscard]] OpenTK::Mathematics::Vector2i ClientSize() const;
        [[nodiscard]] bool IsVisible() const noexcept;
        void IsVisible(bool value);
        void SwapBuffers();

        static bool _describedContext;

        std::shared_ptr<RendererPlatform::Window> _window{};
        std::shared_ptr<MphRead::Scene> _scene{};
        std::string _roomKey{};
        std::int32_t _settleFrames = 0;
        bool _captured = false;
        std::int32_t _attempts = 0;
        OpenTK::Graphics::OpenGL::ErrorCode _frameError
            = OpenTK::Graphics::OpenGL::ErrorCode::NoError;
        OpenTK::Mathematics::Vector2i _asked{};
        bool _isVisible = false;
        bool _closeRequested = false;
    };
}
