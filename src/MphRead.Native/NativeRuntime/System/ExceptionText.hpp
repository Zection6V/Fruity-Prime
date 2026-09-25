#pragma once

// Exception.GetType().Name and Exception.ToString(): what the game prints when
// it writes a failure down.

#include <exception>
#include <string>

namespace MphRead::NativeRuntime
{
    // exception.GetType().Name: the type's name without its namespace, so a
    // System::NullReferenceException is "NullReferenceException".
    [[nodiscard]] std::string ExceptionTypeName(const std::exception& value);
    // exception.Message for an exception held by pointer: what() for a C++
    // exception, empty for anything else.
    [[nodiscard]] std::string ExceptionMessage(const std::exception_ptr& value);
    // exception.ToString(): "Namespace.Type: Message", and the inner exception under it
    // where there is one. A managed exception's ToString also carries the
    // stack it was thrown from, which a C++ exception does not hold -- the
    // platform's own stack walk is in DebugLog instead.
    [[nodiscard]] std::string ExceptionToString(const std::exception& value);
    // The same for an exception held by pointer. An empty pointer gives the
    // text .NET produces for a non-Exception ExceptionObject.
    [[nodiscard]] std::string ExceptionToString(const std::exception_ptr& value);
}
