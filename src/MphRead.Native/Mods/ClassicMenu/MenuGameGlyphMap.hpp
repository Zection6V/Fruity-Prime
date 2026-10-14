#pragma once

#include <cstdint>

namespace MphRead::Mods::ClassicMenu
{
    // Reverse the game's packed 8x8 font code mapping for UTF-8 menu text.
    // -1 means the original DS font has no mapping for this Unicode scalar.
    // When game-code aliases exist, the lowest original code is preferred.
    [[nodiscard]] std::int32_t FindGameGlyphCode(char32_t unicode);
}
