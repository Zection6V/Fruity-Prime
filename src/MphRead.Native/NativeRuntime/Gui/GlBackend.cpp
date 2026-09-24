// The OpenGL backend. It is the default because the launcher's window already
// carries a GL context, which is the same one the game renders through.

#include "Backend.hpp"

#include "../OpenTK/GL.hpp"

#include <algorithm>
#include <cmath>
#include <mutex>
#include <vector>

namespace MphRead::NativeRuntime::Gui
{
    namespace
    {
        namespace GL = ::OpenTK::Graphics::OpenGL::GL;

        constexpr double Pi = 3.14159265358979323846;

        [[nodiscard]] std::int32_t CornerSegments(double radius) noexcept
        {
            const std::int32_t segments
                = static_cast<std::int32_t>(std::ceil(radius / 2.0)) + 2;
            return std::clamp(segments, 3, 16);
        }

        class GlBackend final : public RenderBackend
        {
        public:
            [[nodiscard]] const char* Name() const noexcept override { return "OpenGL"; }

            [[nodiscard]] bool Initialize() override { return true; }

            void Shutdown() override {}

            void BeginFrame(std::int32_t width, std::int32_t height) override
            {
                _width = std::max(1, width);
                _height = std::max(1, height);
                GL::Viewport(0, 0, _width, _height);
                GL::Disable(GL::EnableCap::DepthTest);
                GL::Disable(GL::EnableCap::CullFace);
                GL::Disable(GL::EnableCap::AlphaTest);
                GL::Enable(GL::EnableCap::Blend);
                GL::BlendFunc(
                    GL::BlendingFactor::SrcAlpha, GL::BlendingFactor::OneMinusSrcAlpha);
                GL::Disable(GL::EnableCap::Texture2D);
                GL::Disable(GL::EnableCap::ScissorTest);
            }

            void EndFrame() override
            {
                GL::Disable(GL::EnableCap::ScissorTest);
                GL::Disable(GL::EnableCap::Texture2D);
            }

            void SetClip(const std::optional<Rect>& rect) override
            {
                if (!rect.has_value())
                {
                    GL::Disable(GL::EnableCap::ScissorTest);
                    return;
                }
                GL::Enable(GL::EnableCap::ScissorTest);
                // GL counts scissor rectangles from the bottom left.
                GL::Scissor(
                    static_cast<std::int32_t>(std::floor(rect->X)),
                    static_cast<std::int32_t>(
                        std::floor(_height - (rect->Y + rect->Height))),
                    static_cast<std::int32_t>(std::ceil(rect->Width)),
                    static_cast<std::int32_t>(std::ceil(rect->Height)));
            }

            void FillRoundedRect(Rect rect, double radius, Color color) override
            {
                if (color.A <= 0.0F || rect.Width <= 0.0 || rect.Height <= 0.0)
                {
                    return;
                }
                const double limit = std::min(rect.Width, rect.Height) / 2.0;
                const double r = std::clamp(radius, 0.0, limit);

                GL::Disable(GL::EnableCap::Texture2D);
                GL::Color4(color.R, color.G, color.B, color.A);

                if (r <= 0.5)
                {
                    GL::Begin(GL::PrimitiveType::Quads);
                    Vertex(rect.X, rect.Y);
                    Vertex(rect.X + rect.Width, rect.Y);
                    Vertex(rect.X + rect.Width, rect.Y + rect.Height);
                    Vertex(rect.X, rect.Y + rect.Height);
                    GL::End();
                    return;
                }

                // The middle band and the two side bands, then the corners.
                GL::Begin(GL::PrimitiveType::Quads);
                Vertex(rect.X + r, rect.Y);
                Vertex(rect.X + rect.Width - r, rect.Y);
                Vertex(rect.X + rect.Width - r, rect.Y + rect.Height);
                Vertex(rect.X + r, rect.Y + rect.Height);

                Vertex(rect.X, rect.Y + r);
                Vertex(rect.X + r, rect.Y + r);
                Vertex(rect.X + r, rect.Y + rect.Height - r);
                Vertex(rect.X, rect.Y + rect.Height - r);

                Vertex(rect.X + rect.Width - r, rect.Y + r);
                Vertex(rect.X + rect.Width, rect.Y + r);
                Vertex(rect.X + rect.Width, rect.Y + rect.Height - r);
                Vertex(rect.X + rect.Width - r, rect.Y + rect.Height - r);
                GL::End();

                const std::int32_t segments = CornerSegments(r);
                const double corners[4][3] = {
                    {rect.X + r, rect.Y + r, Pi},
                    {rect.X + rect.Width - r, rect.Y + r, Pi * 1.5},
                    {rect.X + rect.Width - r, rect.Y + rect.Height - r, 0.0},
                    {rect.X + r, rect.Y + rect.Height - r, Pi * 0.5}};
                for (const auto& corner : corners)
                {
                    GL::Begin(GL::PrimitiveType::TriangleFan);
                    Vertex(corner[0], corner[1]);
                    for (std::int32_t i = 0; i <= segments; ++i)
                    {
                        const double angle = corner[2]
                            + (Pi / 2.0) * (static_cast<double>(i) / segments);
                        Vertex(corner[0] + std::cos(angle) * r,
                            corner[1] + std::sin(angle) * r);
                    }
                    GL::End();
                }
            }

