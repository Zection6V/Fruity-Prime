#include "ChatFont.hpp"

#include <array>
#include <cstddef>
#include <cstdint>
#include <initializer_list>
#include <string_view>

namespace MphRead::Mods::Chat
{
    struct ChatFont::State
    {
        std::array<std::uint8_t,
            static_cast<std::size_t>(Count * Cell * Cell)> Pixels{};
        std::array<std::int32_t, static_cast<std::size_t>(Count)> Widths{};

        State();

        void Define(char16_t ch, std::int32_t advance);
        void Define(char16_t ch, std::initializer_list<std::u16string_view> rows);
        void Define(char16_t ch, std::int32_t top,
            std::initializer_list<std::u16string_view> rows);
    };

    void ChatFont::State::Define(char16_t ch, std::int32_t advance)
    {
        Widths[static_cast<std::size_t>(ch - First)] = advance;
    }

    void ChatFont::State::Define(
        char16_t ch, std::initializer_list<std::u16string_view> rows)
    {
        Define(ch, 0, rows);
    }

    void ChatFont::State::Define(char16_t ch, std::int32_t top,
        std::initializer_list<std::u16string_view> rows)
    {
        const std::int32_t glyph = static_cast<std::int32_t>(ch - First) * Cell * Cell;
        std::int32_t width = 0;
        std::int32_t r = 0;
        for (std::u16string_view row : rows)
        {
            for (std::size_t c = 0;
                c < row.size() && c < static_cast<std::size_t>(Cell); c++)
            {
                if (row[c] == u'#')
                {
                    Pixels[static_cast<std::size_t>(
                        glyph + (top + r) * Cell + static_cast<std::int32_t>(c))] = 1;
                    const std::int32_t inkWidth = static_cast<std::int32_t>(c) + 1;
                    if (inkWidth > width)
                    {
                        width = inkWidth;
                    }
                }
            }
            r++;
        }
        Widths[static_cast<std::size_t>(ch - First)] = width + 1;
    }

