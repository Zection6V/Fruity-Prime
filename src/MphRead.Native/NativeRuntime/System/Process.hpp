#pragma once

// System.Diagnostics.Process, as far as a program started and watched from
// here needs it: start with an argument list, ask whether it has exited and
// how, and kill it with its children.

#include <cstdint>
#include <memory>
#include <string>
#include <vector>

namespace MphRead::NativeRuntime
{
    struct ProcessStartInfo
    {
        std::string FileName{};
        std::vector<std::string> ArgumentList{};
        std::string WorkingDirectory{};
        // Windows: ShellExecuteEx, which gives a console program a window of
        // its own. Elsewhere .NET starts the file the same way either way.
        bool UseShellExecute = false;
        bool CreateNoWindow = false;
    };

    class Process final
    {
    public:
        // Process.Start(startInfo): throws when the file cannot be started.
        [[nodiscard]] static std::shared_ptr<Process> Start(const ProcessStartInfo& startInfo);

        Process(const Process&) = delete;
        Process& operator=(const Process&) = delete;
        ~Process();

        [[nodiscard]] bool HasExited();
        // Valid once HasExited; InvalidOperationException before.
        [[nodiscard]] std::int32_t ExitCode();
        // Process.Kill(entireProcessTree).
        void Kill(bool entireProcessTree);
        void Dispose();

    private:
        Process() = default;

#if defined(_WIN32)
        void* _handle = nullptr;
        std::uint32_t _id = 0;
#else
        std::int32_t _pid = -1;
        bool _reaped = false;
#endif
        bool _exited = false;
        std::int32_t _exitCode = 0;
    };

    // IPGlobalProperties.GetIPGlobalProperties().GetActiveUdpListeners(),
    // ports only: IPv4 and IPv6. Throws where the platform will not say.
    [[nodiscard]] std::vector<std::int32_t> ActiveUdpListenerPorts();
}
