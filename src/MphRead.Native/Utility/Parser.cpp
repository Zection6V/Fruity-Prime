#include "Parser.hpp"

#include "../Formats/Enums.hpp"

#include <array>
#include <bit>
#include <charconv>
#include <cmath>
#include <cstdint>
#include <iostream>
#include <limits>
#include <clocale>
#include <stdexcept>
#include <string>
#include <string_view>
#include <system_error>
#include <typeinfo>
#include <utility>

#if defined(_WIN32)
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>
#endif

#if !defined(_WIN32) && (defined(__unix__) || defined(__APPLE__))
#include <unistd.h>
#endif

namespace
{
#if defined(_WIN32)
    constexpr std::string_view EnvironmentNewLine = "\r\n";
#else
    constexpr std::string_view EnvironmentNewLine = "\n";
#endif

    void CheckConsoleOutput()
    {
        if (!std::cout.good())
        {
            throw std::ios_base::failure("Console output failed.");
        }
    }

    void ConsoleWriteLine(const std::string& message)
    {
        std::cout.write(message.data(), static_cast<std::streamsize>(message.size()));
        std::cout.put('\n');
        std::cout.flush();
        CheckConsoleOutput();
    }

    void ConsoleWriteLine()
    {
        std::cout.put('\n');
        std::cout.flush();
        CheckConsoleOutput();
    }

    void ConsoleWrite(const std::string& message)
    {
        std::cout.write(message.data(), static_cast<std::streamsize>(message.size()));
        std::cout.flush();
        CheckConsoleOutput();
    }

    void ConsoleClear()
    {
#if defined(_WIN32)
        HANDLE output = ::GetStdHandle(STD_OUTPUT_HANDLE);
        if (output == nullptr || output == INVALID_HANDLE_VALUE)
        {
            throw std::ios_base::failure("No console is available.");
        }

        CONSOLE_SCREEN_BUFFER_INFO info{};
        if (::GetConsoleScreenBufferInfo(output, &info) == 0)
        {
            throw std::ios_base::failure("No console is available.");
        }

        const DWORD cellCount = static_cast<DWORD>(info.dwSize.X)
            * static_cast<DWORD>(info.dwSize.Y);
        const COORD home{0, 0};
        DWORD written = 0;
        if (::FillConsoleOutputCharacterW(output, L' ', cellCount, home, &written) == 0
            || ::FillConsoleOutputAttribute(
                output, info.wAttributes, cellCount, home, &written) == 0
            || ::SetConsoleCursorPosition(output, home) == 0)
        {
            throw std::ios_base::failure("The console could not be cleared.");
        }
#elif defined(__unix__) || defined(__APPLE__)
        if (::isatty(STDOUT_FILENO) == 0)
        {
            throw std::ios_base::failure("No console is available.");
        }
        std::cout.write("\x1B[2J\x1B[H", 7);
        std::cout.flush();
        CheckConsoleOutput();
#else
        std::cout.write("\x1B[2J\x1B[H", 7);
        std::cout.flush();
        CheckConsoleOutput();
#endif
    }

    [[nodiscard]] std::string ConsoleReadLine()
    {
        std::string input;
        if (!std::getline(std::cin, input))
        {
            if (std::cin.bad())
            {
                throw std::ios_base::failure("Console input failed.");
            }
            if (std::cin.eof())
            {
                return {};
            }
            throw std::ios_base::failure("Console input failed.");
        }
        if (!input.empty() && input.back() == '\r')
        {
            input.pop_back();
        }
        return input;
    }

    [[nodiscard]] std::string ApplyCurrentDecimalSeparator(std::string value)
    {
        const std::lconv* info = std::localeconv();
        if (info == nullptr || info->decimal_point == nullptr || info->decimal_point[0] == '\0'
            || std::string_view(info->decimal_point) == ".")
        {
            return value;
        }
        const std::size_t point = value.find('.');
        if (point != std::string::npos)
        {
            value.replace(point, 1, info->decimal_point);
        }
        return value;
    }

