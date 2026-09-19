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
    };
}
