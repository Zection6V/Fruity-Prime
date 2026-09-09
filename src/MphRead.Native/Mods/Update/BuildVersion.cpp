#include "Mods/Update/build_version.hpp"

#include <algorithm>
#include <charconv>
#include <string>

namespace fruityprime::update {
namespace {

[[nodiscard]] std::string trim_copy(std::string value) {
    const std::size_t first = value.find_first_not_of(" \t\r\n");
    if (first == std::string::npos) {
        return {};
    }
    const std::size_t last = value.find_last_not_of(" \t\r\n");
    return value.substr(first, last - first + 1);
}

} // namespace

bool operator<(const Version& left, const Version& right) noexcept {
    if (left.major != right.major) return left.major < right.major;
    if (left.minor != right.minor) return left.minor < right.minor;
    return left.patch < right.patch;
}

std::optional<Version> BuildVersion::current() noexcept {
#ifdef FRUITY_PRIME_RELEASE_VERSION
    return parse(FRUITY_PRIME_RELEASE_VERSION);
#else
    return std::nullopt;
#endif
}

std::optional<Version> BuildVersion::parse(std::string_view input) noexcept {
    std::string text = trim_copy(std::string(input));
    if (text.empty() || text.find('-') != std::string::npos) {
        return std::nullopt;
    }
    if (text.size() > 1 && (text.front() == 'v' || text.front() == 'V')) {
        text.erase(text.begin());
    }
    Version result;
    int* components[] = {&result.major, &result.minor, &result.patch};
    std::size_t begin = 0;
    for (std::size_t index = 0; index < 3; ++index) {
        const std::size_t end = text.find('.', begin);
        const std::string_view part(text.data() + begin,
            end == std::string::npos ? text.size() - begin : end - begin);
        if (part.empty()) {
            return std::nullopt;
        }
        const auto parsed = std::from_chars(part.data(),
                                            part.data() + part.size(),
                                            *components[index]);
        if (parsed.ec != std::errc{}
            || parsed.ptr != part.data() + part.size()
            || *components[index] < 0) {
            return std::nullopt;
        }
        if (end == std::string::npos) {
            if (index == 0) {
                result.minor = 0;
                result.patch = 0;
            } else if (index == 1) {
                result.patch = 0;
            }
            break;
        }
        begin = end + 1;
        if (index == 2) {
            return std::nullopt;
        }
    }
    // The SDK's unstamped default is 1.0.0. Treat it as a local build, as the
    // managed updater does, so a developer build cannot replace itself.
    if (result.major == 1 && result.minor == 0 && result.patch == 0) {
        return std::nullopt;
    }
    return normalize(result);
}

Version BuildVersion::normalize(Version version) noexcept {
    return Version{version.major, version.minor, std::max(version.patch, 0)};
}

std::string BuildVersion::display(const std::optional<Version>& version) {
    if (!version) {
        return "a local build";
    }
    return "v" + std::to_string(version->major) + "."
        + std::to_string(version->minor) + "."
        + std::to_string(version->patch);
}

} // namespace fruityprime::update
