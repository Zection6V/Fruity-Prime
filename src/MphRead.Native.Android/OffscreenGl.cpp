#include "OffscreenGl.hpp"

#if !defined(__ANDROID__)
#error "OffscreenGl is only valid for the Android native target."
#endif

#include <cstdint>
#include <iostream>
#include <memory>
#include <sstream>
#include <stdexcept>
#include <string>

namespace
{
    std::string EglFailure(const char* operation)
    {
        std::ostringstream stream;
        stream << operation << " (0x"
               << std::uppercase << std::hex
               << static_cast<std::uint32_t>(eglGetError())
               << ')';
        return stream.str();
    }
}

namespace MphRead::Droid
{
    std::shared_ptr<OffscreenGl> OffscreenGl::Create(
        std::int32_t width,
        std::int32_t height
    )
    {
        auto gl = std::make_shared<OffscreenGl>();
        try
        {
            gl->Init(width, height);
        }
        catch (...)
        {
            gl->Dispose();
            throw;
        }
        return gl;
    }

    void OffscreenGl::Init(std::int32_t width, std::int32_t height)
    {
        _display = eglGetDisplay(EGL_DEFAULT_DISPLAY);
        _displayAssigned = true;
        if (_display == EGL_NO_DISPLAY)
        {
            throw std::runtime_error("no EGL display");
        }

        EGLint version[2] = { 0, 0 };
        if (eglInitialize(_display, &version[0], &version[1]) != EGL_TRUE)
        {
            throw std::runtime_error(EglFailure("eglInitialize failed"));
        }

        const EGLint attributes[] =
        {
            EGL_RENDERABLE_TYPE, OpenGlEs3Bit,
            EGL_SURFACE_TYPE, EGL_PBUFFER_BIT,
            EGL_RED_SIZE, 8,
            EGL_GREEN_SIZE, 8,
            EGL_BLUE_SIZE, 8,
            EGL_ALPHA_SIZE, 0,
            EGL_DEPTH_SIZE, 24,
            EGL_STENCIL_SIZE, 8,
            EGL_NONE
        };

        EGLConfig configs[1] = { nullptr };
        EGLint found[1] = { 0 };
        if (eglChooseConfig(
                _display,
                attributes,
                &configs[0],
                1,
                &found[0]
            ) != EGL_TRUE
            || found[0] < 1
            || configs[0] == nullptr)
        {
            throw std::runtime_error(
                "no EGL config with a pbuffer, depth and stencil"
            );
        }

        const EGLint surfaceAttributes[] =
        {
            EGL_WIDTH, static_cast<EGLint>(width),
            EGL_HEIGHT, static_cast<EGLint>(height),
            EGL_NONE
        };
        _surface = eglCreatePbufferSurface(
            _display,
            configs[0],
            surfaceAttributes
        );
        _surfaceAssigned = true;
        if (_surface == EGL_NO_SURFACE)
        {
            throw std::runtime_error(
                EglFailure("eglCreatePbufferSurface failed")
            );
        }

        const EGLint contextAttributes[] =
        {
            EGL_CONTEXT_CLIENT_VERSION, 3,
            EGL_NONE
        };
        _context = eglCreateContext(
            _display,
            configs[0],
            EGL_NO_CONTEXT,
            contextAttributes
        );
        _contextAssigned = true;
        if (_context == EGL_NO_CONTEXT)
        {
            throw std::runtime_error(
                EglFailure("eglCreateContext failed")
            );
        }

        MakeCurrent();
    }

    void OffscreenGl::MakeCurrent()
    {
        if (eglMakeCurrent(
                _display,
                _surface,
                _surface,
                _context
            ) != EGL_TRUE)
        {
            throw std::runtime_error(
                EglFailure("eglMakeCurrent failed")
            );
        }
    }

    void OffscreenGl::Dispose()
    {
        if (!_displayAssigned)
        {
            return;
        }

        try
        {
            (void)eglMakeCurrent(
                _display,
                EGL_NO_SURFACE,
                EGL_NO_SURFACE,
                EGL_NO_CONTEXT
            );
            if (_contextAssigned)
            {
                (void)eglDestroyContext(_display, _context);
            }
            if (_surfaceAssigned)
            {
                (void)eglDestroySurface(_display, _surface);
            }
            (void)eglTerminate(_display);
        }
        catch (const std::exception& ex)
        {
            std::cout
                << "[preview] tearing the offscreen context down failed: "
                << ex.what()
                << '\n';
        }

        _context = EGL_NO_CONTEXT;
        _surface = EGL_NO_SURFACE;
        _display = EGL_NO_DISPLAY;
        _contextAssigned = false;
        _surfaceAssigned = false;
        _displayAssigned = false;
    }
}
