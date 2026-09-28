#pragma once

#include "Resources.hpp"

#include <cstdint>

namespace MphRead::NativeRuntime::Rhi
{
    enum class PresentMode : std::uint8_t
    {
        Immediate,
        Fifo,
        Mailbox
    };

    struct SwapchainDesc final
    {
        std::uint32_t width = 1;
        std::uint32_t height = 1;
        std::uint32_t imageCount = 2;
        TextureFormat format = TextureFormat::BGRA8Unorm;
        PresentMode presentMode = PresentMode::Fifo;

        bool operator==(const SwapchainDesc&) const = default;
    };

    class Swapchain
    {
    public:
        virtual ~Swapchain() = default;
        Swapchain(const Swapchain&) = delete;
        Swapchain& operator=(const Swapchain&) = delete;
        Swapchain(Swapchain&&) = delete;
        Swapchain& operator=(Swapchain&&) = delete;

        [[nodiscard]] virtual const SwapchainDesc& Desc() const noexcept = 0;
        virtual void Resize(std::uint32_t width, std::uint32_t height) = 0;
        [[nodiscard]] virtual Texture& AcquireNextTexture() = 0;
        virtual void Present() = 0;

    protected:
        Swapchain() = default;
    };
}