            void StrokeRect(Rect rect, Thickness thickness, Color color) override
            {
                if (color.A <= 0.0F)
                {
                    return;
                }
                const auto band = [&](Rect value)
                {
                    if (value.Width <= 0.0 || value.Height <= 0.0)
                    {
                        return;
                    }
                    GL::Begin(GL::PrimitiveType::Quads);
                    Vertex(value.X, value.Y);
                    Vertex(value.X + value.Width, value.Y);
                    Vertex(value.X + value.Width, value.Y + value.Height);
                    Vertex(value.X, value.Y + value.Height);
                    GL::End();
                };
                GL::Disable(GL::EnableCap::Texture2D);
                GL::Color4(color.R, color.G, color.B, color.A);
                band(Rect{rect.X, rect.Y, rect.Width, thickness.Top});
                band(Rect{rect.X, rect.Y + rect.Height - thickness.Bottom,
                    rect.Width, thickness.Bottom});
                band(Rect{rect.X, rect.Y + thickness.Top, thickness.Left,
                    std::max(0.0, rect.Height - thickness.Vertical())});
                band(Rect{rect.X + rect.Width - thickness.Right, rect.Y + thickness.Top,
                    thickness.Right, std::max(0.0, rect.Height - thickness.Vertical())});
            }

            void DrawTexturedQuads(TextureHandle texture,
                std::span<const TexturedVertex> vertices, Color tint) override
            {
                if (texture == 0 || vertices.size() < 4)
                {
                    return;
                }
                GL::Enable(GL::EnableCap::Texture2D);
                GL::BindTexture(
                    GL::TextureTarget::Texture2D, static_cast<std::int32_t>(texture));
                GL::Color4(tint.R, tint.G, tint.B, tint.A);
                GL::Begin(GL::PrimitiveType::Quads);
                for (const TexturedVertex& vertex : vertices)
                {
                    GL::TexCoord3(vertex.U, vertex.V, 0.0F);
                    Vertex(vertex.X, vertex.Y);
                }
                GL::End();
                GL::Disable(GL::EnableCap::Texture2D);
            }

            [[nodiscard]] TextureHandle CreateAlphaTexture(
                std::int32_t width, std::int32_t height) override
            {
                const std::int32_t texture = GL::GenTexture();
                GL::BindTexture(GL::TextureTarget::Texture2D, texture);
                // A coverage mask is stored as white with the coverage in the
                // alpha channel, rather than as a one-channel texture. The
                // fixed-function combiner then multiplies the draw colour by
                // white and its alpha by the coverage, which is what a glyph
                // is; a red texture would instead multiply the *colour* by the
                // coverage and leave the alpha at one, drawing a black box
                // around every letter.
                const std::vector<std::uint8_t> empty(
                    static_cast<std::size_t>(width) * height * 4, 0);
                GL::PixelStore(GL::PixelStoreParameter::UnpackAlignment, 1);
                GL::TexImage2D(GL::TextureTarget::Texture2D, 0,
                    GL::PixelInternalFormat::Rgba, width, height, 0,
                    GL::PixelFormat::Rgba, GL::PixelType::UnsignedByte,
                    empty.data());
                SetFiltering();
                return static_cast<TextureHandle>(texture);
            }

