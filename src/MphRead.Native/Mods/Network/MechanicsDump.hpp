#pragma once

#include <string>

namespace MphRead
{
    class MechanicsDump final
    {
    public:
        MechanicsDump() = delete;
        MechanicsDump(const MechanicsDump&) = delete;
        MechanicsDump& operator=(const MechanicsDump&) = delete;
        MechanicsDump(MechanicsDump&&) = delete;
        MechanicsDump& operator=(MechanicsDump&&) = delete;

        static void Run();

    private:
        static void GetWeaponOutput(std::string& output);
        static void GetPlayerOutput(std::string& output);
        static void GetMiscOutput(std::string& output);
    };
}
