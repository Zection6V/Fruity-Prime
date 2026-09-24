#include "ExceptionText.hpp"

#include "Console.hpp"

#include <cstdlib>
#include <memory>
#include <typeinfo>

#if defined(__GNUG__)
#include <cxxabi.h>
#endif

namespace MphRead::NativeRuntime
{
    std::string ExceptionTypeName(const std::exception& value)
    {
        const char* const raw = typeid(value).name();
#if defined(__GNUG__)
        int status = 0;
        const std::unique_ptr<char, decltype(&std::free)> demangled(
            abi::__cxa_demangle(raw, nullptr, nullptr, &status), &std::free);
        if (status == 0 && demangled)
        {
            return demangled.get();
        }
#endif
        return raw == nullptr ? "std::exception" : std::string(raw);
    }

    std::string ExceptionToString(const std::exception& value)
    {
        std::string result = ExceptionTypeName(value) + ": " + value.what();
        const auto* const nested = dynamic_cast<const std::nested_exception*>(&value);
        if (nested != nullptr && nested->nested_ptr())
        {
            result.append(EnvironmentNewLine());
            result.append(" ---> ");
            result.append(ExceptionToString(nested->nested_ptr()));
        }
        return result;
    }

    std::string ExceptionToString(const std::exception_ptr& value)
    {
        if (!value)
        {
            // AppDomain hands ExceptionObject over as a plain object when the
            // failure was not an exception; .NET's own text is its ToString.
            return "System.Object";
        }
        try
        {
            std::rethrow_exception(value);
        }
        catch (const std::exception& ex)
        {
            return ExceptionToString(ex);
        }
        catch (...)
        {
            return "System.Object";
        }
    }
}