    [[nodiscard]] std::string FormatExponent(std::int32_t exponent)
    {
        std::array<char, 16> buffer{};
        const std::uint32_t magnitude = exponent < 0
            ? static_cast<std::uint32_t>(-static_cast<std::int64_t>(exponent))
            : static_cast<std::uint32_t>(exponent);
        auto [end, error] = std::to_chars(buffer.data(), buffer.data() + buffer.size(), magnitude);
        if (error != std::errc{})
        {
            throw std::runtime_error("Failed to format exponent.");
        }
        std::string digits(buffer.data(), end);
        if (digits.size() < 2)
        {
            digits.insert(digits.begin(), 2 - digits.size(), '0');
        }
        return std::string(exponent < 0 ? "-" : "+") + digits;
    }

    [[nodiscard]] std::string FormatDoubleInvariantCore(double value)
    {
        if (std::isnan(value))
        {
            return "NaN";
        }
        if (std::isinf(value))
        {
            return std::signbit(value) ? "-Infinity" : "Infinity";
        }

        std::array<char, 128> buffer{};
        auto [end, error] = std::to_chars(
            buffer.data(), buffer.data() + buffer.size(), value, std::chars_format::scientific);
        if (error != std::errc{})
        {
            throw std::runtime_error("Failed to format floating-point value.");
        }
        std::string text(buffer.data(), end);
        const bool negative = !text.empty() && text.front() == '-';
        if (negative)
        {
            text.erase(text.begin());
        }

        const std::size_t exponentPosition = text.find('e');
        if (exponentPosition == std::string::npos)
        {
            throw std::runtime_error("Unexpected floating-point format.");
        }
        std::string_view exponentText(text.data() + exponentPosition + 1, text.size() - exponentPosition - 1);
        if (!exponentText.empty() && exponentText.front() == '+')
        {
            exponentText.remove_prefix(1);
        }
        std::int32_t exponent = 0;
        const auto [parseEnd, parseError] = std::from_chars(
            exponentText.data(), exponentText.data() + exponentText.size(), exponent);
        if (parseError != std::errc{} || parseEnd != exponentText.data() + exponentText.size())
        {
            throw std::runtime_error("Failed to parse floating-point exponent.");
        }
        std::string digits = text.substr(0, exponentPosition);
        const std::size_t point = digits.find('.');
        if (point != std::string::npos)
        {
            digits.erase(point, 1);
        }

        std::string result;
        if (exponent >= -4 && exponent < std::numeric_limits<double>::max_digits10)
        {
            if (exponent >= 0)
            {
                const std::size_t integerDigits = static_cast<std::size_t>(exponent) + 1U;
                if (digits.size() <= integerDigits)
                {
                    result = digits;
                    result.append(integerDigits - digits.size(), '0');
                }
                else
                {
                    result.assign(digits.data(), integerDigits);
                    result.push_back('.');
                    result.append(digits.data() + integerDigits, digits.size() - integerDigits);
                }
            }
            else
            {
                result = "0.";
                result.append(static_cast<std::size_t>(-exponent - 1), '0');
                result += digits;
            }
        }
        else
        {
            result.push_back(digits.front());
            if (digits.size() > 1)
            {
                result.push_back('.');
                result.append(digits.data() + 1, digits.size() - 1);
            }
            result.push_back('E');
            result += FormatExponent(exponent);
        }

        if (negative)
        {
            result.insert(result.begin(), '-');
        }
        return result;
    }

    [[nodiscard]] std::string FormatDouble(double value)
    {
        return ApplyCurrentDecimalSeparator(FormatDoubleInvariantCore(value));
    }

