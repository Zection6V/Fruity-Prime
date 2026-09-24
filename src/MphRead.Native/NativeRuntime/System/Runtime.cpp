#include "Runtime.hpp"

#include "Exceptions.hpp"

#include <atomic>
#include <cstdio>
#include <cstdlib>
#include <filesystem>
#include <string>
#include <system_error>
#include <vector>

#if defined(_WIN32)
#ifndef NOMINMAX
#define NOMINMAX
#endif
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>
#include <intrin.h>
#else
#if defined(__APPLE__)
#include <climits>
#include <mach-o/dyld.h>
#endif
#include <csignal>
#include <fstream>
#include <string>
#endif

namespace MphRead::NativeRuntime
{
    namespace
    {
        std::atomic<std::int32_t> ExitCode{0};
    }

    std::string AppContextBaseDirectory()
    {
        std::filesystem::path executable;
#if defined(_WIN32)
        std::vector<wchar_t> buffer(260);
        for (;;)
        {
            const DWORD length = ::GetModuleFileNameW(nullptr, buffer.data(), static_cast<DWORD>(buffer.size()));
            if (length == 0)
            {
                break;
            }
            if (length < buffer.size())
            {
                executable = std::filesystem::path(std::wstring(buffer.data(), length));
                break;
            }
            buffer.resize(buffer.size() * 2U);
        }
#elif defined(__APPLE__)
        char path[PATH_MAX];
        std::uint32_t length = PATH_MAX;
        if (::_NSGetExecutablePath(path, &length) == 0)
        {
            std::error_code error;
            executable = std::filesystem::canonical(path, error);
        }
#else
        std::error_code error;
        executable = std::filesystem::read_symlink("/proc/self/exe", error);
#endif
        std::filesystem::path directory = executable.parent_path();
        if (directory.empty())
        {
            std::error_code error;
            directory = std::filesystem::current_path(error);
        }
        const std::u8string text = (directory / "").u8string();
        return std::string(text.begin(), text.end());
    }

    void ForceFullGc()
    {
        // There is no garbage-collected heap: every object is freed when its last
        // owner releases it, so a forced collection has nothing left to collect.
    }

    void SetSustainedLowLatencyGc()
    {
        // No collector exists whose pauses the latency mode would govern.
    }

    bool DebuggerAttached()
    {
#if defined(_WIN32)
        return ::IsDebuggerPresent() != FALSE;
#else
        std::ifstream status("/proc/self/status");
        std::string line;
        while (std::getline(status, line))
        {
            constexpr const char* key = "TracerPid:";
            if (line.rfind(key, 0) == 0)
            {
                return std::strtol(line.c_str() + 10, nullptr, 10) != 0;
            }
        }
        return false;
#endif
    }

    void DebuggerBreak()
    {
        // Debugger.Break() on .NET (Core) signals a user breakpoint to an
        // attached debugger and is a no-op otherwise -- there is no JIT-attach
        // prompt as on .NET Framework, and nothing that ends the process.
        if (!DebuggerAttached())
        {
            return;
        }
#if defined(_WIN32)
        __debugbreak();
#else
        std::raise(SIGTRAP);
#endif
    }

    void DebugAssert(bool condition)
    {
        if (condition)
        {
            return;
        }
        // DebugProvider.FailCore: break into an attached debugger, otherwise
        // Environment.FailFast.
        if (DebuggerAttached())
        {
            DebuggerBreak();
            return;
        }
        std::fputs("Process terminated. Assertion failed.\n", stderr);
        std::fflush(stderr);
        std::abort();
    }

    bool IsAndroid()
    {
#if defined(__ANDROID__)
        return true;
#else
        return false;
#endif
    }

    std::int32_t EnvironmentExitCode() noexcept
    {
        return ExitCode.load();
    }

    void SetEnvironmentExitCode(std::int32_t value) noexcept
    {
        ExitCode.store(value);
    }

    void ThrowDivideByZeroException()
    {
        throw System::DivideByZeroException();
    }

    void ThrowListIndexOutOfRange()
    {
        throw System::ArgumentOutOfRangeException();
    }
}
