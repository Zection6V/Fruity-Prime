#pragma once

// The AI personality tree's nested data records.
// Transliterated from the managed sources so the field names and order stay
// checkable against them.  A C# reference member becomes a pointer.

#include "Formats/Types.hpp"
#include "Formats/raw_formats.hpp"
#include "Formats/enum_tables.hpp"


#include <array>
#include <cstdint>
#include <string>
#include <string_view>
#include <vector>

namespace fruityprime::formats {



struct AiPersonalityData1;
struct AiPersonalityData5;
struct AiPersonalityData2;
struct AiPersonalityData4;

// AiPersonality.cs
struct AiPersonalityData1 {
    std::string_view Label;
    std::int32_t Func24Id{};
    std::vector<AiPersonalityData1> Data1;
    std::vector<AiPersonalityData2> Data2;
    std::vector<std::int32_t> Data3a;
    std::vector<std::int32_t> Data3b;
    AiPersonalityData1* Parent{};
};

// AiPersonality.cs
struct AiPersonalityData5 {
    std::int32_t Param1{};
    std::int32_t Param2{};
    bool IsEmpty{};
};

// AiPersonality.cs
struct AiPersonalityData2 {
    std::int32_t Data1SelectIndex{};
    std::int32_t Weight{};
    std::vector<AiPersonalityData4> Data4;
    std::int32_t Func3Id{};
    AiPersonalityData5 Parameters{};
};

// AiPersonality.cs
struct AiPersonalityData4 {
    std::int32_t Func3Id{};
    AiPersonalityData5 Parameters{};
};

} // namespace fruityprime::formats
