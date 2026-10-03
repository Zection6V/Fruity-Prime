#include "LauncherWindowCheck.hpp"

#if defined(MPHREAD_SHELL)

#include "../Launcher/GuiLauncher.hpp"
#include "../WindowGeometry.hpp"
#include "../Render/UiOverlay.hpp"
#include "../../NativeRuntime/OpenTK/GL.hpp"
#include "../../NativeRuntime/System/Console.hpp"
#include "../../NativeRuntime/System/ExceptionText.hpp"
#include "../../NativeRuntime/System/Exceptions.hpp"
#include "../../Renderer.hpp"
#include "../../NativeRuntime/Rhi/SceneBackend.hpp"
#include "../../Shaders.hpp"

#include <cstdint>
#include <exception>
#include <limits>
#include <string>
#include <utility>
#include <vector>

namespace MphRead::Mods::Diagnostics
{
    namespace GL = ::OpenTK::Graphics::OpenGL::GL;
    using GL::ErrorCode;
    using GL::GetProgramParameterName;
    using GL::ReadBufferMode;
    using GL::ShaderParameter;
    using GL::ShaderType;
    using GL::StringName;

    bool LauncherWindowCheck::_active = false;
    bool LauncherWindowCheck::_passed = false;
    int LauncherWindowCheck::_frames = 0;

    int LauncherWindowCheck::Run()
    {
        _active = true;
        _passed = false;
        _frames = 0;
        const bool geometry = WindowGeometry::Enabled();
        WindowGeometry::Enabled(false);
        bool passed = false;
        try
        {
            const bool ran = Launcher::Gui::GuiLauncher::TryRun();
            passed = ran && _passed;
            NativeRuntime::ConsoleWriteLine(passed
                ? "Launcher window check passed." : "Launcher window check failed.");
        }
        catch (...)
        {
            _active = false;
            WindowGeometry::Enabled(geometry);
            throw;
        }
        _active = false;
        WindowGeometry::Enabled(geometry);
        return passed ? 0 : 1;
    }

    void LauncherWindowCheck::AfterDraw(MphRead::RenderWindow& window)
    {
        if (!_active || (++_frames != 20 && _frames != 40))
        {
            return;
        }
        try
        {
            const bool openGl = NativeRuntime::Rhi::SceneDevice().GetBackend() == NativeRuntime::Rhi::GraphicsBackend::OpenGl;
            if (_frames == 20 && openGl)
            {
                NativeRuntime::ConsoleWriteLine("[windowcheck] " + GL::GetString(StringName::Renderer)
                    + "; GL " + GL::GetString(StringName::Version));
                Link(Shaders::VertexShader, Shaders::FragmentShader);
                Link(Shaders::RttVertexShader, Shaders::RttFragmentShader);
                Link(Shaders::RttVertexShader, Shaders::CelFragmentShader);
                Link(Shaders::RttVertexShader, Shaders::ShiftFragmentShader);
            }

            const OpenTK::Mathematics::Vector2i framebuffer = window.FramebufferSize();
            const int width = framebuffer.X;
            const int height = framebuffer.Y;
            if (!Render::UiOverlay::HasFrame() || width <= 0 || height <= 0)
            {
                throw System::InvalidOperationException("Launcher did not upload a frame.");
            }
            if (width > std::numeric_limits<int>::max() / 4 / height)
            {
                throw System::OverflowException();
            }
            std::vector<std::uint8_t> pixels(static_cast<std::size_t>(width * height * 4));
            NativeRuntime::Rhi::RenderingInfo target{};
            target.swapchain = true;
            target.width = width;
            target.height = height;
            auto commands = NativeRuntime::Rhi::SceneDevice().CreateCommandList();
            commands->ReadColor(target, 0, 0, width, height,
                NativeRuntime::Rhi::TextureFormat::RGBA8Unorm, pixels.data());
            int lit = 0;
            for (std::size_t i = 0; i < pixels.size(); i += 4)
            {
                if (pixels[i] > 16 || pixels[i + 1] > 16 || pixels[i + 2] > 16)
                {
                    ++lit;
                }
            }
            if (lit < width * height / 100)
            {
                throw System::InvalidOperationException("Launcher frame is black.");
            }
            const ErrorCode error = openGl ? GL::GetError() : ErrorCode::NoError;
            if (error != ErrorCode::NoError)
            {
                throw System::InvalidOperationException("OpenGL error: "
                    + ::OpenTK::Graphics::OpenGL::ToString(error));
            }
            NativeRuntime::ConsoleWriteLine("[windowcheck] rendered " + std::to_string(width)
                + "x" + std::to_string(height) + ", " + std::to_string(lit) + " lit pixels");
            if (_frames == 20)
            {
                window.ClientSize(OpenTK::Mathematics::Vector2i(1100, 740));
            }
            else
            {
                _passed = true;
                window.Close();
            }
        }
        catch (const std::exception&)
        {
            NativeRuntime::ConsoleErrorWriteLine("[windowcheck] "
                + NativeRuntime::ExceptionToString(std::current_exception()));
            _active = false;
            window.Close();
        }
    }

    void LauncherWindowCheck::Link(const std::string& vertex, const std::string& fragment)
    {
        const std::int32_t program = GL::CreateProgram();
        try
        {
            const std::pair<ShaderType, const std::string&> shaders[] = {
                {ShaderType::VertexShader, vertex},
                {ShaderType::FragmentShader, fragment}
            };
            for (const auto& [type, source] : shaders)
            {
                const std::int32_t shader = GL::CreateShader(type);
                try
                {
                    GL::ShaderSource(shader, source);
                    GL::CompileShader(shader);
                    std::int32_t compiled = 0;
                    GL::GetShader(shader, ShaderParameter::CompileStatus, compiled);
                    if (compiled == 0)
                    {
                        throw System::InvalidOperationException(GL::GetShaderInfoLog(shader));
                    }
                    GL::AttachShader(program, shader);
                }
                catch (...)
                {
                    GL::DeleteShader(shader);
                    throw;
                }
                GL::DeleteShader(shader);
            }
            GL::LinkProgram(program);
            std::int32_t linked = 0;
            GL::GetProgram(program, GetProgramParameterName::LinkStatus, linked);
            if (linked == 0)
            {
                throw System::InvalidOperationException(GL::GetProgramInfoLog(program));
            }
        }
        catch (...)
        {
            GL::DeleteProgram(program);
            throw;
        }
        GL::DeleteProgram(program);
    }
}

#endif
