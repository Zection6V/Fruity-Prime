#pragma once

// The frontend archive header.
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



struct FrontendHeader;

// Frontend.cs
struct FrontendHeader {
    std::vector<char> Type;
    std::uint16_t Field4{};
    std::uint8_t Field6{};
    std::uint8_t Field7{};
    std::uint32_t Offfset1{};
    std::uint32_t Offfset2{};
    std::uint32_t Offfset3{};
};

} // namespace fruityprime::formats