    ChatFont::State::State()
    {
        Define(u' ', 3);
        Define(u'!', {u"#", u"#", u"#", u"#", u".", u"#"});
        Define(u'"', {u"#.#", u"#.#"});
        Define(u'#', 1, {u".#.#.", u"#####", u".#.#.", u"#####", u".#.#."});
        Define(u'$', {u"..#..", u".####", u"##...", u".###.", u"...##", u"####.", u"..#.."});
        Define(u'%', 1, {u"##..#", u"##.#.", u"..#..", u".#.##", u"#..##"});
        Define(u'&', 1, {u".##..", u"#..#.", u".##..", u"#..##", u".##.#"});
        Define(u'\'', {u"#", u"#"});
        Define(u'(', {u".#", u"#.", u"#.", u"#.", u"#.", u".#"});
        Define(u')', {u"#.", u".#", u".#", u".#", u".#", u"#."});
        Define(u'*', 1, {u"#.#", u".#.", u"#.#"});
        Define(u'+', 2, {u".#.", u"###", u".#."});
        Define(u',', 5, {u".#", u"#."});
        Define(u'-', 3, {u"###"});
        Define(u'.', 5, {u"#"});
        Define(u'/', 1, {u"...#", u"..#.", u"..#.", u".#..", u"#..."});
        Define(u'0', {u".##.", u"#..#", u"#.##", u"##.#", u"#..#", u".##."});
        Define(u'1', {u".#.", u"##.", u".#.", u".#.", u".#.", u"###"});
        Define(u'2', {u".##.", u"#..#", u"...#", u"..#.", u".#..", u"####"});
        Define(u'3', {u"###.", u"...#", u".##.", u"...#", u"#..#", u".##."});
        Define(u'4', {u"..#.", u".##.", u"#.#.", u"####", u"..#.", u"..#."});
        Define(u'5', {u"####", u"#...", u"###.", u"...#", u"#..#", u".##."});
        Define(u'6', {u".##.", u"#...", u"###.", u"#..#", u"#..#", u".##."});
        Define(u'7', {u"####", u"...#", u"..#.", u"..#.", u".#..", u".#.."});
        Define(u'8', {u".##.", u"#..#", u".##.", u"#..#", u"#..#", u".##."});
        Define(u'9', {u".##.", u"#..#", u"#..#", u".###", u"...#", u".##."});
        Define(u':', 2, {u"#", u".", u".", u"#"});
        Define(u';', 2, {u".#", u"..", u"..", u".#", u"#."});
        Define(u'<', 1, {u"..#", u".#.", u"#..", u".#.", u"..#"});
        Define(u'=', 2, {u"####", u"....", u"####"});
        Define(u'>', 1, {u"#..", u".#.", u"..#", u".#.", u"#.."});
        Define(u'?', {u".##.", u"#..#", u"...#", u"..#.", u"....", u"..#."});
        Define(u'@', {u".###.", u"#...#", u"#.###", u"#.#.#", u"#....", u".###."});
        Define(u'A', {u".###.", u"#...#", u"#...#", u"#####", u"#...#", u"#...#"});
        Define(u'B', {u"####.", u"#...#", u"####.", u"#...#", u"#...#", u"####."});
        Define(u'C', {u".###.", u"#...#", u"#....", u"#....", u"#...#", u".###."});
        Define(u'D', {u"####.", u"#...#", u"#...#", u"#...#", u"#...#", u"####."});
        Define(u'E', {u"#####", u"#....", u"####.", u"#....", u"#....", u"#####"});
        Define(u'F', {u"#####", u"#....", u"####.", u"#....", u"#....", u"#...."});
        Define(u'G', {u".###.", u"#...#", u"#....", u"#..##", u"#...#", u".###."});
        Define(u'H', {u"#...#", u"#...#", u"#####", u"#...#", u"#...#", u"#...#"});
        Define(u'I', {u"###", u".#.", u".#.", u".#.", u".#.", u"###"});
        Define(u'J', {u"...##", u"....#", u"....#", u"....#", u"#...#", u".###."});
        Define(u'K', {u"#...#", u"#..#.", u"##...", u"##...", u"#..#.", u"#...#"});
        Define(u'L', {u"#....", u"#....", u"#....", u"#....", u"#....", u"#####"});
        Define(u'M', {u"#...#", u"##.##", u"#.#.#", u"#...#", u"#...#", u"#...#"});
        Define(u'N', {u"#...#", u"##..#", u"#.#.#", u"#..##", u"#...#", u"#...#"});
        Define(u'O', {u".###.", u"#...#", u"#...#", u"#...#", u"#...#", u".###."});
        Define(u'P', {u"####.", u"#...#", u"#...#", u"####.", u"#....", u"#...."});
        Define(u'Q', {u".###.", u"#...#", u"#...#", u"#...#", u"#..#.", u".##.#"});
        Define(u'R', {u"####.", u"#...#", u"#...#", u"####.", u"#..#.", u"#...#"});
        Define(u'S', {u".####", u"#....", u".###.", u"....#", u"....#", u"####."});
        Define(u'T', {u"#####", u"..#..", u"..#..", u"..#..", u"..#..", u"..#.."});
        Define(u'U', {u"#...#", u"#...#", u"#...#", u"#...#", u"#...#", u".###."});
        Define(u'V', {u"#...#", u"#...#", u"#...#", u".#.#.", u".#.#.", u"..#.."});
        Define(u'W', {u"#...#", u"#...#", u"#...#", u"#.#.#", u"#.#.#", u".#.#."});
        Define(u'X', {u"#...#", u"#...#", u".#.#.", u".#.#.", u"#...#", u"#...#"});
        Define(u'Y', {u"#...#", u"#...#", u".#.#.", u"..#..", u"..#..", u"..#.."});
        Define(u'Z', {u"#####", u"....#", u"...#.", u"..#..", u".#...", u"#####"});
        Define(u'[', {u"##", u"#.", u"#.", u"#.", u"#.", u"##"});
        Define(u'\\', 1, {u"#...", u".#..", u".#..", u"..#.", u"...#"});
        Define(u']', {u"##", u".#", u".#", u".#", u".#", u"##"});
        Define(u'^', {u".#.", u"#.#"});
        Define(u'_', 6, {u"####"});
        Define(u'`', {u"#.", u".#"});
        Define(u'a', 1, {u".##.", u"...#", u".###", u"#..#", u".###"});
        Define(u'b', {u"#...", u"#...", u"###.", u"#..#", u"#..#", u"###."});
        Define(u'c', 1, {u".##.", u"#..#", u"#...", u"#..#", u".##."});
        Define(u'd', {u"...#", u"...#", u".###", u"#..#", u"#..#", u".###"});
        Define(u'e', 1, {u".##.", u"#..#", u"####", u"#...", u".###"});
        Define(u'f', {u"..##", u".#..", u"###.", u".#..", u".#..", u".#.."});
        Define(u'g', 1, {u".###", u"#..#", u"#..#", u".###", u"...#", u"###."});
        Define(u'h', {u"#...", u"#...", u"###.", u"#..#", u"#..#", u"#..#"});
        Define(u'i', {u"#", u".", u"#", u"#", u"#", u"#"});
        Define(u'j', {u".#", u"..", u".#", u".#", u".#", u".#", u"#."});
        Define(u'k', {u"#...", u"#..#", u"#.#.", u"##..", u"#.#.", u"#..#"});
        Define(u'l', {u"##", u".#", u".#", u".#", u".#", u".#"});
        Define(u'm', 1, {u"##.#.", u"#.#.#", u"#.#.#", u"#.#.#", u"#.#.#"});
        Define(u'n', 1, {u"###.", u"#..#", u"#..#", u"#..#", u"#..#"});
        Define(u'o', 1, {u".##.", u"#..#", u"#..#", u"#..#", u".##."});
        Define(u'p', 1, {u"###.", u"#..#", u"#..#", u"###.", u"#...", u"#..."});
        Define(u'q', 1, {u".###", u"#..#", u"#..#", u".###", u"...#", u"...#"});
        Define(u'r', 1, {u"#.##", u"##..", u"#...", u"#...", u"#..."});
        Define(u's', 1, {u".###", u"#...", u".##.", u"...#", u"###."});
        Define(u't', {u".#..", u".#..", u"###.", u".#..", u".#..", u"..##"});
        Define(u'u', 1, {u"#..#", u"#..#", u"#..#", u"#..#", u".###"});
        Define(u'v', 1, {u"#...#", u"#...#", u".#.#.", u".#.#.", u"..#.."});
        Define(u'w', 1, {u"#...#", u"#...#", u"#.#.#", u"#.#.#", u".#.#."});
        Define(u'x', 1, {u"#..#", u"#..#", u".##.", u"#..#", u"#..#"});
        Define(u'y', 1, {u"#..#", u"#..#", u"#..#", u".###", u"...#", u"###."});
        Define(u'z', 1, {u"####", u"...#", u".##.", u"#...", u"####"});
        Define(u'{', {u"..#", u".#.", u"##.", u".#.", u".#.", u"..#"});
        Define(u'|', {u"#", u"#", u"#", u"#", u"#", u"#"});
        Define(u'}', {u"#..", u".#.", u".##", u".#.", u".#.", u"#.."});
        Define(u'~', 3, {u".#..#", u"#..#."});
    }

