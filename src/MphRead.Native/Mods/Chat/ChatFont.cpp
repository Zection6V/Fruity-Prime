#include "Mods/Chat/chat_font.hpp"

#include <algorithm>
#include <initializer_list>
#include <string_view>

namespace fruityprime::chat::font {
namespace {

struct FontData {
    std::array<std::uint8_t, Count * Cell * Cell> pixels{};
    std::array<int, Count> widths{};

    void define(char ch, int advance) noexcept {
        widths[static_cast<std::size_t>(ch - First)] = advance;
    }

    void define(char ch, int top,
                std::initializer_list<std::string_view> rows) noexcept {
        const std::size_t glyph =
            static_cast<std::size_t>(ch - First) * Cell * Cell;
        int width = 0;
        int row_index = 0;
        for (const std::string_view row : rows) {
            for (std::size_t column = 0;
                 column < row.size() && column < Cell; ++column) {
                if (row[column] == '#') {
                    const int pixel_row = top + row_index;
                    if (pixel_row >= 0 && pixel_row < Cell) {
                        pixels[glyph + static_cast<std::size_t>(pixel_row) *
                                            Cell + column] = 1;
                    }
                    width = std::max(width, static_cast<int>(column + 1));
                }
            }
            ++row_index;
        }
        widths[static_cast<std::size_t>(ch - First)] = width + 1;
    }
};

FontData make_font() {
    FontData font{};
    font.define(' ', 3);
    font.define('!', 0, {"#", "#", "#", "#", ".", "#"});
    font.define('"', 0, {"#.#", "#.#"});
    font.define('#', 1, {".#.#.", "#####", ".#.#.", "#####", ".#.#."});
    font.define('$', 0, {"..#..", ".####", "##...", ".###.", "...##", "####.", "..#.."});
    font.define('%', 1, {"##..#", "##.#.", "..#..", ".#.##", "#..##"});
    font.define('&', 1, {".##..", "#..#.", ".##..", "#..##", ".##.#"});
    font.define('\'', 0, {"#", "#"});
    font.define('(', 0, {".#", "#.", "#.", "#.", "#.", ".#"});
    font.define(')', 0, {"#.", ".#", ".#", ".#", ".#", "#."});
    font.define('*', 1, {"#.#", ".#.", "#.#"});
    font.define('+', 2, {".#.", "###", ".#."});
    font.define(',', 5, {".#", "#."});
    font.define('-', 3, {"###"});
    font.define('.', 5, {"#"});
    font.define('/', 1, {"...#", "..#.", "..#.", ".#..", "#..."});
    font.define('0', 0, {".##.", "#..#", "#.##", "##.#", "#..#", ".##."});
    font.define('1', 0, {".#.", "##.", ".#.", ".#.", ".#.", "###"});
    font.define('2', 0, {".##.", "#..#", "...#", "..#.", ".#..", "####"});
    font.define('3', 0, {"###.", "...#", ".##.", "...#", "#..#", ".##."});
    font.define('4', 0, {"..#.", ".##.", "#.#.", "####", "..#.", "..#."});
    font.define('5', 0, {"####", "#...", "###.", "...#", "#..#", ".##."});
    font.define('6', 0, {".##.", "#...", "###.", "#..#", "#..#", ".##."});
    font.define('7', 0, {"####", "...#", "..#.", "..#.", ".#..", ".#.."});
    font.define('8', 0, {".##.", "#..#", ".##.", "#..#", "#..#", ".##."});
    font.define('9', 0, {".##.", "#..#", "#..#", ".###", "...#", ".##."});
    font.define(':', 2, {"#", ".", ".", "#"});
    font.define(';', 2, {".#", "..", "..", ".#", "#."});
    font.define('<', 1, {"..#", ".#.", "#..", ".#.", "..#"});
    font.define('=', 2, {"####", "....", "####"});
    font.define('>', 1, {"#..", ".#.", "..#", ".#.", "#.."});
    font.define('?', 0, {".##.", "#..#", "...#", "..#.", "....", "..#."});
    font.define('@', 0, {".###.", "#...#", "#.###", "#.#.#", "#....", ".###."});
    font.define('A', 0, {".###.", "#...#", "#...#", "#####", "#...#", "#...#"});
    font.define('B', 0, {"####.", "#...#", "####.", "#...#", "#...#", "####."});
    font.define('C', 0, {".###.", "#...#", "#....", "#....", "#...#", ".###."});
    font.define('D', 0, {"####.", "#...#", "#...#", "#...#", "#...#", "####."});
    font.define('E', 0, {"#####", "#....", "####.", "#....", "#....", "#####"});
    font.define('F', 0, {"#####", "#....", "####.", "#....", "#....", "#...."});
    font.define('G', 0, {".###.", "#...#", "#....", "#..##", "#...#", ".###."});
    font.define('H', 0, {"#...#", "#...#", "#####", "#...#", "#...#", "#...#"});
    font.define('I', 0, {"###", ".#.", ".#.", ".#.", ".#.", "###"});
    font.define('J', 0, {"...##", "....#", "....#", "....#", "#...#", ".###."});
    font.define('K', 0, {"#...#", "#..#.", "##...", "##...", "#..#.", "#...#"});
    font.define('L', 0, {"#....", "#....", "#....", "#....", "#....", "#####"});
    font.define('M', 0, {"#...#", "##.##", "#.#.#", "#...#", "#...#", "#...#"});
    font.define('N', 0, {"#...#", "##..#", "#.#.#", "#..##", "#...#", "#...#"});
    font.define('O', 0, {".###.", "#...#", "#...#", "#...#", "#...#", ".###."});
    font.define('P', 0, {"####.", "#...#", "#...#", "####.", "#....", "#...."});
    font.define('Q', 0, {".###.", "#...#", "#...#", "#...#", "#..#.", ".##.#"});
    font.define('R', 0, {"####.", "#...#", "#...#", "####.", "#..#.", "#...#"});
    font.define('S', 0, {".####", "#....", ".###.", "....#", "....#", "####."});
    font.define('T', 0, {"#####", "..#..", "..#..", "..#..", "..#..", "..#.."});
    font.define('U', 0, {"#...#", "#...#", "#...#", "#...#", "#...#", ".###."});
    font.define('V', 0, {"#...#", "#...#", "#...#", ".#.#.", ".#.#.", "..#.."});
    font.define('W', 0, {"#...#", "#...#", "#...#", "#.#.#", "#.#.#", ".#.#."});
    font.define('X', 0, {"#...#", "#...#", ".#.#.", ".#.#.", "#...#", "#...#"});
    font.define('Y', 0, {"#...#", "#...#", ".#.#.", "..#..", "..#..", "..#.."});
    font.define('Z', 0, {"#####", "....#", "...#.", "..#..", ".#...", "#####"});
    font.define('[', 0, {"##", "#.", "#.", "#.", "#.", "##"});
    font.define('\\', 1, {"#...", ".#..", ".#..", "..#.", "...#"});
    font.define(']', 0, {"##", ".#", ".#", ".#", ".#", "##"});
    font.define('^', 0, {".#.", "#.#"});
    font.define('_', 6, {"####"});
    font.define('`', 0, {"#.", ".#"});
    font.define('a', 1, {".##.", "...#", ".###", "#..#", ".###"});
    font.define('b', 0, {"#...", "#...", "###.", "#..#", "#..#", "###."});
    font.define('c', 1, {".##.", "#..#", "#...", "#..#", ".##."});
    font.define('d', 0, {"...#", "...#", ".###", "#..#", "#..#", ".###"});
    font.define('e', 1, {".##.", "#..#", "####", "#...", ".###"});
    font.define('f', 0, {"..##", ".#..", "###.", ".#..", ".#..", ".#.."});
    font.define('g', 1, {".###", "#..#", "#..#", ".###", "...#", "###."});
    font.define('h', 0, {"#...", "#...", "###.", "#..#", "#..#", "#..#"});
    font.define('i', 0, {"#", ".", "#", "#", "#", "#"});
    font.define('j', 0, {".#", "..", ".#", ".#", ".#", ".#", "#."});
    font.define('k', 0, {"#...", "#..#", "#.#.", "##..", "#.#.", "#..#"});
    font.define('l', 0, {"##", ".#", ".#", ".#", ".#", ".#"});
    font.define('m', 1, {"##.#.", "#.#.#", "#.#.#", "#.#.#", "#.#.#"});
    font.define('n', 1, {"###.", "#..#", "#..#", "#..#", "#..#"});
    font.define('o', 1, {".##.", "#..#", "#..#", "#..#", ".##."});
    font.define('p', 1, {"###.", "#..#", "#..#", "###.", "#...", "#..."});
    font.define('q', 1, {".###", "#..#", "#..#", ".###", "...#", "...#"});
    font.define('r', 1, {"#.##", "##..", "#...", "#...", "#..."});
    font.define('s', 1, {".###", "#...", ".##.", "...#", "###."});
    font.define('t', 0, {".#..", ".#..", "###.", ".#..", ".#..", "..##"});
    font.define('u', 1, {"#..#", "#..#", "#..#", "#..#", ".###"});
    font.define('v', 1, {"#...#", "#...#", ".#.#.", ".#.#.", "..#.."});
    font.define('w', 1, {"#...#", "#...#", "#.#.#", "#.#.#", ".#.#."});
    font.define('x', 1, {"#..#", "#..#", ".##.", "#..#", "#..#"});
    font.define('y', 1, {"#..#", "#..#", "#..#", ".###", "...#", "###."});
    font.define('z', 1, {"####", "...#", ".##.", "#...", "####"});
    font.define('{', 0, {"..#", ".#.", "##.", ".#.", ".#.", "..#"});
    font.define('|', 0, {"#", "#", "#", "#", "#", "#"});
    font.define('}', 0, {"#..", ".#.", ".##", ".#.", ".#.", "#.."});
    font.define('~', 3, {".#..#", "#..#."});
    return font;
}

const FontData& data() noexcept {
    static const FontData font = make_font();
    return font;
}

} // namespace

const std::array<std::uint8_t, Count * Cell * Cell>& pixels() noexcept {
    return data().pixels;
}

const std::array<int, Count>& widths() noexcept {
    return data().widths;
}

int index(char ch) noexcept {
    return ch < First || ch > Last ? -1 : ch - First;
}

int measure(const std::string_view text) noexcept {
    int width = 0;
    for (const char ch : text) {
        const int glyph = index(ch);
        if (glyph >= 0) {
            width += data().widths[static_cast<std::size_t>(glyph)];
        }
    }
    return width;
}

} // namespace fruityprime::chat::font
