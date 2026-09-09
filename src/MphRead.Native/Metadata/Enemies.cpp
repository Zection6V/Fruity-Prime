#include "Metadata/enemy_subroutines.hpp"
#include "Metadata/enemy_values.hpp"
#include "Metadata/metadata_extra.hpp"
#include "metadata_tables.hpp"

namespace fruityprime::metadata {

const std::array<EnemyInfo, EnemyCount>& enemies() noexcept {
    return detail::EnemyTable;
}

const EnemyInfo& enemy_info(std::uint8_t id) noexcept {
    const std::size_t index = std::min<std::size_t>(id, EnemyCount - 1);
    return detail::EnemyTable[index];
}

std::array<Effectiveness, 9> decode_effectiveness(
    std::uint32_t packed) noexcept {
    std::array<Effectiveness, 9> result{};
    for (std::size_t i = 0; i < result.size(); ++i) {
        result[i] = static_cast<Effectiveness>((packed >> (i * 2)) & 3U);
    }
    return result;
}

float damage_multiplier(Effectiveness value) noexcept {
    switch (value) {
    case Effectiveness::Zero:
        return 0.0F;
    case Effectiveness::Half:
        return 0.5F;
    case Effectiveness::Normal:
        return 1.0F;
    case Effectiveness::Double:
        return 2.0F;
    }
    return 0.0F;
}

} // namespace fruityprime::metadata
