#pragma once

#include "../Capabilities.hpp"
#include <memory>
#include <string>

namespace MphRead::NativeRuntime::Rhi::Vulkan
{
    // Native API objects stay behind the backend's implementation boundary.
    class Context final
    {
    public:
        explicit Context(bool validation);
        ~Context();
        Context(const Context&) = delete;
        Context& operator=(const Context&) = delete;
        [[nodiscard]] const Capabilities& Caps() const noexcept;
        [[nodiscard]] const std::string& DeviceName() const noexcept;
        [[nodiscard]] unsigned ValidationErrors() const noexcept;
        [[nodiscard]] bool ValidationEnabled() const noexcept;
        void WaitIdle();
        void Shutdown();
        void CheckCommandBufferDebugName();
    private:
        struct Impl;
        std::unique_ptr<Impl> _impl;
    };

    // Explicit diagnostic only; does not select a backend for ordinary games.
    int RunFoundationCheck();
}
