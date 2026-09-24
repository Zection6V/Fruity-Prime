#pragma once

// Exception.GetType().Name and Exception.ToString(): what the game prints when
// it writes a failure down.

#include <exception>
#include <string>

namespace MphRead::NativeRuntime
{
    // exception.GetType().Name. The demangled C++ type name, which is the
    // nearest thing to the managed type's name.
    [[nodiscard]] std::string ExceptionTypeName(const std::exception& value);
    // exception.ToString(): "Type: Message", and the inner exception under it
    // where there is one. A managed exception's ToString also carries the
    // stack it was thrown from, which a C++ exception does not hold -- the
    // platform's own stack walk is in DebugLog instead.
    [[nodiscard]] std::string ExceptionToString(const std::exception& value);
    // The same for an exception held by pointer. An empty pointer gives the
    // text .NET produces for a non-Exception ExceptionObject.
    [[nodiscard]] std::string ExceptionToString(const std::exception_ptr& value);
}
