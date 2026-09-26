#pragma once

namespace MphRead::Mods::Launcher
{
    class TextLauncher final
    {
    public:
        TextLauncher() = delete;
        TextLauncher(const TextLauncher&) = delete;
        TextLauncher& operator=(const TextLauncher&) = delete;

        static void Run();

        // Whether stdin has ended. Taking EOF as the default answer is right
        // for one question and wrong for a menu -- a process with nobody at it
        // printing its front screen forever -- so every menu reads this and
        // leaves.
        [[nodiscard]] static bool InputEnded() noexcept { return _inputEnded; }

    private:
        static inline bool _inputEnded = false;
        friend struct TextLauncherAccess;
    };
}
