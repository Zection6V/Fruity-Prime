#pragma once

#include "Resources.hpp"
#include "BackendError.hpp"

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

    enum class PresentationStatus : std::uint8_t
    { Ready, ResizeRequired, TemporarilyUnavailable, SurfaceLost, DeviceLost };

    struct AcquireResult final
    {
        PresentationStatus status = PresentationStatus::Ready;
        Texture* texture = nullptr;
    };
    struct PresentResult final { PresentationStatus status = PresentationStatus::Ready; };

    struct PresentationCapabilities final
    {
        bool immediate = false;
        bool fifo = true;
        bool mailbox = false;
        std::uint32_t minImageCount = 1;
        // Zero means that the surface does not impose a maximum.
        std::uint32_t maxImageCount = 0;
    };

    [[nodiscard]] inline PresentationStatus PresentationFailure(const BackendError& error)
    {
        if (error.Kind() == BackendErrorKind::DeviceLost) return PresentationStatus::DeviceLost;
        if (error.Kind() == BackendErrorKind::SurfaceLost) return PresentationStatus::SurfaceLost;
        throw error;
    }

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
        virtual void SetPresentMode(PresentMode mode) = 0;
        virtual void Present() = 0;
        // Nonblocking for unavailable/minimized surfaces. The synchronous
        // diagnostic entry point above remains available to capture callers.
        [[nodiscard]] virtual AcquireResult TryAcquireTexture()
        {
            try { return {PresentationStatus::Ready, &AcquireNextTexture()}; }
            catch (const BackendError& error) { return {PresentationFailure(error), nullptr}; }
        }
        [[nodiscard]] virtual PresentResult TryPresent()
        {
            try { Present(); return {}; }
            catch (const BackendError& error) { return {PresentationFailure(error)}; }
        }
        [[nodiscard]] virtual PresentationCapabilities PresentationCaps() const noexcept = 0;
        [[nodiscard]] virtual PresentMode RequestedPresentMode() const noexcept = 0;

    protected:
        Swapchain() = default;
    };
}
