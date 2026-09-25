#include "Enum.hpp"

#include "Encoding.hpp"
#include "Globalization.hpp"

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
        bool isSigned,
        std::int32_t bits,
        std::uint64_t& raw)
    {
        raw = 0;
        // Enum.TryParse: value.TrimStart(), and nothing at all is a failure.
        while (!text.empty())
        {
            const Utf8Scalar first = DecodeUtf8Scalar(text, 0);
            if (!CharIsWhiteSpace(first.Value))
            {
                break;
            }
            text.remove_prefix(first.Length);
        }
        if (text.empty())
        {
            return false;
        }

        // A digit or a sign in front is a number, if it parses as one; a
        // number out of range is a failure rather than a name.
        const char first = text.front();
        if ((first >= '0' && first <= '9') || first == '-' || first == '+')
        {
            std::int64_t value = 0;
            if (TryParseInteger(text, NumberStyles::AllowLeadingSign | NumberStyles::AllowTrailingWhite,
                    NumberFormatInfo::InvariantInfo(), isSigned, bits, value))
            {
                const std::uint64_t mask = bits >= 64 ? ~std::uint64_t(0) : (std::uint64_t(1) << bits) - 1;
                raw = static_cast<std::uint64_t>(value) & mask;
                return true;
            }
        }

        // Enum.TryParseByName: "A, B" is A | B.
        std::uint64_t result = 0;
        std::size_t start = 0;
        while (true)
        {
            const std::size_t comma = text.find(',', start);
            const std::string_view part = StringTrimView(text.substr(start,
                comma == std::string_view::npos ? std::string_view::npos : comma - start));
            bool found = false;
            for (std::size_t i = 0; i < count && !found; ++i)
            {
                const std::string_view name = names[i].Name;
                if (ignoreCase ? StringEqualsOrdinalIgnoreCase(name, part) : name == part)
                {
                    result |= names[i].Value;
                    found = true;
                }
            }
            if (!found)
            {
                return false;
            }
            if (comma == std::string_view::npos)
            {
                break;
            }
            start = comma + 1;
        }
        raw = result;
        return true;
    }
}
