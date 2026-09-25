#include "ExceptionText.hpp"

#include "Console.hpp"

#include <cstdlib>
#include <memory>
#include <string_view>
#include <typeinfo>
#include <utility>

#if defined(__GNUG__)
#include <cxxabi.h>
#endif

namespace MphRead::NativeRuntime
{
    namespace
    {
        // Type.FullName: the demangled C++ name with its scopes joined by
        // dots, which for the System exceptions is .NET's own full name.
        [[nodiscard]] std::string ExceptionFullName(const std::exception& value)
        {
            const char* const raw = typeid(value).name();
            std::string name = raw == nullptr ? std::string("std::exception") : std::string(raw);
#if defined(__GNUG__)
            int status = 0;
            const std::unique_ptr<char, decltype(&std::free)> demangled(
                abi::__cxa_demangle(raw, nullptr, nullptr, &status), &std::free);
            if (status == 0 && demangled)
            {
                name = demangled.get();
            }
#endif
            for (const std::string_view prefix : {"class ", "struct "})
            {
                if (name.starts_with(prefix))
                {
                    name.erase(0, prefix.size());
                }
            }
            // Template arguments are no part of a managed type's name.
            if (const std::size_t angle = name.find('<'); angle != std::string::npos)
            {
                name.erase(angle);
            }
            // A standard library exception is the managed exception the same
            // failure raises in .NET.
            static constexpr std::pair<std::string_view, std::string_view> Standard[] = {
                {"std::bad_alloc", "System.OutOfMemoryException"},
                {"std::bad_array_new_length", "System.OutOfMemoryException"},
                {"std::bad_optional_access", "System.InvalidOperationException"},
                {"std::bad_any_cast", "System.InvalidCastException"},
                {"std::bad_cast", "System.InvalidCastException"},
                {"std::out_of_range", "System.ArgumentOutOfRangeException"},
                {"std::length_error", "System.ArgumentOutOfRangeException"},
                {"std::invalid_argument", "System.ArgumentException"},
                {"std::overflow_error", "System.OverflowException"},
                {"std::ios_base::failure", "System.IO.IOException"},
                {"std::filesystem::__cxx11::filesystem_error", "System.IO.IOException"},
                {"std::filesystem::filesystem_error", "System.IO.IOException"},
                {"std::system_error", "System.IO.IOException"},
            };
            for (const auto& [native, managed] : Standard)
            {
                if (name == native)
                {
                    return std::string(managed);
                }
            }
            if (name.starts_with("std::"))
            {
                return "System.Exception";
            }
            std::string result;
            result.reserve(name.size());
            for (std::size_t i = 0; i < name.size(); ++i)
            {
                if (name[i] == ':' && i + 1 < name.size() && name[i + 1] == ':')
                {
                    result.push_back('.');
                    ++i;
                }
                else
                {
                    result.push_back(name[i]);
                }
            }
            return result;
        }
    }

    std::string ExceptionTypeName(const std::exception& value)
    {
        // Type.Name: the full name without its namespace.
        const std::string full = ExceptionFullName(value);
        const std::size_t dot = full.find_last_of('.');
        return dot == std::string::npos ? full : full.substr(dot + 1);
    }

    std::string ExceptionMessage(const std::exception_ptr& value)
    {
        if (!value)
        {
            return std::string();
        }
        try
        {
            std::rethrow_exception(value);
        }
        catch (const std::exception& ex)
        {
            return ex.what();
        }
        catch (...)
        {
            return std::string();
        }
    }

    std::string ExceptionToString(const std::exception& value)
    {
        std::string result = ExceptionFullName(value) + ": " + value.what();
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