    [[nodiscard]] std::string FormatInt32(std::int32_t value)
    {
        std::array<char, 16> buffer{};
        auto [end, error] = std::to_chars(buffer.data(), buffer.data() + buffer.size(), value);
        if (error != std::errc{})
        {
            throw std::runtime_error("Failed to format Int32 value.");
        }
        return std::string(buffer.data(), end);
    }

    [[nodiscard]] std::string FormatUInt32(std::uint32_t value)
    {
        std::array<char, 16> buffer{};
        auto [end, error] = std::to_chars(buffer.data(), buffer.data() + buffer.size(), value);
        if (error != std::errc{})
        {
            throw std::runtime_error("Failed to format UInt32 value.");
        }
        return std::string(buffer.data(), end);
    }

    [[nodiscard]] std::string EnumToString(const std::type_info& enumType, std::int32_t value)
    {
        if (enumType == typeid(MphRead::PolygonMode))
        {
            const std::uint32_t raw = std::bit_cast<std::uint32_t>(value);
            switch (raw)
            {
            case static_cast<std::uint32_t>(MphRead::PolygonMode::Modulate):
                return "Modulate";
            case static_cast<std::uint32_t>(MphRead::PolygonMode::Decal):
                return "Decal";
            case static_cast<std::uint32_t>(MphRead::PolygonMode::Toon):
                return "Toon";
            case static_cast<std::uint32_t>(MphRead::PolygonMode::Shadow):
                return "Shadow";
            default:
                return FormatUInt32(raw);
            }
        }
        return FormatInt32(value);
    }

    [[nodiscard]] constexpr bool IsHexWhiteSpace(unsigned char value) noexcept
    {
        return value == 0x20U || (value >= 0x09U && value <= 0x0DU);
    }

    [[nodiscard]] bool TryParseHexInt32(std::string_view text, std::int32_t& result) noexcept
    {
        std::size_t first = 0;
        while (first < text.size() && IsHexWhiteSpace(static_cast<unsigned char>(text[first])))
        {
            ++first;
        }
        std::size_t last = text.size();
        while (last > first && IsHexWhiteSpace(static_cast<unsigned char>(text[last - 1])))
        {
            --last;
        }
        if (first == last)
        {
            result = 0;
            return false;
        }

        std::uint32_t parsed = 0;
        for (std::size_t index = first; index < last; ++index)
        {
            const unsigned char ch = static_cast<unsigned char>(text[index]);
            std::uint32_t digit = 0;
            if (ch >= '0' && ch <= '9')
            {
                digit = ch - '0';
            }
            else if (ch >= 'A' && ch <= 'F')
            {
                digit = ch - 'A' + 10U;
            }
            else if (ch >= 'a' && ch <= 'f')
            {
                digit = ch - 'a' + 10U;
            }
            else
            {
                result = 0;
                return false;
            }
            parsed = (parsed << 4U) | digit;
        }
        result = std::bit_cast<std::int32_t>(parsed);
        return true;
    }

    [[nodiscard]] std::string ToBinaryString(std::int32_t value)
    {
        std::string result(32, '0');
        const std::uint32_t bits = std::bit_cast<std::uint32_t>(value);
        for (std::uint32_t bit = 0; bit < 32U; ++bit)
        {
            if ((bits & (1U << bit)) != 0)
            {
                result[31U - bit] = '1';
            }
        }
        return result;
    }

}

namespace MphRead::Utility
{
    Parser::Thing::Thing(std::int32_t bit, std::string name, ThingType type)
        : Name(std::move(name)), Type(type), EnumType(nullptr), BitFrom(bit), BitTo(bit)
    {
    }

    Parser::Thing::Thing(std::int32_t bit, std::string name, const std::type_info& enumType)
        : Name(std::move(name)), Type(ThingType::Enum), EnumType(&enumType), BitFrom(bit), BitTo(bit)
    {
    }

