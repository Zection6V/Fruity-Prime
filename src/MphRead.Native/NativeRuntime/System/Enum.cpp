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
}
