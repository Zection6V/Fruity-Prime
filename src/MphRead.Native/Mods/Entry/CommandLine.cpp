#include "CommandLine.hpp"

#include "../../NativeRuntime/System/Globalization.hpp"

#include <cstddef>

namespace MphRead::Mods::Entry
{
    std::string_view TrimStartHyphen(std::string_view value) noexcept
    {
        while (!value.empty() && value.front() == '-')
        {
            value.remove_prefix(1);
        }
        return value;
    }

    bool IsFlag(std::string_view argument, std::string_view name) noexcept
    {
        return ::MphRead::NativeRuntime::StringEqualsOrdinalIgnoreCase(TrimStartHyphen(argument), name);
    }

    bool HasFlag(const std::vector<std::string>& args, std::string_view name) noexcept
    {
        for (const std::string& argument : args)
        {
            if (IsFlag(argument, name))
            {
                return true;
            }
        }
        return false;
    }

    int IndexOfFlag(const std::vector<std::string>& args, std::string_view name) noexcept
    {
        for (std::size_t i = 0; i < args.size(); ++i)
        {
            if (IsFlag(args[i], name))
            {
                return static_cast<int>(i);
            }
        }
        return -1;
    }

    std::optional<std::string> ValueAfter(
        const std::vector<std::string>& args, std::string_view name)
    {
        if (args.size() < 2)
        {
            return std::nullopt;
        }
        for (std::size_t i = 0; i + 1 < args.size(); ++i)
        {
            if (IsFlag(args[i], name))
            {
                return args[i + 1];
            }
        }
        return std::nullopt;
    }

    std::vector<std::string> ValuesAfter(
        const std::vector<std::string>& args, std::string_view name)
    {
        std::vector<std::string> result;
        if (args.size() < 2)
        {
            return result;
        }
        for (std::size_t i = 0; i + 1 < args.size(); ++i)
        {
            if (IsFlag(args[i], name))
            {
                result.push_back(args[i + 1]);
            }
        }
        return result;
    }

    bool StartsWithHyphen(const std::optional<std::string>& value) noexcept
    {
        return value.has_value() && !value->empty() && value->front() == '-';
    }
}
