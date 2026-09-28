#include "ThumbnailWindowCheck.hpp"

#if defined(MPHREAD_SHELL)

#include "../ScreenCapture.hpp"
#include "../ThumbnailCapture.hpp"
#include "../../NativeRuntime/OpenTK/GL.hpp"
#include "../../NativeRuntime/System/Console.hpp"
#include "../../NativeRuntime/System/ExceptionText.hpp"
#include "../../NativeRuntime/System/Exceptions.hpp"
#include "../../Renderer.hpp"

#include <cstdint>
#include <cstdlib>
#include <exception>
#include <memory>
#include <string>
#include <vector>

namespace MphRead::Mods::Diagnostics
{
    namespace GL = ::OpenTK::Graphics::OpenGL::GL;

    int ThumbnailWindowCheck::Run(bool legacyCheck)
    {
        try
        {
            // Use the worker's actual settings in a separate process, before
            // the launcher's context can initialize GLFW or mask a failure.
            RendererPlatform::WindowSettings settings = ThumbnailCapture::WindowSettings(64, 64);
            if (legacyCheck)
            {
                settings.ApiMajor = 2;
                settings.ApiMinor = 1;
                settings.Profile = RendererPlatform::WindowSettings::ContextProfile::Any;
            }
            const std::shared_ptr<RendererPlatform::Window> window = RendererPlatform::CreateWindow(settings);
            bool debugSkipped = false;
            ScreenCapture::EnableDebugOutput([&debugSkipped](const std::string& line)
            {
                NativeRuntime::ConsoleWriteLine(line);
                debugSkipped |= line.find("GL debug output unavailable") != std::string::npos;
            });
            NativeRuntime::ConsoleWriteLine(ScreenCapture::DescribeContext());
            if (GL::GetError() != GL::ErrorCode::NoError)
            {
                throw System::InvalidOperationException("Thumbnail diagnostics raised an OpenGL error.");
            }
            if (legacyCheck
                && (!debugSkipped || !GL::GetString(GL::StringName::Version).starts_with("2.1")))
            {
                throw System::InvalidOperationException(
                    "Legacy regression requires GL 2.1 without KHR_debug.");
            }
            (void)window;

            const std::int32_t texture = GL::GenTexture();
            const std::int32_t framebuffer = GL::GenFramebuffer();
            const std::int32_t vertexBuffer = GL::GenBuffer();
            const std::int32_t indexBuffer = GL::GenBuffer();
            try
            {
                GL::BindTexture(GL::TextureTarget::Texture2D, texture);
                GL::TexImage2D(GL::TextureTarget::Texture2D, 0, GL::PixelInternalFormat::Rgba8,
                    64, 64, 0, GL::PixelFormat::Rgba, GL::PixelType::UnsignedByte, nullptr);
                GL::BindFramebuffer(GL::FramebufferTarget::Framebuffer, framebuffer);
                GL::FramebufferTexture2D(GL::FramebufferTarget::Framebuffer,
                    GL::FramebufferAttachment::ColorAttachment0,
                    GL::TextureTarget::Texture2D, texture, 0);
                if (GL::CheckFramebufferStatus(GL::FramebufferTarget::Framebuffer)
                    != GL::FramebufferErrorCode::FramebufferComplete)
                {
                    throw System::InvalidOperationException("Thumbnail framebuffer is incomplete.");
                }
                GL::DrawBuffer(GL::DrawBufferMode::ColorAttachment0);
                GL::ReadBuffer(GL::ReadBufferMode::ColorAttachment0);
                GL::Viewport(0, 0, 64, 64);
                GL::ClearColor(0, 0, 0, 1);
                GL::Clear(GL::ClearBufferMask::ColorBufferBit);
                // Exercise the compatibility-profile VBO/IBO path used by
                // Phase 4 thumbnail rendering without relying on immediate mode.
                constexpr float vertices[]{
                    -1.0F, -1.0F,
                     1.0F, -1.0F,
                     1.0F,  1.0F,
                    -1.0F,  1.0F
                };
                constexpr std::uint32_t indices[]{0U, 1U, 2U, 3U};
                GL::BindBuffer(GL::BufferTarget::ArrayBuffer, vertexBuffer);
                GL::BufferData(GL::BufferTarget::ArrayBuffer, sizeof(vertices),
                    vertices, GL::BufferUsageHint::StaticDraw);
                GL::BindBuffer(GL::BufferTarget::ElementArrayBuffer, indexBuffer);
                GL::BufferData(GL::BufferTarget::ElementArrayBuffer, sizeof(indices),
                    indices, GL::BufferUsageHint::StaticDraw);
                GL::Color3(1.0F, 0.25F, 0.5F);
                GL::DisableClientState(GL::ClientState::ColorArray);
                GL::EnableClientState(GL::ClientState::VertexArray);
                GL::VertexPointer(2, GL::PointerType::Float, 0, nullptr);
                GL::DrawElements(GL::PrimitiveType::Quads, 4,
                    GL::DrawElementsType::UnsignedInt, nullptr);
                GL::DisableClientState(GL::ClientState::VertexArray);
                GL::BindBuffer(GL::BufferTarget::ArrayBuffer, 0);
                GL::BindBuffer(GL::BufferTarget::ElementArrayBuffer, 0);
                std::vector<std::uint8_t> pixel(4);
                GL::ReadPixels(32, 32, 1, 1, GL::PixelFormat::Rgba,
                    GL::PixelType::UnsignedByte, pixel.data());
                if (GL::GetError() != GL::ErrorCode::NoError || pixel[0] < 240
                    || std::abs(static_cast<int>(pixel[1]) - 64) > 4
                    || std::abs(static_cast<int>(pixel[2]) - 128) > 4)
                {
                    throw System::InvalidOperationException("Thumbnail legacy rendering/readback failed.");
                }
            }
            catch (...)
            {
                GL::BindFramebuffer(GL::FramebufferTarget::Framebuffer, 0);
                GL::DeleteBuffer(indexBuffer);
                GL::DeleteBuffer(vertexBuffer);
                GL::DeleteFramebuffer(framebuffer);
                GL::DeleteTexture(texture);
                throw;
            }
            GL::BindFramebuffer(GL::FramebufferTarget::Framebuffer, 0);
            GL::DeleteBuffer(indexBuffer);
            GL::DeleteBuffer(vertexBuffer);
            GL::DeleteFramebuffer(framebuffer);
            GL::DeleteTexture(texture);
            NativeRuntime::ConsoleWriteLine("Thumbnail window check passed.");
            return 0;
        }
        catch (const std::exception&)
        {
            NativeRuntime::ConsoleErrorWriteLine("[thumbnailwindowcheck] "
                + NativeRuntime::ExceptionToString(std::current_exception()));
            return 1;
        }
    }
}

#endif
