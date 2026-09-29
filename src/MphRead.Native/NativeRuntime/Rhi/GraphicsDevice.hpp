#pragma once

#include "Backend.hpp"
#include "Bindings.hpp"
#include "Capabilities.hpp"
#include "CommandList.hpp"
#include "Pipeline.hpp"
#include "Resources.hpp"
#include "Swapchain.hpp"

#include <memory>

namespace MphRead::NativeRuntime::Rhi
{
    class GraphicsDevice
    {
    public:
        virtual ~GraphicsDevice() = default;
        GraphicsDevice(const GraphicsDevice&) = delete;
        GraphicsDevice& operator=(const GraphicsDevice&) = delete;
        GraphicsDevice(GraphicsDevice&&) = delete;
        GraphicsDevice& operator=(GraphicsDevice&&) = delete;

        [[nodiscard]] virtual GraphicsBackend GetBackend() const noexcept = 0;
        [[nodiscard]] virtual const Capabilities& GetCapabilities() const noexcept = 0;

        [[nodiscard]] virtual std::unique_ptr<Buffer> CreateBuffer(const BufferDesc& desc) = 0;
        [[nodiscard]] virtual std::unique_ptr<Texture> CreateTexture(const TextureDesc& desc) = 0;
        [[nodiscard]] virtual std::unique_ptr<TextureView> CreateTextureView(
            Texture& texture, const TextureViewDesc& desc) = 0;
        [[nodiscard]] virtual std::unique_ptr<Sampler> CreateSampler(const SamplerDesc& desc) = 0;
        [[nodiscard]] virtual std::unique_ptr<Shader> CreateShader(const ShaderDesc& desc) = 0;
        [[nodiscard]] virtual std::unique_ptr<BindingLayout> CreateBindingLayout(
            const BindingLayoutDesc& desc) = 0;
        [[nodiscard]] virtual std::unique_ptr<BindingSet> CreateBindingSet(
            const BindingSetDesc& desc) = 0;
        [[nodiscard]] virtual std::unique_ptr<GraphicsPipeline> CreateGraphicsPipeline(
            const GraphicsPipelineDesc& desc) = 0;
        [[nodiscard]] virtual std::unique_ptr<CommandList> CreateCommandList() = 0;

        // A texture whose handle the caller chose, for the few callers that
        // keep a reserved range of handles of their own (the map thumbnails).
        // Throws if a live texture already has that handle.
        [[nodiscard]] virtual std::unique_ptr<Texture> CreateTexture(
            const TextureDesc& desc, TextureHandle handle) = 0;
        // The live texture with this handle, whoever created it, or null.
        [[nodiscard]] virtual Texture* FindTexture(TextureHandle handle) noexcept = 0;
        // Hand a texture to the device to keep for as long as the device
        // lives: a texture written under a caller's reserved handle outlives
        // every scene that draws it.
        virtual Texture& RetainTexture(std::unique_ptr<Texture> texture) = 0;

        // Replace a texture's contents and, if the size differs, its extent.
        // The texture keeps its handle: whatever holds it keeps working.
        virtual void WriteTexture(Texture& texture, const TextureWrite& write) = 0;
        // Re-specify a render target's storage at a new extent, contents
        // undefined, keeping its handle and every view of it.
        virtual void ResizeTexture(Texture& texture, std::uint32_t width, std::uint32_t height) = 0;

        // Whether this combination of attachments can be rendered to on this
        // device: the question a driver answers about a depth texture it may
        // refuse to attach.
        [[nodiscard]] virtual bool CanRender(const RenderingInfo& info) = 0;
        // The depth buffer's bit depth as the device reports it for these
        // attachments, or 0 when it will not say.
        [[nodiscard]] virtual std::uint32_t DepthBits(const RenderingInfo& info) = 0;

    protected:
        GraphicsDevice() = default;
    };
}
