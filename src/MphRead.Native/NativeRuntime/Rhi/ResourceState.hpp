#pragma once

#include <cstdint>
#include <type_traits>

namespace MphRead::NativeRuntime::Rhi
{
    enum class ResourceState : std::uint32_t
    {
        Undefined = 0,
        Common = 1U << 0,
        VertexBuffer = 1U << 1,
        IndexBuffer = 1U << 2,
        ConstantBuffer = 1U << 3,
        ShaderRead = 1U << 4,
        ShaderWrite = 1U << 5,
        ColorAttachment = 1U << 6,
        DepthStencilRead = 1U << 7,
        DepthStencilWrite = 1U << 8,
        CopySrc = 1U << 9,
        CopyDst = 1U << 10,
        Present = 1U << 11
    };

    [[nodiscard]] constexpr ResourceState operator|(ResourceState left, ResourceState right) noexcept
    {
        using U = std::underlying_type_t<ResourceState>;
        return static_cast<ResourceState>(static_cast<U>(left) | static_cast<U>(right));
    }

    [[nodiscard]] constexpr ResourceState operator&(ResourceState left, ResourceState right) noexcept
    {
        using U = std::underlying_type_t<ResourceState>;
        return static_cast<ResourceState>(static_cast<U>(left) & static_cast<U>(right));
    }

    constexpr ResourceState& operator|=(ResourceState& left, ResourceState right) noexcept
    {
        left = left | right;
        return left;
    }

    [[nodiscard]] constexpr bool HasAny(ResourceState value, ResourceState mask) noexcept
    {
        using U = std::underlying_type_t<ResourceState>;
        return (static_cast<U>(value) & static_cast<U>(mask)) != 0;
    }

    [[nodiscard]] constexpr bool HasSingleBit(ResourceState value) noexcept
    {
        using U = std::underlying_type_t<ResourceState>;
        const U bits = static_cast<U>(value);
        return bits != 0 && (bits & (bits - 1)) == 0;
    }

    [[nodiscard]] constexpr bool IsValidResourceState(ResourceState state) noexcept
    {
        if (state == ResourceState::Undefined)
        {
            return true;
        }

        const ResourceState exclusive = ResourceState::Common | ResourceState::Present;
        if (HasAny(state, exclusive))
        {
            return HasSingleBit(state);
        }

        const ResourceState writeStates = ResourceState::ShaderWrite
            | ResourceState::ColorAttachment
            | ResourceState::DepthStencilWrite
            | ResourceState::CopyDst;

        using U = std::underlying_type_t<ResourceState>;
        const U writes = static_cast<U>(state & writeStates);
        return writes == 0 || (writes & (writes - 1)) == 0;
    }

    [[nodiscard]] constexpr bool IsValidTransition(
        ResourceState before, ResourceState after) noexcept
    {
        return before != after
            && IsValidResourceState(before)
            && IsValidResourceState(after)
            && after != ResourceState::Undefined;
    }
}