            void UpdateAlphaTexture(TextureHandle texture, std::int32_t x, std::int32_t y,
                std::int32_t width, std::int32_t height, const std::uint8_t* pixels) override
            {
                if (texture == 0 || width <= 0 || height <= 0 || pixels == nullptr)
                {
                    return;
                }
                const std::size_t count = static_cast<std::size_t>(width) * height;
                std::vector<std::uint8_t> rgba(count * 4);
                for (std::size_t i = 0; i < count; ++i)
                {
                    rgba[i * 4] = 0xFF;
                    rgba[i * 4 + 1] = 0xFF;
                    rgba[i * 4 + 2] = 0xFF;
                    rgba[i * 4 + 3] = pixels[i];
                }
                GL::BindTexture(
                    GL::TextureTarget::Texture2D, static_cast<std::int32_t>(texture));
                GL::PixelStore(GL::PixelStoreParameter::UnpackAlignment, 1);
                GL::TexSubImage2D(GL::TextureTarget::Texture2D, 0, x, y, width, height,
                    GL::PixelFormat::Rgba, GL::PixelType::UnsignedByte, rgba.data());
            }

            [[nodiscard]] TextureHandle CreateColorTexture(std::int32_t width,
                std::int32_t height, const std::uint8_t* rgba) override
            {
                const std::int32_t texture = GL::GenTexture();
                GL::BindTexture(GL::TextureTarget::Texture2D, texture);
                GL::PixelStore(GL::PixelStoreParameter::UnpackAlignment, 1);
                GL::TexImage2D(GL::TextureTarget::Texture2D, 0,
                    GL::PixelInternalFormat::Rgba, width, height, 0,
                    GL::PixelFormat::Rgba, GL::PixelType::UnsignedByte, rgba);
                SetFiltering();
                return static_cast<TextureHandle>(texture);
            }

            void DestroyTexture(TextureHandle texture) override
            {
                if (texture != 0)
                {
                    GL::DeleteTexture(static_cast<std::int32_t>(texture));
                }
            }

        private:
            void Vertex(double x, double y) const
            {
                // Pixels, top left, to normalized device coordinates. The
                // launcher's surface carries no projection of its own.
                GL::Vertex3(
                    static_cast<float>(x / _width * 2.0 - 1.0),
                    static_cast<float>(1.0 - y / _height * 2.0),
                    0.0F);
            }

            static void SetFiltering()
            {
                GL::TexParameter(GL::TextureTarget::Texture2D,
                    GL::TextureParameterName::TextureMinFilter, 0x2601); // Linear
                GL::TexParameter(GL::TextureTarget::Texture2D,
                    GL::TextureParameterName::TextureMagFilter, 0x2601);
                GL::TexParameter(GL::TextureTarget::Texture2D,
                    GL::TextureParameterName::TextureWrapS, 0x812F); // ClampToEdge
                GL::TexParameter(GL::TextureTarget::Texture2D,
                    GL::TextureParameterName::TextureWrapT, 0x812F);
            }

            std::int32_t _width = 1;
            std::int32_t _height = 1;
        };

        std::shared_ptr<RenderBackend>& Current()
        {
            static std::shared_ptr<RenderBackend> backend;
            return backend;
        }
    }

    std::shared_ptr<RenderBackend> CreateGlBackend()
    {
        return std::make_shared<GlBackend>();
    }

    RenderBackend& Backend()
    {
        static std::mutex lock;
        const std::lock_guard<std::mutex> guard(lock);
        if (Current() == nullptr)
        {
            std::shared_ptr<RenderBackend> backend = CreateGlBackend();
            (void)backend->Initialize();
            Current() = std::move(backend);
        }
        return *Current();
    }

    void SetBackend(std::shared_ptr<RenderBackend> backend)
    {
        if (backend == nullptr)
        {
            return;
        }
        (void)backend->Initialize();
        Current() = std::move(backend);
    }
}
