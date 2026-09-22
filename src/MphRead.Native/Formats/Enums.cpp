#include "Enums.hpp"

#include "../NativeRuntime/System/Enum.hpp"

namespace MphRead
{
    namespace
    {
        // Enums.cs Hunter : byte
        constexpr ::MphRead::NativeRuntime::EnumNameEntry HunterNames[] = {
            {0x0ULL, "Samus"},
            {0x1ULL, "Kanden"},
            {0x2ULL, "Trace"},
            {0x3ULL, "Sylux"},
            {0x4ULL, "Noxus"},
            {0x5ULL, "Spire"},
            {0x6ULL, "Weavel"},
            {0x7ULL, "Guardian"},
            {0x8ULL, "Random"},
        };
    }

    std::string ToString(Hunter value)
    {
        return ::MphRead::NativeRuntime::ManagedEnumToString(
            value, HunterNames, std::size(HunterNames), false);
    }
}
