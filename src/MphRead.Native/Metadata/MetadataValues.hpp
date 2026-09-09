#pragma once

// The remaining standalone values from Metadata.cs.  The large tables that
// already have native consumers remain in Metadata/metadata_values.hpp;
// these records keep the managed names and shapes available to the native
// metadata facade as well.

#include "Metadata/metadata.hpp"
#include "Formats/Types.hpp"

#include <array>
#include <cstdint>
#include <string_view>

namespace fruityprime::metadata {

struct PaletteData {
    std::uint16_t Data = 0;

    constexpr PaletteData(std::uint16_t data = 0) : Data(data) {}
};

struct PowerPaletteEntry {
    std::string_view Name;
    std::array<PaletteData, 8> Values{};
};

extern const std::array<formats::Vector3, 31> ToonTable;
extern const std::array<PowerPaletteEntry, 4> PowerPalettes;
extern const std::array<float, HunterCount> HunterScales;
extern const std::array<std::array<std::string_view, 4>, HunterCount>
    HunterModels;
extern const std::array<int, 16> ImaIndexTable;

[[nodiscard]] const PowerPaletteEntry* power_palette(
    std::string_view name) noexcept;

} // namespace fruityprime::metadata
