#pragma once

// System.GC, System.Runtime.GCSettings, System.Diagnostics.Debugger/Debug,
// System.Environment and System.OperatingSystem: the members the game calls.

#include <cstdint>
#include <optional>
#include <string>
#include <string_view>

namespace MphRead::NativeRuntime
{
    // GC.Collect(GC.MaxGeneration, GCCollectionMode.Forced, blocking: true, compacting: true).
    void ForceFullGc();
    // GCSettings.LatencyMode = GCLatencyMode.SustainedLowLatency.
    void SetSustainedLowLatencyGc();

    // Debugger.IsAttached.
    [[nodiscard]] bool DebuggerAttached();
    // Debugger.Break(): a break into an attached debugger, nothing otherwise.
    void DebuggerBreak();
    // Debug.Assert(condition). Callers compile the call only where the C# code is
    // built with DEBUG defined, as [Conditional("DEBUG")] does.
    void DebugAssert(bool condition);

    // OperatingSystem.IsAndroid().
    [[nodiscard]] bool IsAndroid();
    // OperatingSystem.IsMacOS().
    [[nodiscard]] bool IsMacOS();
    // OperatingSystem.IsLinux(): false on Android, which is its own answer.
    [[nodiscard]] constexpr bool IsLinux() noexcept
    {
#if defined(__linux__) && !defined(__ANDROID__)
        return true;
#else
        return false;
#endif
    }
    // OperatingSystem.IsWindows().
    [[nodiscard]] constexpr bool IsWindows() noexcept
    {
#if defined(_WIN32)
        return true;
#else
        return false;
#endif
    }

    // Environment.GetFolderPath(Environment.SpecialFolder.UserProfile): the
    // home directory, or an empty string where there is none, as .NET returns.
    [[nodiscard]] std::string EnvironmentUserProfile();
    // Environment.CurrentDirectory.
    [[nodiscard]] std::string EnvironmentCurrentDirectory();
    // Environment.OSVersion.ToString().
    [[nodiscard]] std::string EnvironmentOSVersion();
    // Environment.Version. This build is not on .NET, so the text says so
    // rather than naming a runtime version it does not have.
    [[nodiscard]] std::string EnvironmentVersion();
    // Environment.Is64BitProcess.
    [[nodiscard]] bool EnvironmentIs64BitProcess() noexcept;
    // Environment.ProcessorCount.
    [[nodiscard]] std::int32_t EnvironmentProcessorCount();
    // Environment.ProcessId.
    [[nodiscard]] std::int32_t EnvironmentProcessId() noexcept;
    // Environment.CommandLine: the executable and its arguments, quoted the
    // way the platform gives them.
    [[nodiscard]] std::string EnvironmentCommandLine();

    // System.Runtime.InteropServices.RuntimeInformation. The architecture
    // names are the Architecture enum's own spellings, which is what the game
    // prints.
    [[nodiscard]] std::string RuntimeInformationProcessArchitecture();
    [[nodiscard]] std::string RuntimeInformationOSArchitecture();
    [[nodiscard]] std::string RuntimeInformationOSDescription();
    [[nodiscard]] std::string RuntimeInformationRuntimeIdentifier();
    // FrameworkDescription names the runtime the process is actually on. This
    // build is not on .NET, and says so where the C# prints its version -- the
    // same answer DebugLog's system line already gives.
    [[nodiscard]] std::string RuntimeInformationFrameworkDescription();

    // Environment.ProcessPath: the executable, resolved, or nullopt where the
    // platform will not say. WTF-8 on Windows (see Encoding.hpp).
    [[nodiscard]] std::optional<std::string> EnvironmentProcessPath();
    // PasteArguments.AppendArgument: one argument of a Windows command line
    // as Process builds it from ProcessStartInfo.ArgumentList. Left alone when
    // it is not empty and has no white space or quote in it; otherwise quoted,
    // with the backslashes before a quote -- or before the closing quote --
    // doubled.
    [[nodiscard]] std::wstring PasteArgument(std::wstring_view value);
    // AppContext.BaseDirectory: the directory the executable is in, ending in
    // a separator; the current directory where there is no executable path.
    [[nodiscard]] std::string AppContextBaseDirectory();

    // Environment.TickCount64: milliseconds since the system started.
    [[nodiscard]] std::int64_t EnvironmentTickCount64() noexcept;

    // Environment.ExitCode.
    [[nodiscard]] std::int32_t EnvironmentExitCode() noexcept;
    void SetEnvironmentExitCode(std::int32_t value) noexcept;

    // Integer division or remainder by zero.
    [[noreturn]] void ThrowDivideByZeroException();
    // List<T>[index] outside the list.
    [[noreturn]] void ThrowListIndexOutOfRange();
}
