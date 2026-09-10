#pragma once

#include <functional>
#include <memory>

namespace MphRead::Mods
{
    class ShutdownSignals final
    {
    public:
        ShutdownSignals();
        ~ShutdownSignals() noexcept(false);

        ShutdownSignals(const ShutdownSignals&) = delete;
        ShutdownSignals& operator=(const ShutdownSignals&) = delete;
        ShutdownSignals(ShutdownSignals&&) = delete;
        ShutdownSignals& operator=(ShutdownSignals&&) = delete;

        void OnShutdown(std::function<void()> action);
        void Dispose();

    private:
        struct Impl;
        std::unique_ptr<Impl> _impl;
    };
}
