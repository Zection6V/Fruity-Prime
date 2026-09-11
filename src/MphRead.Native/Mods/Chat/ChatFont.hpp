#pragma once

#include <cstdint>
#include <span>

namespace MphRead::Mods::Chat
{
    class ChatFont final
    {
    public:
        ChatFont() = delete;
        ChatFont(const ChatFont&) = delete;
        ChatFont& operator=(const ChatFont&) = delete;

        inline static constexpr std::int32_t Cell = 8;
        inline static constexpr char16_t First = u' ';
        inline static constexpr char16_t Last = u'~';

        static std::span<std::uint8_t> Pixels();
        static std::span<std::int32_t> Widths();

        static std::int32_t Index(char16_t ch);
        static std::int32_t Measure(std::span<const char16_t> text);

    private:
        inline static constexpr std::int32_t Count = Last - First + 1;

        struct State;
        static State& GetState();
    };
}
