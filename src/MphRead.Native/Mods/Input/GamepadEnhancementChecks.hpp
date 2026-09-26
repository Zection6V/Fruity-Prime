#pragma once

namespace MphRead::Mods::Input
{
    class GamepadEnhancementChecks final
    {
    public:
        GamepadEnhancementChecks() = delete;

        static void Run();

    private:
        static void CheckCalibration();
        static void CheckMapping();
        static void CheckProfiles();
    };
}
