#pragma once

#include <optional>
#include <string>
#include <string_view>

namespace fruityprime::update {

struct Version {
    int major = 0;
    int minor = 0;
    int patch = 0;

    friend bool operator==(const Version&, const Version&) = default;
    friend bool operator<(const Version& left, const Version& right) noexcept;
};

class BuildVersion {
public:
    [[nodiscard]] static std::optional<Version> current() noexcept;
    [[nodiscard]] static std::optional<Version> parse(
        std::string_view text) noexcept;
    [[nodiscard]] static Version normalize(Version version) noexcept;
    [[nodiscard]] static std::string display(
        const std::optional<Version>& version);
};

} // namespace fruityprime::update
