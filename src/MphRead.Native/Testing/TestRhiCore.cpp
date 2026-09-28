#include "../NativeRuntime/Rhi/Backend.hpp"
#include "../NativeRuntime/Rhi/Bindings.hpp"
#include "../NativeRuntime/Rhi/Capabilities.hpp"
#include "../NativeRuntime/Rhi/CommandList.hpp"
#include "../NativeRuntime/Rhi/GraphicsDevice.hpp"
#include "../NativeRuntime/Rhi/Pipeline.hpp"
#include "../NativeRuntime/Rhi/Resources.hpp"
#include "../NativeRuntime/Rhi/ResourceState.hpp"
#include "../NativeRuntime/Rhi/Swapchain.hpp"

#include <memory>
#include <type_traits>

namespace
{
    using namespace MphRead::NativeRuntime::Rhi;

    constexpr BufferDesc BufferA{
        4096,
        BufferUsage::Vertex | BufferUsage::TransferDst,
        MemoryUsage::GpuOnly,
        ResourceState::CopyDst
    };
    constexpr BufferDesc BufferB = BufferA;
    static_assert(BufferA == BufferB);

    constexpr TextureViewDesc ViewA{
        TextureFormat::RGBA8Unorm,
        1,
        2,
        3,
        4
    };
    constexpr TextureViewDesc ViewB = ViewA;
    static_assert(ViewA == ViewB);

    constexpr SwapchainDesc SwapchainA{
        1280,
        720,
        3,
        TextureFormat::BGRA8Srgb,
        PresentMode::Fifo
    };
    constexpr SwapchainDesc SwapchainB = SwapchainA;
    static_assert(SwapchainA == SwapchainB);

    static_assert(IsValidResourceState(
        ResourceState::VertexBuffer | ResourceState::ShaderRead | ResourceState::CopySrc));
    static_assert(!IsValidResourceState(
        ResourceState::ColorAttachment | ResourceState::CopyDst));
    static_assert(IsValidTransition(ResourceState::Undefined, ResourceState::CopyDst));
    static_assert(!IsValidTransition(ResourceState::CopyDst, ResourceState::Undefined));

    static_assert(std::is_abstract_v<GraphicsDevice>);
    static_assert(std::is_abstract_v<CommandList>);
    static_assert(std::is_abstract_v<Swapchain>);
    static_assert(!std::is_copy_constructible_v<Buffer>);
    static_assert(!std::is_copy_constructible_v<Texture>);
    static_assert(!std::is_copy_constructible_v<GraphicsPipeline>);

    using BufferOwner = decltype(std::declval<GraphicsDevice&>().CreateBuffer(BufferA));
    static_assert(std::is_same_v<BufferOwner, std::unique_ptr<Buffer>>);
}