    Parser::Thing::Thing(
        std::int32_t bitFrom, std::int32_t bitTo, std::string name, ThingType type)
        : Name(std::move(name)), Type(type), EnumType(nullptr), BitFrom(bitFrom), BitTo(bitTo)
    {
    }

    Parser::Thing::Thing(
        std::int32_t bitFrom, std::int32_t bitTo, std::string name, const std::type_info& enumType)
        : Name(std::move(name)), Type(ThingType::Enum), EnumType(&enumType), BitFrom(bitFrom), BitTo(bitTo)
    {
    }

    std::string Parser::Thing::Get(std::int32_t value) const
    {
        const std::int32_t width = ManagedAdd(ManagedSubtract(BitTo, BitFrom), 1);
        const std::int32_t mask = ManagedNot(ManagedLeftShift(-1, width));
        value = ManagedAnd(ManagedArithmeticRightShift(value, BitFrom), mask);

        std::string output;
        if (Type == ThingType::Boolean)
        {
            output = value == 0 ? "No" : "Yes";
        }
        else if (Type == ThingType::Enum)
        {
            if (EnumType == nullptr)
            {
                throw std::invalid_argument("enumType");
            }
            output = EnumToString(*EnumType, value);
        }
        else
        {
            output = FormatInt32(value);
        }
        return Name + ": " + output;
    }

    const std::unordered_map<std::string, std::vector<Parser::Thing>> Parser::_things = {
        {
            "POLYGON_ATTR",
            {
                Thing(0, "Light 1", ThingType::Boolean),
                Thing(1, "Light 2", ThingType::Boolean),
                Thing(2, "Light 3", ThingType::Boolean),
                Thing(3, "Light 4", ThingType::Boolean),
                Thing(4, 5, "Polygon mode", typeid(PolygonMode)),
                Thing(6, "Back face", ThingType::Boolean),
                Thing(7, "Front face", ThingType::Boolean),
                Thing(11, "Set new depth", ThingType::Boolean),
                Thing(12, "Render far", ThingType::Boolean),
                Thing(13, "Render 1-dot", ThingType::Boolean),
                Thing(14, "Equal depth test", ThingType::Boolean),
                Thing(15, "Enable fog", ThingType::Boolean),
                Thing(16, 20, "Alpha", ThingType::General),
                Thing(24, 29, "Polygon ID", ThingType::General)
            }
        }
    };

    void Parser::ParseFloat(std::uint64_t value)
    {
        const double d = std::bit_cast<double>(value);
        ConsoleWriteLine(FormatDouble(d) + " (" + FormatDouble(d / 4096.0F) + ")");
    }

    void Parser::MainLoop()
    {
        while (true)
        {
            ConsoleClear();
            ConsoleWriteLine(std::string("1: POLYGON_ATTR") + std::string(EnvironmentNewLine) + "x: quit");
            const std::string input = ConsoleReadLine();
            if (input == "x" || input == "X")
            {
                break;
            }

            const std::string* type = nullptr;
            static const std::string polygonAttr = "POLYGON_ATTR";
            if (input == "1")
            {
                type = &polygonAttr;
            }
            if (type != nullptr)
            {
                bool hasOutput = false;
                std::string output;
                while (true)
                {
                    ConsoleClear();
                    if (hasOutput)
                    {
                        ConsoleWriteLine(output);
                        ConsoleWriteLine();
                        output.clear();
                        hasOutput = false;
                    }
                    ConsoleWrite("Value: ");
                    const std::string value = ConsoleReadLine();
                    if (value == "x" || value == "X")
                    {
                        break;
                    }

                    std::int32_t result = 0;
                    if (value.size() <= 8 && TryParseHexInt32(value, result))
                    {
                        output = ToBinaryString(result);
                        const auto& things = _things.at(*type);
                        for (const Thing& thing : things)
                        {
                            output += EnvironmentNewLine;
                            output += thing.Get(result);
                        }
                        hasOutput = true;
                    }
                }
            }
        }
    }
}
