#pragma once

#include <atomic>
#include <cstdint>

namespace MphRead::NativeRuntime::Rhi::FullscreenExclusive
{
    // Which monitor the game window is in exclusive fullscreen on (an
    // HMONITOR on Windows), or null for windowed and borderless. Written by
    // the window, read by the Vulkan swapchain, which recreates itself with
    // VK_EXT_full_screen_exclusive in application-controlled mode and takes
    // the display -- the PUBG/Fortnite kind of fullscreen, where the
    // compositor is out of the path entirely. Active tracks temporary focus
    // loss separately so it need not rebuild that swapchain configuration.
    // The generation tells the swapchain either request changed.
    namespace Detail
    {
        inline std::atomic<void*> monitor{nullptr};
        inline std::atomic<bool> active{true};
        inline std::atomic<std::uint64_t> generation{0};
    }

    inline void Request(void* monitor) noexcept
    {
        if (Detail::monitor.exchange(monitor, std::memory_order_acq_rel) != monitor)
        {
            Detail::generation.fetch_add(1, std::memory_order_acq_rel);
        }
    }

    [[nodiscard]] inline void* Monitor() noexcept
    {
        return Detail::monitor.load(std::memory_order_acquire);
    }

    // Focus changes ownership, not the swapchain's fullscreen configuration.
    inline void Active(bool active) noexcept
    {
        if (Detail::active.exchange(active, std::memory_order_acq_rel) != active)
            Detail::generation.fetch_add(1, std::memory_order_acq_rel);
    }

    [[nodiscard]] inline bool Active() noexcept
    {
        return Detail::active.load(std::memory_order_acquire);
    }

    [[nodiscard]] inline std::uint64_t Generation() noexcept
    {
        return Detail::generation.load(std::memory_order_acquire);
    }
}
