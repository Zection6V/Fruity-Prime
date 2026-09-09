#pragma once

#include <array>
#include <cstddef>
#include <cstdint>
#include <string_view>

namespace fruityprime::chat::font {

inline constexpr std::int32_t Cell = 8;
inline constexpr char16_t First = u' ';
inline constexpr char16_t Last = u'~';

using PixelArray = std::array<
    std::uint8_t,
    static_cast<std::size_t>(Last - First + 1) * Cell * Cell>;
using WidthArray = std::array<
    std::int32_t,
    static_cast<std::size_t>(Last - First + 1)>;

PixelArray& pixels() noexcept;
WidthArray& widths() noexcept;
std::int32_t index(char16_t ch) noexcept;
std::int32_t index(char ch) noexcept;
std::int32_t measure(std::u16string_view text) noexcept;
std::int32_t measure(std::string_view text) noexcept;

} // namespace fruityprime::chat::font
