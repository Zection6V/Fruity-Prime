#pragma once

#include <array>
#include <ostream>
#include <string>
#include <string_view>

namespace fruityprime::credits {

struct Entry {
    std::string_view who;
    std::string_view what;
    std::string_view where;
};

inline constexpr std::string_view Author = "Livetek";
inline constexpr std::string_view ForkWork =
    "this fork: multiplayer and the dedicated server, "
    "the launcher, custom maps, the Android head and the pro HUD";
inline constexpr std::string_view SupportUrl = "https://ko-fi.com/livetek";

[[nodiscard]] const std::array<Entry, 12>& entries() noexcept;
[[nodiscard]] std::string summary();
[[nodiscard]] std::string names();
[[nodiscard]] std::string compact();
void print(std::ostream& output);

} // namespace fruityprime::credits

namespace fruityprime::mods {

class Credits final {
public:
    using Entry = credits::Entry;
    inline static constexpr std::string_view Author = credits::Author;
    inline static constexpr std::string_view ForkWork = credits::ForkWork;
    inline static constexpr std::string_view SupportUrl = credits::SupportUrl;

    [[nodiscard]] static const std::array<Entry, 12>& Entries() noexcept;
    [[nodiscard]] static std::string Summary();
    [[nodiscard]] static std::string Names();
    [[nodiscard]] static std::string Compact();
    static void Print(std::ostream& output);
};

} // namespace fruityprime::mods

namespace MphReadNative::Mods {
using Credits = ::fruityprime::mods::Credits;
}
