#pragma once

#include "../../../RendererGpuMesh.hpp"
#include "../CommandList.hpp"
#include "../GraphicsDevice.hpp"
#include "../SceneShaders.hpp"

#include <memory>
#include <span>

// The scene renderer's Vulkan resources: the four programs from the SPIR-V
// generated out of Shaders.cpp, and the mesh and transient geometry the
// renderer draws. The OpenGL counterparts are OpenGlShaderInterface and
// OpenGlGeometry; these take the command list the geometry is drawn through,
// since Vulkan has no context to draw into implicitly.
namespace MphRead::NativeRuntime::Rhi::Vulkan
{
    [[nodiscard]] std::unique_ptr<SceneShaderSet> CreateSceneShaderSet(GraphicsDevice& device,
        std::span<const float> toonTable, std::span<const float> shiftTable);
    [[nodiscard]] std::shared_ptr<MphRead::GpuMeshResource> CreateGpuMeshResource(
        GraphicsDevice& device, CommandList& commands, const MphRead::RendererGeometry& geometry);
    [[nodiscard]] std::shared_ptr<MphRead::TransientGeometryResource> CreateTransientGeometryResource(
        GraphicsDevice& device, CommandList& commands);

    // The window-level draws the launcher makes when the window presents
    // through Vulkan and there is no GL context: the photograph, and a
    // texture (the Skia UI) laid over whatever the frame holds. Everything
    // goes into the device's window target, in OpenGL's rows, as a scene's
    // window passes do.
    struct WindowQuadVertex final
    {
        float Position[2]{};
        float TexCoord[2]{};
        float TexCoord1[2]{};
    };

    class WindowUi final
    {
    public:
        explicit WindowUi(GraphicsDevice& device);
        ~WindowUi();
        WindowUi(const WindowUi&) = delete;
        WindowUi& operator=(const WindowUi&) = delete;

        // Open the window target at this size; clear it first when asked.
        void Begin(std::uint32_t width, std::uint32_t height, bool clear);
        void DrawTexture(const Texture& texture, const Sampler& sampler,
            std::span<const WindowQuadVertex, 4> strip, bool premultiplied);
        void DrawBackdrop(const Texture& photo, const Sampler& sampler,
            std::span<const WindowQuadVertex, 4> strip, float strength, float seconds);
        void End();

    private:
        struct Impl;
        std::unique_ptr<Impl> _impl;
    };
}
