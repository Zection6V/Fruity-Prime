#pragma once

#include <optional>
#include <string>
#include <string_view>

namespace MphRead::Droid
{
    class AndroidConsole final
    {
    public:
        AndroidConsole() = default;
        AndroidConsole(const AndroidConsole&) = delete;
        AndroidConsole& operator=(const AndroidConsole&) = delete;
        AndroidConsole(AndroidConsole&&) = delete;
        AndroidConsole& operator=(AndroidConsole&&) = delete;
        ~AndroidConsole() = default;

        [[nodiscard]] std::string_view Encoding() const noexcept;

        static void Install() noexcept;

        void Write(char16_t value);
        void Write(const std::optional<std::u16string_view>& value);
        void WriteLine(const std::optional<std::u16string_view>& value);
        void Flush();

    private:
        static constexpr std::string_view Tag = "FruityPrime";

        std::u16string _line;
    };
}
