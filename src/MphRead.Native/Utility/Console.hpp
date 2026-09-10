#pragma once

#include <cstdint>
#include <string>

namespace MphRead
{
    class ConsoleSetup final
    {
    public:
        [[nodiscard]] static std::string LaunchDirectory();
        static void Run();
        static std::uint32_t GetLastError();

        ConsoleSetup() = delete;
        ConsoleSetup(const ConsoleSetup&) = delete;
        ConsoleSetup& operator=(const ConsoleSetup&) = delete;
    };
}
