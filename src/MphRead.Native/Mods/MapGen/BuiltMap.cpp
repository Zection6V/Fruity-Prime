#include "Mods/MapGen/mapgen.hpp"

#include <utility>

namespace fruityprime::mapgen {

BuiltMap::BuiltMap(MapDefinition value)
    : definition(std::move(value)) {}

} // namespace fruityprime::mapgen
