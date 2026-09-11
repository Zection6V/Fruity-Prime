#pragma once

namespace MphRead::Mods
{
    class Headless final
    {
    public:
        Headless() = delete;
        Headless(const Headless&) = delete;
        Headless& operator=(const Headless&) = delete;

        static bool Active() noexcept;
        static void Enter() noexcept;

    private:
        static bool _active;
    };
}
