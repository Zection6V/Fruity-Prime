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

    protected:
        GraphicsDevice() = default;
    };
}
