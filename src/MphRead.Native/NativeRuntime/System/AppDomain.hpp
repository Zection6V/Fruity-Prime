#pragma once

// AppDomain.CurrentDomain.UnhandledException: the one member the game
// subscribes to, and the only thing it uses it for -- writing down a failure
// that would otherwise take the process with it silently.

#include <exception>
#include <functional>

namespace MphRead::NativeRuntime
{
    // The handler is given the exception that escaped. It is null where the
    // fault was not a C++ exception at all (a hardware fault), which has no
    // counterpart in .NET: there the same fault is a managed exception.
    using UnhandledExceptionHandler = std::function<void(std::exception_ptr)>;

    // AppDomain.CurrentDomain.UnhandledException += handler. Handlers run in
    // the order they were added, as .NET runs them, and a handler that throws
    // does not stop the ones after it.
    void AppDomainAddUnhandledExceptionHandler(UnhandledExceptionHandler handler);

    // AppDomain.CurrentDomain.ProcessExit += handler: run once, in the order
    // added, as the process exits normally.
    void AppDomainAddProcessExitHandler(std::function<void()> handler);
}
