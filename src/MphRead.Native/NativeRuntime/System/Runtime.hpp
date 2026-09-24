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
    // Debugger.Break().
    void DebuggerBreak();
    // Debug.Assert(condition). Callers compile the call only where the C# code is
    // built with DEBUG defined, as [Conditional("DEBUG")] does.
    void DebugAssert(bool condition);

    // OperatingSystem.IsAndroid().
    [[nodiscard]] bool IsAndroid();

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
