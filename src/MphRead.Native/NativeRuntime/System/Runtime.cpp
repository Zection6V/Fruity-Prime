#include "Runtime.hpp"

#include "Exceptions.hpp"

#include <atomic>
#include <cstdio>
#include <cstdlib>

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
