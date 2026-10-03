#pragma once

#include "Resources.hpp"

namespace MphRead::NativeRuntime::Rhi
{
    [[nodiscard]] constexpr bool IsDepthStencilFormat(TextureFormat format) noexcept
    {
        return format == TextureFormat::D16Unorm || format == TextureFormat::D24UnormS8Uint
            || format == TextureFormat::D32Float || format == TextureFormat::D32FloatS8Uint;
    }

    // Descriptor usage is the resource's lifetime contract, including initial
    // state and explicit transitions. Native handles are never needed here.
    [[nodiscard]] constexpr bool IsValidBufferState(const BufferDesc& desc, ResourceState state) noexcept
    {
        constexpr auto known = BufferUsage::Vertex | BufferUsage::Index | BufferUsage::Uniform
            | BufferUsage::Storage | BufferUsage::TransferSrc | BufferUsage::TransferDst;
        const auto usage = static_cast<std::uint32_t>(desc.usage);
        if (!usage || (usage & ~static_cast<std::uint32_t>(known)) || !IsValidBufferState(state)) return false;
        const auto allows = [usage, state](ResourceState required, BufferUsage flag) {
            return !HasAny(state, required) || (usage & static_cast<std::uint32_t>(flag)) != 0;
        };
        return allows(ResourceState::VertexBuffer, BufferUsage::Vertex)
            && allows(ResourceState::IndexBuffer, BufferUsage::Index)
            && allows(ResourceState::ConstantBuffer, BufferUsage::Uniform)
            && allows(ResourceState::ShaderRead | ResourceState::ShaderWrite, BufferUsage::Storage)
            && allows(ResourceState::CopySrc, BufferUsage::TransferSrc)
            && allows(ResourceState::CopyDst, BufferUsage::TransferDst);
    }

    [[nodiscard]] constexpr bool IsValidTextureState(const TextureDesc& desc, ResourceState state) noexcept
    {
        constexpr auto known = TextureUsage::Sampled | TextureUsage::Storage | TextureUsage::ColorAttachment
            | TextureUsage::DepthStencilAttachment | TextureUsage::TransferSrc | TextureUsage::TransferDst;
        const auto usage = static_cast<std::uint32_t>(desc.usage);
        const auto has = [usage](TextureUsage flag) { return (usage & static_cast<std::uint32_t>(flag)) != 0; };
        if (!usage || (usage & ~static_cast<std::uint32_t>(known)) || !IsValidTextureState(state)
            || desc.format == TextureFormat::Undefined || desc.format > TextureFormat::RGB8Unorm) return false;
        const bool depth = IsDepthStencilFormat(desc.format);
        if ((depth && has(TextureUsage::ColorAttachment)) || (!depth && has(TextureUsage::DepthStencilAttachment))) return false;
        // Present belongs to swapchain drawables, never to CreateTexture's
        // independently allocated image, even when that image is a color target.
        if (HasAny(state, ResourceState::Present)) return false;
        const auto allows = [state, &has](ResourceState required, TextureUsage flag) {
            return !HasAny(state, required) || has(flag);
        };
        return (!HasAny(state, ResourceState::ShaderRead) || has(TextureUsage::Sampled | TextureUsage::Storage))
            && allows(ResourceState::ShaderWrite, TextureUsage::Storage)
            && allows(ResourceState::ColorAttachment, TextureUsage::ColorAttachment)
            && allows(ResourceState::DepthStencilRead | ResourceState::DepthStencilWrite, TextureUsage::DepthStencilAttachment)
            && allows(ResourceState::CopySrc, TextureUsage::TransferSrc)
            && allows(ResourceState::CopyDst, TextureUsage::TransferDst);
    }
}
