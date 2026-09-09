#pragma once

#include "Mods/MapGen/mapgen.hpp"

namespace fruityprime::mapgen::detail {

[[nodiscard]] Q3ConvertResult convert_q3_recipe(
    const Q3ConvertOptions& options);

} // namespace fruityprime::mapgen::detail
