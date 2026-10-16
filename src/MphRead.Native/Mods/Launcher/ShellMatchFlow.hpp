#pragma once

#include "Portable/LaunchPlan.hpp"

#include <functional>
#include <memory>
#include <string>

namespace MphRead
{
    class MenuSettings;
    class RenderWindow;
}

namespace MphRead::Mods::Launcher
{
    // Owns deferred match starts and the last offline plan used by Play Another.
    class ShellMatchFlow final
    {
    public:
        using StartCallback = std::function<bool(MphRead::RenderWindow&)>;

        ShellMatchFlow() = delete;

        static void Queue(LaunchPlan plan);
        static void Queue(StartCallback start);
        [[nodiscard]] static bool HasPending() noexcept;
        // Consumes the queued request; call only after HasPending() is true.
        [[nodiscard]] static bool StartPending(
            MphRead::RenderWindow& window,
            std::shared_ptr<MphRead::MenuSettings> settings);
        [[nodiscard]] static bool CanPlayAnother() noexcept;
        [[nodiscard]] static bool QueueAnother(std::string roomKey);
        static void ClearPending() noexcept;
    };
}
