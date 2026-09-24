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
#include <TargetConditionals.h>
#include <climits>
#include <mach-o/dyld.h>
#endif
#include <csignal>
#include <fstream>
#include <string>
#include <sys/utsname.h>
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

    bool IsMacOS()
    {
#if defined(__APPLE__)
        return TARGET_OS_OSX != 0;
#else
        return false;
#endif
    }

    std::string EnvironmentCurrentDirectory()
    {
        std::error_code error;
        const std::filesystem::path directory = std::filesystem::current_path(error);
        if (error)
        {
            return std::string();
        }
        const std::u8string text = directory.u8string();
        return std::string(text.begin(), text.end());
    }

    std::string RuntimeInformationProcessArchitecture()
    {
#if defined(__x86_64__) || defined(_M_X64)
        return "X64";
#elif defined(__i386__) || defined(_M_IX86)
        return "X86";
#elif defined(__aarch64__) || defined(_M_ARM64)
        return "Arm64";
#elif defined(__arm__) || defined(_M_ARM)
#if defined(__ARM_ARCH_6__)
        return "Armv6";
#else
        return "Arm";
#endif
#elif defined(__wasm__)
        return "Wasm";
#elif defined(__s390x__)
        return "S390x";
#elif defined(__loongarch64)
        return "LoongArch64";
#elif defined(__powerpc64__) && defined(__LITTLE_ENDIAN__)
        return "Ppc64le";
#else
        return "Unknown";
#endif
    }

    std::string RuntimeInformationOSArchitecture()
    {
#if defined(_WIN32)
        SYSTEM_INFO info{};
        ::GetNativeSystemInfo(&info);
        switch (info.wProcessorArchitecture)
        {
        case PROCESSOR_ARCHITECTURE_AMD64:
            return "X64";
        case PROCESSOR_ARCHITECTURE_INTEL:
            return "X86";
        case PROCESSOR_ARCHITECTURE_ARM:
            return "Arm";
        case PROCESSOR_ARCHITECTURE_ARM64:
            return "Arm64";
        default:
            return RuntimeInformationProcessArchitecture();
        }
#else
        struct utsname info{};
        if (::uname(&info) != 0)
        {
            return RuntimeInformationProcessArchitecture();
        }
        const std::string machine(info.machine);
        if (machine == "x86_64" || machine == "amd64")
        {
            return "X64";
        }
        if (machine == "i386" || machine == "i486"
            || machine == "i586" || machine == "i686")
        {
            return "X86";
        }
        if (machine == "aarch64" || machine == "arm64")
        {
            return "Arm64";
        }
        if (machine.rfind("armv6", 0) == 0)
        {
            return "Armv6";
        }
        if (machine.rfind("arm", 0) == 0)
        {
            return "Arm";
        }
        if (machine == "s390x")
        {
            return "S390x";
        }
        if (machine == "loongarch64")
        {
            return "LoongArch64";
        }
        if (machine == "ppc64le")
        {
            return "Ppc64le";
        }
        return RuntimeInformationProcessArchitecture();
#endif
    }

    std::string RuntimeInformationOSDescription()
    {
#if defined(_WIN32)
        // "Microsoft Windows <major>.<minor>.<build>", as .NET spells it.
        using RtlGetVersionFn = LONG(WINAPI*)(PRTL_OSVERSIONINFOW);
        RTL_OSVERSIONINFOW version{};
        version.dwOSVersionInfoSize = sizeof(version);
        const HMODULE ntdll = ::GetModuleHandleW(L"ntdll.dll");
        if (ntdll != nullptr)
        {
            const auto getVersion = reinterpret_cast<RtlGetVersionFn>(
                reinterpret_cast<void*>(::GetProcAddress(ntdll, "RtlGetVersion")));
            if (getVersion != nullptr && getVersion(&version) == 0)
            {
                return "Microsoft Windows " + std::to_string(version.dwMajorVersion) + "."
                    + std::to_string(version.dwMinorVersion) + "."
                    + std::to_string(version.dwBuildNumber);
            }
        }
        return "Microsoft Windows";
#else
        // "<sysname> <release> <version>", which is what .NET builds from uname.
        struct utsname info{};
        if (::uname(&info) != 0)
        {
            return std::string();
        }
        return std::string(info.sysname) + " " + info.release + " " + info.version;
#endif
    }

    std::string RuntimeInformationRuntimeIdentifier()
    {
        std::string architecture = RuntimeInformationProcessArchitecture();
        for (char& value : architecture)
        {
            value = static_cast<char>(
                value >= 'A' && value <= 'Z' ? value - 'A' + 'a' : value);
        }
#if defined(__ANDROID__)
        const std::string platform = "android";
#elif defined(_WIN32)
        const std::string platform = "win";
#elif defined(__APPLE__)
        const std::string platform = "osx";
#else
        const std::string platform = "linux";
#endif
        return platform + "-" + architecture;
    }

    std::string RuntimeInformationFrameworkDescription()
    {
        return ".NET native";
    }

    std::string EnvironmentUserProfile()
    {
#if defined(_WIN32)
        // Environment.GetFolderPath reads the known folder; USERPROFILE is
        // what .NET falls back to and what every shell here sets.
        std::vector<wchar_t> buffer(260);
        for (;;)
        {
            const DWORD length = ::GetEnvironmentVariableW(
                L"USERPROFILE", buffer.data(), static_cast<DWORD>(buffer.size()));
            if (length == 0)
            {
                return std::string();
            }
            if (length < buffer.size())
            {
                const std::wstring wide(buffer.data(), length);
                const std::u8string text = std::filesystem::path(wide).u8string();
                return std::string(text.begin(), text.end());
            }
            buffer.resize(length);
        }
#else
        const char* const home = std::getenv("HOME");
        return home == nullptr ? std::string() : std::string(home);
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
