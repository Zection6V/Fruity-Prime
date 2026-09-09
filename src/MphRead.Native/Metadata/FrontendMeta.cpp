#include "Metadata/movie_files.hpp"
#include "metadata_tables.hpp"

namespace fruityprime::metadata {

const std::array<EffectInfo, EffectCount>& effects() noexcept {
    return detail::EffectTable;
}

const EffectInfo* effect_info(std::uint16_t id) noexcept {
    return id < EffectCount ? &detail::EffectTable[id] : nullptr;
}


} // namespace fruityprime::metadata

