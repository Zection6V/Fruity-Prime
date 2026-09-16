#pragma once

#include <cstdint>
#include <optional>
#include <streambuf>
#include <string>
#include <string_view>

namespace MphRead::Droid
{
    class AndroidConsole final : private std::streambuf
    {
    public:
        AndroidConsole() = default;
        AndroidConsole(const AndroidConsole&) = delete;
        AndroidConsole& operator=(const AndroidConsole&) = delete;
        ~AndroidConsole() override = default;

        [[nodiscard]] std::string_view Encoding() const noexcept;

        static void Install() noexcept;

        void Write(char16_t value);
        void Write(const std::optional<std::u16string_view>& value);
        void WriteLine(const std::optional<std::u16string_view>& value);
        void Flush();

    private:
        static constexpr std::string_view Tag = "FruityPrime";

        std::u16string _line;
        std::uint32_t _streamCodePoint = 0;
        std::uint32_t _streamMinimum = 0;
        unsigned int _streamRemaining = 0;

        void WriteStreamByte(unsigned char value);
        void FinishIncompleteStreamCharacter();
        void AppendStreamCodePoint(std::uint32_t value);

        int_type overflow(int_type value) override;
        std::streamsize xsputn(const char* data, std::streamsize count) override;
        int sync() override;
    };
}
