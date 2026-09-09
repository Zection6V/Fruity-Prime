#include "MetadataValues.hpp"

namespace fruityprime::metadata {
namespace {

constexpr formats::Vector3 get_color(std::uint16_t value) noexcept {
    const float red = static_cast<float>((value >> 0) & 0x1f) / 31.0F;
    const float green = static_cast<float>((value >> 5) & 0x1f) / 31.0F;
    const float blue = static_cast<float>((value >> 10) & 0x1f) / 31.0F;
    return {red, green, blue};
}

} // namespace

const std::array<formats::Vector3, 31> ToonTable{{
    get_color(0x2000), get_color(0x2000), get_color(0x2020),
    get_color(0x2021), get_color(0x2021), get_color(0x2041),
    get_color(0x2441), get_color(0x2461), get_color(0x2461),
    get_color(0x2462), get_color(0x2482), get_color(0x2482),
    get_color(0x28C3), get_color(0x2CE4), get_color(0x3105),
    get_color(0x3546), get_color(0x3967), get_color(0x3D88),
    get_color(0x41C9), get_color(0x45EA), get_color(0x4A0B),
    get_color(0x4E4B), get_color(0x526C), get_color(0x568D),
    get_color(0x5ACE), get_color(0x5EEF), get_color(0x6310),
    get_color(0x6751), get_color(0x6B72), get_color(0x6F93),
    get_color(0x73D4),
}};

const std::array<PowerPaletteEntry, 4> PowerPalettes{{
    {"Alimbic_Power", {{{32576}, {32576}, {32608}, {32640},
                         {32711}, {32719}, {32758}, {32733}}}},
    {"Generic_Power", {{{19393}, {18369}, {17345}, {16321},
                         {19400}, {23535}, {26614}, {31741}}}},
    {"Ice_Power", {{{29453}, {29453}, {29485}, {29517},
                     {30578}, {30614}, {31705}, {32734}}}},
    {"Lava_Power", {{{671}, {639}, {607}, {575},
                      {7807}, {16127}, {23391}, {30719}}}},
}};

const std::array<float, HunterCount> HunterScales{{
    1.0F,
    static_cast<float>(0x10F5) / 4096.0F,
    1.0F,
    1.0F,
    1.0F,
    static_cast<float>(0x123D) / 4096.0F,
    1.0F,
    1.0F,
}};

const std::array<std::array<std::string_view, 4>, HunterCount> HunterModels{{
    {{"Samus_lod0", "Samus_lod1", "SamusAlt_lod0", "SamusGun"}},
    {{"Kanden_lod0", "Kanden_lod1", "KandenAlt_lod0", "KandenGun"}},
    {{"Trace_lod0", "Trace_lod1", "TraceAlt_lod0", "TraceGun"}},
    {{"Sylux_lod0", "Sylux_lod1", "SyluxAlt_lod0", "SyluxGun"}},
    {{"Nox_lod0", "Nox_lod1", "NoxAlt_lod0", "NoxGun"}},
    {{"Spire_lod0", "Spire_lod1", "SpireAlt_lod0", "SpireGun"}},
    {{"Weavel_lod0", "Weavel_lod1", "WeavelAlt_lod0", "WeavelGun"}},
    {{"Guardian_lod0", "Guardian_lod1", "SamusAlt_lod0", "SamusGun"}},
}};

const std::array<int, 16> ImaIndexTable{{
    -1, -1, -1, -1, 2, 4, 6, 8,
    -1, -1, -1, -1, 2, 4, 6, 8,
}};

const PowerPaletteEntry* power_palette(std::string_view name) noexcept {
    for (const auto& entry : PowerPalettes) {
        if (entry.Name == name) {
            return &entry;
        }
    }
    return nullptr;
}

} // namespace fruityprime::metadata