    ChatFont::State& ChatFont::GetState()
    {
        static State state;
        return state;
    }

    std::span<std::uint8_t> ChatFont::Pixels()
    {
        State& state = GetState();
        return std::span<std::uint8_t>(state.Pixels.data(), state.Pixels.size());
    }

    std::span<std::int32_t> ChatFont::Widths()
    {
        State& state = GetState();
        return std::span<std::int32_t>(state.Widths.data(), state.Widths.size());
    }

    std::int32_t ChatFont::Index(char16_t ch)
    {
        (void)GetState();
        return ch < First || ch > Last
            ? -1
            : static_cast<std::int32_t>(ch - First);
    }

    std::int32_t ChatFont::Measure(std::span<const char16_t> text)
    {
        State& state = GetState();

        std::uint32_t width = 0;
        for (char16_t ch : text)
        {
            const std::int32_t index = ch < First || ch > Last
                ? -1
                : static_cast<std::int32_t>(ch - First);
            if (index >= 0)
            {
                width += static_cast<std::uint32_t>(
                    state.Widths[static_cast<std::size_t>(index)]);
            }
        }

        if (width <= 0x7FFFFFFFU)
        {
            return static_cast<std::int32_t>(width);
        }
        return static_cast<std::int32_t>(
            static_cast<std::int64_t>(width) - 0x100000000LL);
    }
}
