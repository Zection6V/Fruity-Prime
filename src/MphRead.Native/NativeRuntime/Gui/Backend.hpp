#pragma once

// The drawing surface the launcher's controls are painted on, behind an
// interface so the graphics API is a choice rather than an assumption.
//
// Everything above this line -- the control tree, layout, text shaping, the
// views themselves -- speaks only in rectangles, colours, textures and quads.
// A backend turns those into API calls. GlBackend.cpp is the one that exists;
// a Vulkan or D3D12 one implements the same interface and nothing above it
// moves.
//
// Nothing here assumes immediate mode: geometry arrives as batches of vertices
// that a backend is free to put in a buffer, textures are opaque handles it
// creates and destroys, and every frame has a beginning and an end. The
// coordinate system is stated rather than inherited -- pixels, origin at the
// top left, y downwards -- because the APIs disagree about it.

#include "Element.hpp"

#include <cstdint>
#include <memory>
#include <optional>
#include <span>

namespace MphRead::NativeRuntime::Gui
{
    // A texture owned by the backend. Zero is "none".
    using TextureHandle = std::uint32_t;

    // One corner of a textured quad, in pixels and in texture coordinates.
    struct TexturedVertex final
    {
        float X = 0.0F;
        float Y = 0.0F;
        float U = 0.0F;
        float V = 0.0F;
    };

    class RenderBackend
    {
    public:
        virtual ~RenderBackend() = default;

        // A name for the log, such as "OpenGL" or "Vulkan".
        [[nodiscard]] virtual const char* Name() const noexcept = 0;

        // Whatever the backend needs once a surface exists. False means this
        // backend cannot run here and another should be tried.
        [[nodiscard]] virtual bool Initialize() = 0;
        virtual void Shutdown() = 0;

        // A frame's worth of drawing, into a surface this many pixels across.
        // Coordinates are pixels with the origin at the top left.
        virtual void BeginFrame(std::int32_t width, std::int32_t height) = 0;
        virtual void EndFrame() = 0;

        // Nothing outside the rectangle is drawn; no value clears the clip.
        virtual void SetClip(const std::optional<Rect>& rect) = 0;

        virtual void FillRoundedRect(Rect rect, double radius, Color color) = 0;
        // A border drawn inside the rectangle.
        virtual void StrokeRect(Rect rect, Thickness thickness, Color color) = 0;
        // Quads in groups of four vertices, all from one texture, all tinted
        // the same. This is how both text runs and images arrive.
        virtual void DrawTexturedQuads(TextureHandle texture,
            std::span<const TexturedVertex> vertices, Color tint) = 0;

        // A single-channel texture, for glyph coverage.
        [[nodiscard]] virtual TextureHandle CreateAlphaTexture(
            std::int32_t width, std::int32_t height) = 0;
        virtual void UpdateAlphaTexture(TextureHandle texture, std::int32_t x,
            std::int32_t y, std::int32_t width, std::int32_t height,
            const std::uint8_t* pixels) = 0;
        // A four-channel texture, for images.
        [[nodiscard]] virtual TextureHandle CreateColorTexture(std::int32_t width,
            std::int32_t height, const std::uint8_t* rgba) = 0;
        virtual void DestroyTexture(TextureHandle texture) = 0;
    };

    // The backend in use. The first call installs the default one, which is
    // OpenGL, because that is the context the launcher's window already has.
    [[nodiscard]] RenderBackend& Backend();
    // Installs another, before any drawing has happened.
    void SetBackend(std::shared_ptr<RenderBackend> backend);

    // The default, for a host that wants it by name.
    [[nodiscard]] std::shared_ptr<RenderBackend> CreateGlBackend();
}
