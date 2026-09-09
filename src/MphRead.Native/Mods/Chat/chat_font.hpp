#pragma once

#include <array>
#include <cstddef>
#include <cstdint>
#include <string_view>

namespace fruityprime::chat::font {

inline constexpr int Cell = 8;
inline constexpr char First = ' ';
inline constexpr char Last = '~';
inline constexpr std::size_t Count =
    static_cast<std::size_t>(Last - First + 1);

const std::array<std::uint8_t, Count * Cell * Cell>& pixels() noexcept;
const std::array<int, Count>& widths() noexcept;
int index(char ch) noexcept;
int measure(std::string_view text) noexcept;

} // namespace fruityprime::chat::font
