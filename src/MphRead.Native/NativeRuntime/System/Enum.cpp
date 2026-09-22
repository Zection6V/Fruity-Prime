#include "Enum.hpp"

#include <algorithm>
#include <vector>

namespace MphRead::NativeRuntime
{
    std::string ManagedEnumToString(
        std::uint64_t raw,
        std::int64_t signedValue,
        bool isSigned,
        bool isFlags,
        const EnumNameEntry* names,
        std::size_t count)
    {
        for (std::size_t i = 0; i < count; ++i)
        {
            if (names[i].Value == raw)
            {
                return names[i].Name;
            }
        }
        if (isFlags && raw != 0)
        {
            std::vector<const EnumNameEntry*> sorted;
            sorted.reserve(count);
            for (std::size_t i = 0; i < count; ++i)
            {
                sorted.push_back(&names[i]);
            }
            std::stable_sort(sorted.begin(), sorted.end(),
                [](const EnumNameEntry* left, const EnumNameEntry* right)
                {
                    return left->Value < right->Value;
                });
            std::uint64_t remaining = raw;
            std::vector<const EnumNameEntry*> found;
            for (std::size_t index = sorted.size(); index-- > 0;)
            {
                const std::uint64_t current = sorted[index]->Value;
                if (index == 0 && current == 0)
                {
                    break;
                }
                if ((remaining & current) == current)
                {
                    remaining -= current;
                    found.push_back(sorted[index]);
                }
            }
            if (remaining == 0)
            {
                std::string result;
                for (std::size_t i = found.size(); i-- > 0;)
                {
                    result += found[i]->Name;
                    if (i > 0)
                    {
                        result += ", ";
                    }
                }
                return result;
            }
        }
        if (isSigned)
        {
            return std::to_string(signedValue);
        }
        return std::to_string(raw);
    }

    bool ManagedEnumTryParse(
        std::string_view text,
        bool ignoreCase,
        const EnumNameEntry* names,
        std::size_t count,
        std::uint64_t& raw)
    {
        // Enum.TryParse rejects null and empty outright, and trims nothing but
        // leading white space.
        std::size_t start = 0;
        while (start < text.size()
            && (text[start] == ' ' || text[start] == '\t' || text[start] == '\n'
                || text[start] == '\r'))
        {
            ++start;
        }
        const std::string_view value = text.substr(start);
        if (value.empty())
        {
            raw = 0;
            return false;
        }

        // A leading digit or sign means the number path; otherwise a name.
        const char first = value.front();
        if (!((first >= '0' && first <= '9') || first == '-' || first == '+'))
        {
            for (std::size_t i = 0; i < count; ++i)
            {
                const std::string_view name = names[i].Name;
                if (name.size() != value.size())
                {
                    continue;
                }
                bool equal = true;
                for (std::size_t c = 0; c < name.size(); ++c)
                {
                    char left = name[c];
                    char right = value[c];
                    if (ignoreCase)
                    {
                        if (left >= 'a' && left <= 'z')
                        {
                            left = static_cast<char>(left - ('a' - 'A'));
                        }
                        if (right >= 'a' && right <= 'z')
                        {
                            right = static_cast<char>(right - ('a' - 'A'));
                        }
                    }
                    if (left != right)
                    {
                        equal = false;
                        break;
                    }
                }
                if (equal)
                {
                    raw = names[i].Value;
                    return true;
                }
            }
            raw = 0;
            return false;
        }

        const bool negative = first == '-';
        std::size_t index = (first == '-' || first == '+') ? 1U : 0U;
        if (index >= value.size())
        {
            raw = 0;
            return false;
        }
        std::uint64_t magnitude = 0;
        for (; index < value.size(); ++index)
        {
            const char digit = value[index];
            if (digit < '0' || digit > '9')
            {
                raw = 0;
                return false;
            }
            magnitude = magnitude * 10U + static_cast<std::uint64_t>(digit - '0');
        }
        raw = negative ? ~magnitude + 1U : magnitude;
        return true;
    }

}
