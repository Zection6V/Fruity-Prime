#pragma once

#include <cstdint>
#include <string>

namespace MphRead
{
    class ConsoleSetup final
    {
    public:
        [[nodiscard]] static std::string LaunchDirectory();

        // "Press any key to exit", where there may be no key to press.
        //
        // Console.ReadKey() throws when the process has no console or its
        // input is a pipe, and both are ordinary: the Windows game build is a
        // GUI binary with no console at all, and every command run by a script
        // or by the launcher has its streams captured. An unhandled exception
        // on the way out of a message that was only being polite is how a
        // Windows build ends up exiting with nothing on the screen.
        static void PauseIfInteractive();

        static void Run();
        static std::uint32_t GetLastError();

        ConsoleSetup() = delete;
        ConsoleSetup(const ConsoleSetup&) = delete;
        ConsoleSetup& operator=(const ConsoleSetup&) = delete;
    };
}
