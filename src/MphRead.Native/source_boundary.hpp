#pragma once

#include <cstddef>
#include <string_view>

namespace fruityprime::port {

enum class SourceStatus {
    BoundaryOnly,
};

struct SourceUnit final {
    std::string_view managed_path;
    std::string_view native_path;
    SourceStatus status;
};

template <std::size_t N>
[[nodiscard]] consteval SourceUnit make_source_unit(
    const char (&managed_path)[N], std::string_view native_path) noexcept {
    return {std::string_view(managed_path, N - 1), native_path,
            SourceStatus::BoundaryOnly};
}

#define FRUITY_PRIME_DECLARE_SOURCE_UNIT(path) \
    namespace { \
    [[maybe_unused]] constexpr auto kManagedSourceUnit = \
        ::fruityprime::port::make_source_unit(path, __FILE__); \
    }

} // namespace fruityprime::port
