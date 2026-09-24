#pragma once

// System.GC, System.Runtime.GCSettings, System.Diagnostics.Debugger/Debug,
// System.Environment and System.OperatingSystem: the members the game calls.

#include <cstdint>
#include <string>

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

    // Environment.GetFolderPath(Environment.SpecialFolder.UserProfile): the
    // home directory, or an empty string where there is none, as .NET returns.
    [[nodiscard]] std::string EnvironmentUserProfile();
    // Environment.CurrentDirectory.
    [[nodiscard]] std::string EnvironmentCurrentDirectory();

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

    // AppContext.BaseDirectory: the directory the executable is in, UTF-8,
    // ending in a separator.
    [[nodiscard]] std::string AppContextBaseDirectory();

    // Environment.ExitCode.
    [[nodiscard]] std::int32_t EnvironmentExitCode() noexcept;
    void SetEnvironmentExitCode(std::int32_t value) noexcept;

    // Integer division or remainder by zero.
    [[noreturn]] void ThrowDivideByZeroException();
    // List<T>[index] outside the list.
    [[noreturn]] void ThrowListIndexOutOfRange();
}
