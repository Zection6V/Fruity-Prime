#pragma once

#include <EGL/egl.h>

#include <cstdint>
#include <memory>

namespace MphRead::Droid
{
    class OffscreenGl final
    {
    public:
        OffscreenGl() noexcept = default;
        OffscreenGl(const OffscreenGl&) = delete;
        OffscreenGl& operator=(const OffscreenGl&) = delete;
        OffscreenGl(OffscreenGl&&) = delete;
        OffscreenGl& operator=(OffscreenGl&&) = delete;
        ~OffscreenGl() = default;

        [[nodiscard]] static std::shared_ptr<OffscreenGl> Create(
            std::int32_t width,
            std::int32_t height
        );

        void MakeCurrent();
        void Dispose();

    private:
        static constexpr EGLint OpenGlEs3Bit = 0x40;

        void Init(std::int32_t width, std::int32_t height);

        EGLDisplay _display = EGL_NO_DISPLAY;
        EGLSurface _surface = EGL_NO_SURFACE;
        EGLContext _context = EGL_NO_CONTEXT;

        bool _displayAssigned = false;
        bool _surfaceAssigned = false;
        bool _contextAssigned = false;
    };
}
