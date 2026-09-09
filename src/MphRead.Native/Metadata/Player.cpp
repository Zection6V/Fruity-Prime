#include "Metadata/player_metadata.hpp"
#include "metadata_tables.hpp"

#include <algorithm>
#include <limits>

namespace fruityprime::metadata {

const std::array<HunterInfo, HunterCount>& hunters() noexcept {
    return detail::HunterTable;
}

const HunterInfo* find_hunter(std::string_view name) noexcept {
    for (const auto& hunter : detail::HunterTable) {
        if (detail::equal_ascii_insensitive(hunter.name, name)) {
            return &hunter;
        }
    }
    return nullptr;
}

const HunterInfo& hunter_info(std::uint8_t id) noexcept {
    const std::size_t index = std::min<std::size_t>(id, HunterCount - 1);
    return detail::HunterTable[index];
}

std::optional<std::uint8_t> parse_hunter(std::string_view value) noexcept {
    if (value.empty()) {
        return std::nullopt;
    }
    // Random is a launcher choice, not a model row, so it intentionally does
    // not live in detail::HunterTable/HunterCount.
    if (detail::equal_ascii_insensitive(value, "Random")) {
        return static_cast<std::uint8_t>(Hunter::Random);
    }
    if (const auto* found = find_hunter(value); found != nullptr) {
        return static_cast<std::uint8_t>(found->id);
    }
    std::uint32_t number = 0;
    for (const char character : value) {
        if (character < '0' || character > '9') {
            return std::nullopt;
        }
        const std::uint32_t digit = static_cast<std::uint32_t>(character - '0');
        if (number > (std::numeric_limits<std::uint8_t>::max() - digit) / 10) {
            return std::nullopt;
        }
        number = number * 10 + digit;
    }
    return number <= std::numeric_limits<std::uint8_t>::max()
        ? std::optional<std::uint8_t>(static_cast<std::uint8_t>(number))
        : std::nullopt;
}

std::uint8_t roll_hunter(std::uint32_t seed) noexcept {
    // Deterministic equivalent of the launcher's random choice. The native
    // launcher can persist the requested Random value without making replay
    // or tests depend on the process's global RNG state.
    seed ^= seed >> 16;
    seed *= 0x7feb352dU;
    seed ^= seed >> 15;
    seed *= 0x846ca68bU;
    seed ^= seed >> 16;
    return static_cast<std::uint8_t>(seed % PlayableHunterCount);
}


} // namespace fruityprime::metadata

