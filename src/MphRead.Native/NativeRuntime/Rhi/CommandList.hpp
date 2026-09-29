#pragma once

#include "Bindings.hpp"
#include "Pipeline.hpp"
#include "ResourceState.hpp"
#include "Resources.hpp"

#include <cstdint>
#include <span>

namespace MphRead::NativeRuntime::Rhi
{
    enum class LoadOp : std::uint8_t
    {
        Load,
        Clear,
        DontCare
    };

    enum class StoreOp : std::uint8_t
    {
        Store,
        DontCare
    };

    enum class IndexType : std::uint8_t
    {
        UInt16,
        UInt32
    };

    struct Viewport final
    {
        float x = 0.0F;
        float y = 0.0F;
        float width = 0.0F;
        float height = 0.0F;
        float minDepth = 0.0F;
        float maxDepth = 1.0F;

        bool operator==(const Viewport&) const = default;
    };

    struct Scissor final
    {
        std::int32_t x = 0;
        std::int32_t y = 0;
        std::uint32_t width = 0;
        std::uint32_t height = 0;

        bool operator==(const Scissor&) const = default;
    };

    struct ClearColor final
    {
        float red = 0.0F;
        float green = 0.0F;
        float blue = 0.0F;
        float alpha = 0.0F;

        bool operator==(const ClearColor&) const = default;
    };

    struct RenderingColorAttachment final
    {
        const TextureView* view = nullptr;
        LoadOp loadOp = LoadOp::Load;
        StoreOp storeOp = StoreOp::Store;
        ClearColor clearValue{};

        bool operator==(const RenderingColorAttachment&) const = default;
    };

    struct RenderingDepthStencilAttachment final
    {
        const TextureView* view = nullptr;
        LoadOp depthLoadOp = LoadOp::Load;
        StoreOp depthStoreOp = StoreOp::Store;
        LoadOp stencilLoadOp = LoadOp::Load;
        StoreOp stencilStoreOp = StoreOp::Store;
        float clearDepth = 1.0F;
        std::uint32_t clearStencil = 0;

        bool operator==(const RenderingDepthStencilAttachment&) const = default;
    };

    struct RenderingInfo final
    {
        std::uint32_t width = 0;
        std::uint32_t height = 0;
        std::span<const RenderingColorAttachment> colorAttachments{};
        const RenderingDepthStencilAttachment* depthStencilAttachment = nullptr;
        // Render to the window's own surface rather than to attachments; the
        // attachment spans must then be empty.
        bool swapchain = false;
    };

    struct BufferTextureCopy final
    {
        std::uint64_t bufferOffset = 0;
        std::uint32_t bytesPerRow = 0;
        std::uint32_t rowsPerImage = 0;
        std::uint32_t mipLevel = 0;
        std::uint32_t arrayLayer = 0;
        std::uint32_t x = 0;
        std::uint32_t y = 0;
        std::uint32_t z = 0;
        std::uint32_t width = 1;
        std::uint32_t height = 1;
        std::uint32_t depth = 1;

        bool operator==(const BufferTextureCopy&) const = default;
    };

    class CommandList
    {
    public:
        virtual ~CommandList() = default;
        CommandList(const CommandList&) = delete;
        CommandList& operator=(const CommandList&) = delete;
        CommandList(CommandList&&) = delete;
        CommandList& operator=(CommandList&&) = delete;

        virtual void Begin() = 0;
        virtual void End() = 0;

        virtual void BeginRendering(const RenderingInfo& info) = 0;
        virtual void EndRendering() = 0;

        virtual void SetPipeline(const GraphicsPipeline& pipeline) = 0;
        virtual void SetViewport(const Viewport& viewport) = 0;
        virtual void SetScissor(const Scissor& scissor) = 0;

        virtual void SetVertexBuffer(
            std::uint32_t slot, const Buffer& buffer, std::uint64_t offset = 0) = 0;
        virtual void SetIndexBuffer(
            const Buffer& buffer, IndexType indexType, std::uint64_t offset = 0) = 0;

        virtual void SetBindingSet(std::uint32_t slot, const BindingSet& bindingSet) = 0;
        virtual void SetStencilReference(std::uint32_t reference) = 0;

        virtual void Draw(std::uint32_t vertexCount, std::uint32_t instanceCount = 1,
            std::uint32_t firstVertex = 0, std::uint32_t firstInstance = 0) = 0;
        virtual void DrawIndexed(std::uint32_t indexCount, std::uint32_t instanceCount = 1,
            std::uint32_t firstIndex = 0, std::int32_t vertexOffset = 0,
            std::uint32_t firstInstance = 0) = 0;

        virtual void CopyBuffer(const Buffer& source, std::uint64_t sourceOffset,
            Buffer& destination, std::uint64_t destinationOffset, std::uint64_t size) = 0;
        virtual void CopyBufferToTexture(
            const Buffer& source, Texture& destination, const BufferTextureCopy& region) = 0;
        virtual void CopyTextureToBuffer(
            const Texture& source, Buffer& destination, const BufferTextureCopy& region) = 0;

        virtual void Transition(
            Buffer& resource, ResourceState before, ResourceState after) = 0;
        virtual void Transition(
            Texture& resource, ResourceState before, ResourceState after) = 0;

        // Push-descriptor style texture binding: bind a texture and the
        // sampler it is read through to a shader texture slot for the draws
        // that follow. A null texture unbinds the slot.
        virtual void BindSampledTexture(
            std::uint32_t slot, const Texture* texture, const Sampler* sampler) = 0;
        // Read back a region of a rendering target's colour: tightly packed
        // rows, bottom row first. Waits for the GPU.
        virtual void ReadColor(const RenderingInfo& info, std::uint32_t x, std::uint32_t y,
            std::uint32_t width, std::uint32_t height, TextureFormat format, void* destination) = 0;
        // Copy a region of the current colour attachment into a texture.
        virtual void CopyColorAttachmentToTexture(
            Texture& destination, std::uint32_t width, std::uint32_t height) = 0;

    protected:
        CommandList() = default;
    };
}
