#pragma once

#include "Mods/Network/demo.hpp"

#include <array>
#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <iosfwd>
#include <optional>
#include <vector>

namespace fruityprime::demo {

// The file inspection half of Mods/Network/DemoInfo.cs.  It is intentionally
// independent of a room/session so `-demoinfo` can diagnose a recording even
// when the ROM is not available.
struct Info final {
    std::uint8_t protocol = 0;
    std::size_t records = 0;
    std::size_t payload_bytes = 0;
    std::array<std::size_t, 256> packet_counts{};
    std::array<std::size_t, 256> packet_bytes{};
    std::uint32_t first_frame = 0;
    std::uint32_t last_frame = 0;
    std::uint32_t biggest_gap = 0;
    std::size_t snapshots = 0;
    // .NET Dictionary enumerates entries in insertion order.  Keeping the
    // first-seen packet types lets the native diagnostic print the same order
    // instead of the byte-value order of packet_counts.
    std::vector<std::uint8_t> packet_order;
    std::size_t inflated_bytes = 0;
    std::uintmax_t on_disk = 0;
    bool have_frame = false;
    bool tail_truncated = false;
};

[[nodiscard]] std::optional<Info> inspect(const std::filesystem::path& path);
void print(std::ostream& output, const std::filesystem::path& path,
           const Info& info);

// The command boundary owned by Mods/Network/DemoInfo.cs for the inspection
// (non-replay) path.  Replay remains a separate Scene/DemoPlayback port until
// that call path is 1:1 rather than a headless substitute.
[[nodiscard]] int print_command(const std::filesystem::path& path);

} // namespace fruityprime::demo
