#include "InputSettings.hpp"
#include "NativeRuntime/System/Charconv.hpp"

#include "Input/PadBindings.hpp"
#include "Input/PointerInput.hpp"
#include "Input/StylusZone.hpp"
#include "Input/TouchSettings.hpp"
#include "Network/DemoClip.hpp"
#include "Branding.hpp"
#include "../Entities/Players/PlayerEntity.hpp"
#include "Launcher/Portable/LauncherPrefs.hpp"
#include "../NativeRuntime/System/Globalization.hpp"
#include "../NativeRuntime/System/Managed.hpp"

#include <algorithm>
#include <array>
#include <charconv>
#include <cmath>
#include <cstdint>
#include <fstream>
#include <limits>
#include <iterator>
#include <stdexcept>
#include <string>
#include <string_view>
#include <system_error>
#include <type_traits>
#include <utility>
#include <vector>

using ::MphRead::NativeRuntime::Int32TryParseInvariant;
using ::MphRead::NativeRuntime::MathClamp;
using ::MphRead::NativeRuntime::StringEqualsOrdinalIgnoreCase;
using ::MphRead::NativeRuntime::StringTrimView;

namespace MphRead::Mods
{
    namespace
    {
        constexpr InputButtonType ButtonTypeKey = static_cast<InputButtonType>(0);
        constexpr InputButtonType ButtonTypeMouse = static_cast<InputButtonType>(1);
        constexpr InputButtonType ButtonTypeScrollUp = static_cast<InputButtonType>(2);
        constexpr InputButtonType ButtonTypeScrollDown = static_cast<InputButtonType>(3);

        constexpr InputKey KeyUnknown = static_cast<InputKey>(-1);
        constexpr InputKey KeyT = static_cast<InputKey>(84);
        constexpr InputKey KeyF10 = static_cast<InputKey>(299);

        constexpr InputMouseButton MouseLeft = static_cast<InputMouseButton>(0);
        constexpr InputMouseButton MouseRight = static_cast<InputMouseButton>(1);
        constexpr InputMouseButton MouseMiddle = static_cast<InputMouseButton>(2);

        struct EnumName final
        {
            std::int32_t Value;
            std::string_view Name;
        };

        constexpr EnumName KeyNames[] =
        {
            {-1, "Unknown"},
            {32, "Space"},
            {39, "Apostrophe"},
            {44, "Comma"},
            {45, "Minus"},
            {46, "Period"},
            {47, "Slash"},
            {48, "D0"},
            {49, "D1"},
            {50, "D2"},
            {51, "D3"},
            {52, "D4"},
            {53, "D5"},
            {54, "D6"},
            {55, "D7"},
            {56, "D8"},
            {57, "D9"},
            {59, "Semicolon"},
            {61, "Equal"},
            {65, "A"},
            {66, "B"},
            {67, "C"},
            {68, "D"},
            {69, "E"},
            {70, "F"},
            {71, "G"},
            {72, "H"},
            {73, "I"},
            {74, "J"},
            {75, "K"},
            {76, "L"},
            {77, "M"},
            {78, "N"},
            {79, "O"},
            {80, "P"},
            {81, "Q"},
            {82, "R"},
            {83, "S"},
            {84, "T"},
            {85, "U"},
            {86, "V"},
            {87, "W"},
            {88, "X"},
            {89, "Y"},
            {90, "Z"},
            {91, "LeftBracket"},
            {92, "Backslash"},
            {93, "RightBracket"},
            {96, "GraveAccent"},
            {256, "Escape"},
            {257, "Enter"},
            {258, "Tab"},
            {259, "Backspace"},
            {260, "Insert"},
            {261, "Delete"},
            {262, "Right"},
            {263, "Left"},
            {264, "Down"},
            {265, "Up"},
            {266, "PageUp"},
            {267, "PageDown"},
            {268, "Home"},
            {269, "End"},
            {280, "CapsLock"},
            {281, "ScrollLock"},
            {282, "NumLock"},
            {283, "PrintScreen"},
            {284, "Pause"},
            {290, "F1"},
            {291, "F2"},
            {292, "F3"},
            {293, "F4"},
            {294, "F5"},
            {295, "F6"},
            {296, "F7"},
            {297, "F8"},
            {298, "F9"},
            {299, "F10"},
            {300, "F11"},
            {301, "F12"},
            {302, "F13"},
            {303, "F14"},
            {304, "F15"},
            {305, "F16"},
            {306, "F17"},
            {307, "F18"},
            {308, "F19"},
            {309, "F20"},
            {310, "F21"},
            {311, "F22"},
            {312, "F23"},
            {313, "F24"},
            {314, "F25"},
            {320, "KeyPad0"},
            {321, "KeyPad1"},
            {322, "KeyPad2"},
            {323, "KeyPad3"},
            {324, "KeyPad4"},
            {325, "KeyPad5"},
            {326, "KeyPad6"},
            {327, "KeyPad7"},
            {328, "KeyPad8"},
            {329, "KeyPad9"},
            {330, "KeyPadDecimal"},
            {331, "KeyPadDivide"},
            {332, "KeyPadMultiply"},
            {333, "KeyPadSubtract"},
            {334, "KeyPadAdd"},
            {335, "KeyPadEnter"},
            {336, "KeyPadEqual"},
            {340, "LeftShift"},
            {341, "LeftControl"},
            {342, "LeftAlt"},
            {343, "LeftSuper"},
            {344, "RightShift"},
            {345, "RightControl"},
            {346, "RightAlt"},
            {347, "RightSuper"},
            {348, "Menu"}
        };

        constexpr EnumName KeyAliases[] =
        {
            {348, "LastKey"}
        };

        constexpr EnumName MouseButtonNames[] =
        {
            {0, "Button1"},
            {1, "Button2"},
            {2, "Button3"},
            {3, "Button4"},
            {4, "Button5"},
            {5, "Button6"},
            {6, "Button7"},
            {7, "Button8"}
        };

        constexpr EnumName MouseButtonAliases[] =
        {
            {0, "Left"},
            {1, "Right"},
            {2, "Middle"},
            {7, "Last"}
        };

        constexpr std::array<std::pair<std::int32_t, std::string_view>, 17>
            GamepadButtonNames =
        {{
            {0, "None"},
            {1 << 0, "A"},
            {1 << 1, "B"},
            {1 << 2, "X"},
            {1 << 3, "Y"},
            {1 << 4, "LeftBumper"},
            {1 << 5, "RightBumper"},
            {1 << 6, "Back"},
            {1 << 7, "Start"},
            {1 << 8, "LeftThumb"},
            {1 << 9, "RightThumb"},
            {1 << 10, "DpadUp"},
            {1 << 11, "DpadRight"},
            {1 << 12, "DpadDown"},
            {1 << 13, "DpadLeft"},
            {1 << 14, "LeftTrigger"},
            {1 << 15, "RightTrigger"}
        }};

        std::string TrimCopy(std::string_view value)
        {
            const std::string_view trimmed = StringTrimView(value);
            return std::string(trimmed);
        }

        char AsciiLower(char value)
        {
            return value >= 'A' && value <= 'Z'
                ? static_cast<char>(value + ('a' - 'A'))
                : value;
        }

        bool TryParseBoolean(std::string_view value, bool& parsed)
        {
            value = StringTrimView(value);
            if (StringEqualsOrdinalIgnoreCase(value, "true"))
            {
                parsed = true;
                return true;
            }
            if (StringEqualsOrdinalIgnoreCase(value, "false"))
            {
                parsed = false;
                return true;
            }
            return false;
        }

        bool TryParseSingle(std::string_view value, float& parsed)
        {
            value = StringTrimView(value);
            if (value.empty())
            {
                return false;
            }

            if (StringEqualsOrdinalIgnoreCase(value, "NaN"))
            {
                parsed = std::numeric_limits<float>::quiet_NaN();
                return true;
            }
            if (StringEqualsOrdinalIgnoreCase(value, "Infinity")
                || StringEqualsOrdinalIgnoreCase(value, "+Infinity"))
            {
                parsed = std::numeric_limits<float>::infinity();
                return true;
            }
            if (StringEqualsOrdinalIgnoreCase(value, "-Infinity"))
            {
                parsed = -std::numeric_limits<float>::infinity();
                return true;
            }

            bool positiveSign = false;
            if (value.front() == '+')
            {
                positiveSign = true;
                value.remove_prefix(1);
                if (value.empty())
                {
                    return false;
                }
            }

            float result = 0.0F;
            const char* const end = value.data() + value.size();
            const auto [ptr, error] = ::MphRead::NativeRuntime::FromChars(
                value.data(), end, result, std::chars_format::general);
            if (ptr != end)
            {
                return false;
            }
            if (error == std::errc::result_out_of_range)
            {
                // .NET Single.TryParse saturates overflow to infinity but
                // underflows toward signed zero. A double parse separates the
                // two ranges without introducing locale-sensitive strtof.
                double wide = 0.0;
                const auto [widePtr, wideError] = ::MphRead::NativeRuntime::FromChars(
                    value.data(), end, wide, std::chars_format::general);
                if (widePtr != end)
                {
                    return false;
                }
                if (wideError == std::errc{})
                {
                    if (wide > std::numeric_limits<float>::max())
                    {
                        parsed = std::numeric_limits<float>::infinity();
                    }
                    else if (wide < -std::numeric_limits<float>::max())
                    {
                        parsed = -std::numeric_limits<float>::infinity();
                    }
                    else
                    {
                        parsed = static_cast<float>(wide);
                    }
                    return true;
                }
                if (wideError != std::errc::result_out_of_range)
                {
                    return false;
                }

                const bool negative = !value.empty() && value.front() == '-';
                std::string_view magnitude = value;
                if (!magnitude.empty()
                    && (magnitude.front() == '-' || magnitude.front() == '+'))
                {
                    magnitude.remove_prefix(1);
                }
                const std::size_t exponentAt = magnitude.find_first_of("eE");
                const std::string_view mantissa = magnitude.substr(0, exponentAt);
                const std::size_t dot = mantissa.find('.');
                const std::size_t decimalIndex = dot == std::string_view::npos
                    ? mantissa.size()
                    : dot;
                std::size_t digitIndex = 0;
                std::size_t firstNonZero = std::string_view::npos;
                for (const char ch : mantissa)
                {
                    if (ch == '.')
                    {
                        continue;
                    }
                    if (ch != '0' && firstNonZero == std::string_view::npos)
                    {
                        firstNonZero = digitIndex;
                    }
                    ++digitIndex;
                }
                if (firstNonZero == std::string_view::npos)
                {
                    parsed = negative ? -0.0F : 0.0F;
                    return true;
                }

                std::int64_t exponent = 0;
                if (exponentAt != std::string_view::npos)
                {
                    std::string_view exponentText = magnitude.substr(exponentAt + 1);
                    bool exponentNegative = false;
                    if (!exponentText.empty()
                        && (exponentText.front() == '+' || exponentText.front() == '-'))
                    {
                        exponentNegative = exponentText.front() == '-';
                        exponentText.remove_prefix(1);
                    }
                    const auto [expPtr, expError] = std::from_chars(
                        exponentText.data(), exponentText.data() + exponentText.size(), exponent, 10);
                    if (expError == std::errc::result_out_of_range)
                    {
                        exponent = exponentNegative ? -1000000 : 1000000;
                    }
                    else if (expError != std::errc{}
                        || expPtr != exponentText.data() + exponentText.size())
                    {
                        return false;
                    }
                    else if (exponentNegative)
                    {
                        exponent = -exponent;
                    }
                }

                const std::int64_t scientificExponent = exponent
                    + static_cast<std::int64_t>(decimalIndex)
                    - static_cast<std::int64_t>(firstNonZero) - 1;
                if (scientificExponent >= 0)
                {
                    parsed = negative
                        ? -std::numeric_limits<float>::infinity()
                        : std::numeric_limits<float>::infinity();
                }
                else
                {
                    parsed = negative ? -0.0F : 0.0F;
                }
                return true;
            }
            if (error != std::errc{})
            {
                return false;
            }
            if (positiveSign && std::signbit(result))
            {
                return false;
            }
            parsed = result;
            return true;
        }

        template <typename TEnum>
        std::int32_t EnumValue(TEnum value) noexcept
        {
            using Underlying = std::underlying_type_t<TEnum>;
            return static_cast<std::int32_t>(static_cast<Underlying>(value));
        }

        std::string EnumToString(std::int32_t value,
            const EnumName* names, std::size_t count)
        {
            for (std::size_t i = 0; i < count; ++i)
            {
                if (names[i].Value == value)
                {
                    return std::string(names[i].Name);
                }
            }
            return std::to_string(value);
        }

        bool TryParseNamedEnum(std::string_view value,
            const EnumName* names, std::size_t count,
            const EnumName* aliases, std::size_t aliasCount,
            std::int32_t& parsed)
        {
            value = StringTrimView(value);
            if (value.empty())
            {
                return false;
            }

            const char first = value.front();
            if ((first >= '0' && first <= '9') || first == '+' || first == '-')
            {
                return Int32TryParseInvariant(value, parsed);
            }

            std::uint32_t combined = 0;
            std::size_t start = 0;
            while (true)
            {
                const std::size_t comma = value.find(',', start);
                const std::size_t length = comma == std::string_view::npos
                    ? std::string_view::npos
                    : comma - start;
                const std::string_view part =
                    StringTrimView(value.substr(start, length));
                if (part.empty())
                {
                    return false;
                }

                bool found = false;
                for (std::size_t i = 0; i < count && !found; ++i)
                {
                    if (part == names[i].Name)
                    {
                        combined |= static_cast<std::uint32_t>(names[i].Value);
                        found = true;
                    }
                }
                for (std::size_t i = 0; i < aliasCount && !found; ++i)
                {
                    if (part == aliases[i].Name)
                    {
                        combined |= static_cast<std::uint32_t>(aliases[i].Value);
                        found = true;
                    }
                }
                if (!found)
                {
                    return false;
                }
                if (comma == std::string_view::npos)
                {
                    break;
                }
                start = comma + 1;
            }

            parsed = static_cast<std::int32_t>(combined);
            return true;
        }

        std::string KeyToString(InputKey key)
        {
            return EnumToString(EnumValue(key), KeyNames, std::size(KeyNames));
        }

        bool TryParseKey(std::string_view value, InputKey& parsed)
        {
            std::int32_t numeric = 0;
            if (!TryParseNamedEnum(value, KeyNames, std::size(KeyNames),
                KeyAliases, std::size(KeyAliases), numeric))
            {
                return false;
            }
            parsed = static_cast<InputKey>(numeric);
            return true;
        }

        std::string MouseButtonToString(InputMouseButton button)
        {
            return EnumToString(EnumValue(button),
                MouseButtonNames, std::size(MouseButtonNames));
        }

        bool TryParseMouseButton(std::string_view value,
            InputMouseButton& parsed)
        {
            std::int32_t numeric = 0;
            if (!TryParseNamedEnum(value,
                MouseButtonNames, std::size(MouseButtonNames),
                MouseButtonAliases, std::size(MouseButtonAliases), numeric))
            {
                return false;
            }
            parsed = static_cast<InputMouseButton>(numeric);
            return true;
        }

        std::string GamepadButtonsToString(Input::GamepadButtons buttons)
        {
            const std::int32_t value = static_cast<std::int32_t>(buttons);
            for (const auto& entry : GamepadButtonNames)
            {
                if (entry.first == value)
                {
                    return std::string(entry.second);
                }
            }

            std::uint32_t remaining = static_cast<std::uint32_t>(value);
            std::array<std::string_view, 16> found{};
            std::size_t foundCount = 0;
            for (std::size_t i = GamepadButtonNames.size(); i-- > 1;)
            {
                const std::uint32_t flag =
                    static_cast<std::uint32_t>(GamepadButtonNames[i].first);
                if ((remaining & flag) == flag)
                {
                    remaining &= ~flag;
                    found[foundCount++] = GamepadButtonNames[i].second;
                    if (remaining == 0)
                    {
                        break;
                    }
                }
            }
            if (remaining != 0 || foundCount == 0)
            {
                return std::to_string(value);
            }

            std::string result;
            for (std::size_t i = foundCount; i-- > 0;)
            {
                if (!result.empty())
                {
                    result += ", ";
                }
                result += found[i];
            }
            return result;
        }

        std::string BoolToLower(bool value)
        {
            return value ? "true" : "false";
        }

        std::string FloatToInvariant(float value)
        {
            if (std::isnan(value))
            {
                return "NaN";
            }
            if (std::isinf(value))
            {
                return std::signbit(value) ? "-Infinity" : "Infinity";
            }

            char buffer[64];
            const auto [ptr, error] = std::to_chars(
                std::begin(buffer), std::end(buffer),
                value, std::chars_format::general);
            if (error != std::errc{})
            {
                throw std::runtime_error("Could not format Single.");
            }
            std::string result(buffer, ptr);
            const std::size_t exponent = result.find('e');
            if (exponent != std::string::npos)
            {
                result[exponent] = 'E';
            }
            return result;
        }

        std::string FloatToCustom(float value, int decimals)
        {
            if (std::isnan(value) || std::isinf(value))
            {
                return FloatToInvariant(value);
            }

            char buffer[128];
            const auto [ptr, error] = std::to_chars(
                std::begin(buffer), std::end(buffer),
                value, std::chars_format::fixed, decimals);
            if (error != std::errc{})
            {
                throw std::runtime_error("Could not format Single.");
            }

            std::string result(buffer, ptr);
            const std::size_t dot = result.find('.');
            if (dot != std::string::npos)
            {
                while (!result.empty() && result.back() == '0')
                {
                    result.pop_back();
                }
                if (!result.empty() && result.back() == '.')
                {
                    result.pop_back();
                }
            }
            return result;
        }

        bool IsAsciiUpper(char value) noexcept
        {
            return value >= 'A' && value <= 'Z';
        }

        bool IsAsciiDigit(char value) noexcept
        {
            return value >= '0' && value <= '9';
        }

        void AppendUtf8(std::string& text, std::uint32_t codePoint)
        {
            if (codePoint <= 0x7F)
            {
                text.push_back(static_cast<char>(codePoint));
            }
            else if (codePoint <= 0x7FF)
            {
                text.push_back(static_cast<char>(0xC0 | (codePoint >> 6)));
                text.push_back(static_cast<char>(0x80 | (codePoint & 0x3F)));
            }
            else if (codePoint <= 0xFFFF)
            {
                text.push_back(static_cast<char>(0xE0 | (codePoint >> 12)));
                text.push_back(static_cast<char>(
                    0x80 | ((codePoint >> 6) & 0x3F)));
                text.push_back(static_cast<char>(0x80 | (codePoint & 0x3F)));
            }
            else
            {
                text.push_back(static_cast<char>(0xF0 | (codePoint >> 18)));
                text.push_back(static_cast<char>(
                    0x80 | ((codePoint >> 12) & 0x3F)));
                text.push_back(static_cast<char>(
                    0x80 | ((codePoint >> 6) & 0x3F)));
                text.push_back(static_cast<char>(0x80 | (codePoint & 0x3F)));
            }
        }

        std::string DecodeUtf16(std::string_view bytes, bool bigEndian)
        {
            constexpr std::uint32_t Replacement = 0xFFFD;
            const auto unitAt = [&](std::size_t offset)
            {
                const std::uint16_t first =
                    static_cast<unsigned char>(bytes[offset]);
                const std::uint16_t second =
                    static_cast<unsigned char>(bytes[offset + 1]);
                return static_cast<std::uint16_t>(bigEndian
                    ? (first << 8) | second
                    : first | (second << 8));
            };

            std::string text;
            text.reserve(bytes.size());
            std::size_t offset = 0;
            while (offset + 1 < bytes.size())
            {
                const std::uint16_t first = unitAt(offset);
                offset += 2;
                if (first >= 0xD800 && first <= 0xDBFF)
                {
                    if (offset + 1 < bytes.size())
                    {
                        const std::uint16_t second = unitAt(offset);
                        if (second >= 0xDC00 && second <= 0xDFFF)
                        {
                            offset += 2;
                            const std::uint32_t codePoint = 0x10000
                                + ((static_cast<std::uint32_t>(first) - 0xD800)
                                    << 10)
                                + (static_cast<std::uint32_t>(second) - 0xDC00);
                            AppendUtf8(text, codePoint);
                            continue;
                        }
                    }
                    AppendUtf8(text, Replacement);
                }
                else if (first >= 0xDC00 && first <= 0xDFFF)
                {
                    AppendUtf8(text, Replacement);
                }
                else
                {
                    AppendUtf8(text, first);
                }
            }
            if (offset < bytes.size())
            {
                AppendUtf8(text, Replacement);
            }
            return text;
        }

        std::string DecodeUtf32(std::string_view bytes, bool bigEndian)
        {
            constexpr std::uint32_t Replacement = 0xFFFD;
            std::string text;
            text.reserve(bytes.size());
            std::size_t offset = 0;
            while (offset + 3 < bytes.size())
            {
                const std::uint32_t b0 =
                    static_cast<unsigned char>(bytes[offset]);
                const std::uint32_t b1 =
                    static_cast<unsigned char>(bytes[offset + 1]);
                const std::uint32_t b2 =
                    static_cast<unsigned char>(bytes[offset + 2]);
                const std::uint32_t b3 =
                    static_cast<unsigned char>(bytes[offset + 3]);
                const std::uint32_t codePoint = bigEndian
                    ? (b0 << 24) | (b1 << 16) | (b2 << 8) | b3
                    : b0 | (b1 << 8) | (b2 << 16) | (b3 << 24);
                offset += 4;
                if (codePoint > 0x10FFFF
                    || (codePoint >= 0xD800 && codePoint <= 0xDFFF))
                {
                    AppendUtf8(text, Replacement);
                }
                else
                {
                    AppendUtf8(text, codePoint);
                }
            }
            if (offset < bytes.size())
            {
                AppendUtf8(text, Replacement);
            }
            return text;
        }

        std::string DecodeControlsText(std::string_view bytes)
        {
            const auto byteAt = [&](std::size_t index)
            {
                return static_cast<unsigned char>(bytes[index]);
            };

            if (bytes.size() >= 4
                && byteAt(0) == 0xFF && byteAt(1) == 0xFE
                && byteAt(2) == 0x00 && byteAt(3) == 0x00)
            {
                return DecodeUtf32(bytes.substr(4), false);
            }
            if (bytes.size() >= 4
                && byteAt(0) == 0x00 && byteAt(1) == 0x00
                && byteAt(2) == 0xFE && byteAt(3) == 0xFF)
            {
                return DecodeUtf32(bytes.substr(4), true);
            }
            if (bytes.size() >= 2
                && byteAt(0) == 0xFF && byteAt(1) == 0xFE)
            {
                return DecodeUtf16(bytes.substr(2), false);
            }
            if (bytes.size() >= 2
                && byteAt(0) == 0xFE && byteAt(1) == 0xFF)
            {
                return DecodeUtf16(bytes.substr(2), true);
            }
            if (bytes.size() >= 3
                && byteAt(0) == 0xEF && byteAt(1) == 0xBB
                && byteAt(2) == 0xBF)
            {
                bytes.remove_prefix(3);
            }
            return std::string(bytes);
        }

        std::vector<std::string> ReadAllLines(const std::filesystem::path& path)
        {
            std::ifstream stream(path, std::ios::binary);
            if (!stream)
            {
                throw std::runtime_error("Could not open controls file.");
            }

            const auto bytesBegin = std::istreambuf_iterator<char>(stream);
            const auto bytesEnd = std::istreambuf_iterator<char>();
            const std::string bytes(bytesBegin, bytesEnd);
            if (stream.bad())
            {
                throw std::runtime_error("Could not read controls file.");
            }

            const std::string text = DecodeControlsText(bytes);
            std::vector<std::string> lines;
            std::size_t start = 0;
            while (start < text.size())
            {
                const std::size_t end = text.find_first_of("\r\n", start);
                if (end == std::string::npos)
                {
                    lines.push_back(text.substr(start));
                    break;
                }

                lines.push_back(text.substr(start, end - start));
                if (text[end] == '\r'
                    && end + 1 < text.size() && text[end + 1] == '\n')
                {
                    start = end + 2;
                }
                else
                {
                    start = end + 1;
                }
            }
            return lines;
        }

        void WriteAllLines(const std::filesystem::path& path,
            const std::vector<std::string>& lines)
        {
            std::ofstream stream(path, std::ios::trunc);
            if (!stream)
            {
                throw std::runtime_error("Could not open controls file.");
            }
            for (const std::string& line : lines)
            {
                stream << line << '\n';
                if (!stream)
                {
                    throw std::runtime_error("Could not write controls file.");
                }
            }
            stream.close();
            if (!stream)
            {
                throw std::runtime_error("Could not write controls file.");
            }
        }

        bool FileExists(const std::filesystem::path& path) noexcept
        {
            std::error_code error;
            const bool exists = std::filesystem::is_regular_file(path, error);
            return !error && exists;
        }

        bool IsAndroid() noexcept
        {
#if defined(__ANDROID__)
            return true;
#else
            return false;
#endif
        }
    }

    float InputSettings::_mouseSensitivity = 1.0F;
    bool InputSettings::_invertMouseY = false;
    bool InputSettings::_invertMouseX = false;
    bool InputSettings::_scrollAllWeapons = true;
    InputKey InputSettings::_chatKey = KeyT;
    InputKey InputSettings::_clipKey = KeyF10;
    float InputSettings::_gamepadDeadZone = 0.2F;
    float InputSettings::_gamepadLook = 1.0F;
    bool InputSettings::_gamepadInvertY = false;
    bool InputSettings::_creating = false;
    std::unique_ptr<Entities::PlayerControls> InputSettings::_current{};
    std::optional<std::array<InputBindingProperty, 35>> InputSettings::_bindings{};

    const std::array<std::string_view, 16> InputSettings::_order =
    {
        "MoveUp", "MoveDown", "MoveLeft", "MoveRight",
        "Jump", "Boost", "Shoot", "Zoom",
        "Morph", "AltAttack", "NextWeapon", "PrevWeapon",
        "WeaponMenu", "ScanVisor", "Pause", "HudOverlay"
    };

    float InputSettings::MouseSensitivity() noexcept
    {
        return _mouseSensitivity;
    }

    void InputSettings::MouseSensitivity(float value) noexcept
    {
        _mouseSensitivity = value;
    }

    bool InputSettings::InvertMouseY() noexcept
    {
        return _invertMouseY;
    }

    void InputSettings::InvertMouseY(bool value) noexcept
    {
        _invertMouseY = value;
    }

    bool InputSettings::InvertMouseX() noexcept
    {
        return _invertMouseX;
    }

    void InputSettings::InvertMouseX(bool value) noexcept
    {
        _invertMouseX = value;
    }

    bool InputSettings::ScrollAllWeapons() noexcept
    {
        return _scrollAllWeapons;
    }

    void InputSettings::ScrollAllWeapons(bool value) noexcept
    {
        _scrollAllWeapons = value;
    }

    InputKey InputSettings::ChatKey() noexcept
    {
        return _chatKey;
    }

    void InputSettings::ChatKey(InputKey value) noexcept
    {
        _chatKey = value;
    }

    InputKey InputSettings::ClipKey() noexcept
    {
        return _clipKey;
    }

    void InputSettings::ClipKey(InputKey value)
    {
        if (_clipKey != value)
        {
            Network::DemoClip::Purge();
        }
        _clipKey = value;
    }

    float InputSettings::GamepadDeadZone() noexcept
    {
        return _gamepadDeadZone;
    }

    void InputSettings::GamepadDeadZone(float value) noexcept
    {
        _gamepadDeadZone = MathClamp(value, 0.0F, 0.9F);
    }

    float InputSettings::GamepadLookSensitivity() noexcept
    {
        return _gamepadLook;
    }

    void InputSettings::GamepadLookSensitivity(float value) noexcept
    {
        _gamepadLook = MathClamp(value, 0.1F, 5.0F);
    }

    bool InputSettings::GamepadInvertY() noexcept
    {
        return _gamepadInvertY;
    }

    void InputSettings::GamepadInvertY(bool value) noexcept
    {
        _gamepadInvertY = value;
    }

    Entities::PlayerControls& InputSettings::Current()
    {
        if (_current == nullptr)
        {
            _creating = true;
            auto controls = Entities::PlayerControls::GetDefault();
            _current = std::make_unique<Entities::PlayerControls>(
                std::move(controls));
            _creating = false;
        }
        return *_current;
    }

    const std::array<InputBindingProperty, 35>& InputSettings::Bindings()
    {
        if (!_bindings.has_value())
        {
            _bindings = FindBindings();
        }
        return *_bindings;
    }

    std::array<InputBindingProperty, 35> InputSettings::FindBindings()
    {
        std::array<InputBindingProperty, 35> all =
        {{
            {"MoveLeft", [](Entities::PlayerControls& c) -> Entities::Keybind& { return c.MoveLeft(); }},
            {"MoveRight", [](Entities::PlayerControls& c) -> Entities::Keybind& { return c.MoveRight(); }},
            {"MoveUp", [](Entities::PlayerControls& c) -> Entities::Keybind& { return c.MoveUp(); }},
            {"MoveDown", [](Entities::PlayerControls& c) -> Entities::Keybind& { return c.MoveDown(); }},
            {"RolltLeft", [](Entities::PlayerControls& c) -> Entities::Keybind& { return c.RolltLeft(); }},
            {"RollRight", [](Entities::PlayerControls& c) -> Entities::Keybind& { return c.RollRight(); }},
            {"RollUp", [](Entities::PlayerControls& c) -> Entities::Keybind& { return c.RollUp(); }},
            {"RollDown", [](Entities::PlayerControls& c) -> Entities::Keybind& { return c.RollDown(); }},
            {"AimLeft", [](Entities::PlayerControls& c) -> Entities::Keybind& { return c.AimLeft(); }},
            {"AimRight", [](Entities::PlayerControls& c) -> Entities::Keybind& { return c.AimRight(); }},
            {"AimUp", [](Entities::PlayerControls& c) -> Entities::Keybind& { return c.AimUp(); }},
            {"AimDown", [](Entities::PlayerControls& c) -> Entities::Keybind& { return c.AimDown(); }},
            {"Shoot", [](Entities::PlayerControls& c) -> Entities::Keybind& { return c.Shoot(); }},
            {"Zoom", [](Entities::PlayerControls& c) -> Entities::Keybind& { return c.Zoom(); }},
            {"Jump", [](Entities::PlayerControls& c) -> Entities::Keybind& { return c.Jump(); }},
            {"Morph", [](Entities::PlayerControls& c) -> Entities::Keybind& { return c.Morph(); }},
            {"Boost", [](Entities::PlayerControls& c) -> Entities::Keybind& { return c.Boost(); }},
            {"AltAttack", [](Entities::PlayerControls& c) -> Entities::Keybind& { return c.AltAttack(); }},
            {"ScanVisor", [](Entities::PlayerControls& c) -> Entities::Keybind& { return c.ScanVisor(); }},
            {"Scan", [](Entities::PlayerControls& c) -> Entities::Keybind& { return c.Scan(); }},
            {"NextWeapon", [](Entities::PlayerControls& c) -> Entities::Keybind& { return c.NextWeapon(); }},
            {"PrevWeapon", [](Entities::PlayerControls& c) -> Entities::Keybind& { return c.PrevWeapon(); }},
            {"WeaponMenu", [](Entities::PlayerControls& c) -> Entities::Keybind& { return c.WeaponMenu(); }},
            {"PowerBeam", [](Entities::PlayerControls& c) -> Entities::Keybind& { return c.PowerBeam(); }},
            {"Missile", [](Entities::PlayerControls& c) -> Entities::Keybind& { return c.Missile(); }},
            {"VoltDriver", [](Entities::PlayerControls& c) -> Entities::Keybind& { return c.VoltDriver(); }},
            {"Battlehammer", [](Entities::PlayerControls& c) -> Entities::Keybind& { return c.Battlehammer(); }},
            {"Imperialist", [](Entities::PlayerControls& c) -> Entities::Keybind& { return c.Imperialist(); }},
            {"Judicator", [](Entities::PlayerControls& c) -> Entities::Keybind& { return c.Judicator(); }},
            {"Magmaul", [](Entities::PlayerControls& c) -> Entities::Keybind& { return c.Magmaul(); }},
            {"ShockCoil", [](Entities::PlayerControls& c) -> Entities::Keybind& { return c.ShockCoil(); }},
            {"OmegaCannon", [](Entities::PlayerControls& c) -> Entities::Keybind& { return c.OmegaCannon(); }},
            {"AffinitySlot", [](Entities::PlayerControls& c) -> Entities::Keybind& { return c.AffinitySlot(); }},
            {"Pause", [](Entities::PlayerControls& c) -> Entities::Keybind& { return c.Pause(); }},
            {"HudOverlay", [](Entities::PlayerControls& c) -> Entities::Keybind& { return c.HudOverlay(); }}
        }};

        const auto orderIndex = [](std::string_view name)
        {
            const auto found = std::find(
                _order.begin(), _order.end(), name);
            return found == _order.end()
                ? static_cast<std::ptrdiff_t>(_order.size())
                : std::distance(_order.begin(), found);
        };

        std::stable_sort(all.begin(), all.end(),
            [&](const InputBindingProperty& left,
                const InputBindingProperty& right)
            {
                return orderIndex(left.Name) < orderIndex(right.Name);
            });
        return all;
    }

    Entities::Keybind& InputSettings::Bind(
        const InputBindingProperty& property)
    {
        return property.GetValue(Current());
    }

    std::string InputSettings::Describe(const Entities::Keybind& bind)
    {
        const InputButtonType type = bind.Type();
        if (type == ButtonTypeMouse)
        {
            const InputMouseButton button = bind.MouseButton();
            if (button == MouseLeft)
            {
                return "Mouse left";
            }
            if (button == MouseRight)
            {
                return "Mouse right";
            }
            if (button == MouseMiddle)
            {
                return "Mouse middle";
            }
            return "Mouse " + std::to_string(EnumValue(button) + 1);
        }
        if (type == ButtonTypeScrollUp)
        {
            return "Scroll up";
        }
        if (type == ButtonTypeScrollDown)
        {
            return "Scroll down";
        }
        return bind.Key() == KeyUnknown ? "unbound" : KeyName(bind.Key());
    }

    std::string InputSettings::KeyName(InputKey key)
    {
        const std::string name = KeyToString(key);
        if (name.size() == 2 && name[0] == 'D' && IsAsciiDigit(name[1]))
        {
            return std::string(1, name[1]);
        }

        std::string result;
        result.reserve(name.size() + 4);
        for (std::size_t i = 0; i < name.size(); ++i)
        {
            if (i > 0 && IsAsciiUpper(name[i])
                && !IsAsciiUpper(name[i - 1]))
            {
                result.push_back(' ');
                result.push_back(AsciiLower(name[i]));
            }
            else
            {
                result.push_back(name[i]);
            }
        }
        return result;
    }

    std::string InputSettings::ActionName(
        const InputBindingProperty& property)
    {
        std::string name;
        if (property.Name == "Pause")
        {
            name = "Scoreboard";
        }
        else if (property.Name == "RolltLeft")
        {
            name = "Roll left";
        }
        else
        {
            name = std::string(property.Name);
        }

        std::string result;
        result.reserve(name.size() + 4);
        for (std::size_t i = 0; i < name.size(); ++i)
        {
            if (i > 0 && IsAsciiUpper(name[i])
                && !IsAsciiUpper(name[i - 1]))
            {
                result.push_back(' ');
                result.push_back(AsciiLower(name[i]));
            }
            else if (i == 0 && name[i] >= 'a' && name[i] <= 'z')
            {
                result.push_back(
                    static_cast<char>(name[i] - ('a' - 'A')));
            }
            else
            {
                result.push_back(name[i]);
            }
        }
        return result;
    }

    void InputSettings::Rebind(const InputBindingProperty& property,
        InputButtonType type, InputKey key, InputMouseButton button)
    {
        Entities::Keybind& bind = Bind(property);
        bind.SetType(type);
        bind.SetKey(type == ButtonTypeKey ? key : KeyUnknown);
        bind.SetMouseButton(button);
    }

    void InputSettings::Apply(Entities::PlayerControls& controls)
    {
        if (_creating || _current == nullptr)
        {
            return;
        }

        for (const InputBindingProperty& property : Bindings())
        {
            const Entities::Keybind& source = property.GetValue(*_current);
            Entities::Keybind& target = property.GetValue(controls);
            target.SetType(source.Type());
            target.SetKey(source.Key());
            target.SetMouseButton(source.MouseButton());
        }
        controls.SetScrollAllWeapons(ScrollAllWeapons());
    }

    void InputSettings::ApplyToPlayers()
    {
        try
        {
            const auto& players = Entities::PlayerEntity::Players();
            for (std::size_t i = 0; i < players.size(); ++i)
            {
                const auto& player = players[i];
                if (!player)
                {
                    // C# throws NullReferenceException here and the outer
                    // catch ends the push. Throwing is the mechanical native
                    // equivalent; silently skipping would change ordering.
                    throw std::runtime_error("Player entry was null.");
                }
                Apply(player->Controls());
            }
        }
        catch (...)
        {
        }
    }

    std::filesystem::path InputSettings::Path()
    {
        return std::filesystem::path(Launcher::LauncherPrefs::Directory())
            / "controls.txt";
    }

    void InputSettings::Load()
    {
        const std::filesystem::path path = Path();
        if (!FileExists(path))
        {
            return;
        }

        try
        {
            const std::vector<std::string> lines = ReadAllLines(path);
            for (const std::string& raw : lines)
            {
                const std::string line = TrimCopy(raw);
                const std::size_t split = line.find('=');
                if (line.empty() || line[0] == '#'
                    || split == std::string::npos || split == 0)
                {
                    continue;
                }

                const std::string key =
                    TrimCopy(std::string_view(line).substr(0, split));
                const std::string value =
                    TrimCopy(std::string_view(line).substr(split + 1));

                if (key == "sensitivity")
                {
                    float parsed = 0.0F;
                    if (TryParseSingle(value, parsed))
                    {
                        MouseSensitivity(MathClamp(parsed, 0.05F, 10.0F));
                    }
                    continue;
                }

                bool boolean = false;
                if (key == "invert_y" && TryParseBoolean(value, boolean))
                {
                    InvertMouseY(boolean);
                    continue;
                }
                if (key == "invert_x" && TryParseBoolean(value, boolean))
                {
                    InvertMouseX(boolean);
                    continue;
                }
                if (key == "pointer_jump_guard"
                    && TryParseBoolean(value, boolean))
                {
                    Input::PointerInput::GuardJumps(boolean);
                }
                if (key == "stylus_zone" && TryParseBoolean(value, boolean))
                {
                    Input::StylusZone::Enabled(boolean && !IsAndroid());
                }
                if (key == "stylus_zone_opacity")
                {
                    float opacity = 0.0F;
                    if (TryParseSingle(value, opacity))
                    {
                        Input::StylusZone::Opacity(
                            MathClamp(opacity, 0.02F, 1.0F));
                    }
                }
                if (key == "stylus_zone_rect")
                {
                    std::array<std::string_view, 3> parts{};
                    std::size_t partCount = 0;
                    std::size_t start = 0;
                    while (partCount < parts.size())
                    {
                        const std::size_t comma = value.find(',', start);
                        if (comma == std::string::npos)
                        {
                            parts[partCount++] =
                                std::string_view(value).substr(start);
                            start = std::string::npos;
                            break;
                        }
                        parts[partCount++] =
                            std::string_view(value).substr(start, comma - start);
                        start = comma + 1;
                    }
                    const bool exactlyThree = partCount == 3
                        && start == std::string::npos;
                    float left = 0.0F;
                    float top = 0.0F;
                    float width = 0.0F;
                    if (exactlyThree
                        && TryParseSingle(parts[0], left)
                        && TryParseSingle(parts[1], top)
                        && TryParseSingle(parts[2], width))
                    {
                        Input::StylusZone::SetRect(left, top, width);
                    }
                }
                if (key == "scroll_all_weapons"
                    && TryParseBoolean(value, boolean))
                {
                    ScrollAllWeapons(boolean);
                    continue;
                }
                if (key == "gamepad")
                {
                    continue;
                }
                if (Input::PadBindings::TryLoad(key, value))
                {
                    continue;
                }
                if (key == "clip_key")
                {
                    if (StringEqualsOrdinalIgnoreCase(value, "none"))
                    {
                        _clipKey = KeyUnknown;
                    }
                    else
                    {
                        InputKey parsed = _clipKey;
                        if (TryParseKey(value, parsed))
                        {
                            _clipKey = parsed;
                        }
                    }
                    continue;
                }
                if (key == "clip_seconds")
                {
                    std::int32_t seconds = 0;
                    if (Int32TryParseInvariant(value, seconds))
                    {
                        Network::DemoClip::Seconds(seconds);
                        continue;
                    }
                }
                if (Input::TouchSettings::ReadSetting(key, value))
                {
                    continue;
                }
                if (key == "gamepad_deadzone")
                {
                    float deadZone = 0.0F;
                    if (TryParseSingle(value, deadZone))
                    {
                        GamepadDeadZone(deadZone);
                        continue;
                    }
                }
                if (key == "gamepad_look")
                {
                    float look = 0.0F;
                    if (TryParseSingle(value, look))
                    {
                        GamepadLookSensitivity(look);
                        continue;
                    }
                }
                if (key == "gamepad_invert_y"
                    && TryParseBoolean(value, boolean))
                {
                    GamepadInvertY(boolean);
                    continue;
                }
                if (key == "chat_key")
                {
                    if (StringEqualsOrdinalIgnoreCase(value, "none"))
                    {
                        ChatKey(KeyUnknown);
                    }
                    else
                    {
                        InputKey chatKey = KeyUnknown;
                        if (TryParseKey(value, chatKey))
                        {
                            ChatKey(chatKey);
                        }
                    }
                    continue;
                }

                const auto& bindings = Bindings();
                const auto property = std::find_if(
                    bindings.begin(), bindings.end(),
                    [&](const InputBindingProperty& item)
                    {
                        return item.Name == key;
                    });
                if (property != bindings.end())
                {
                    ParseBind(*property, value);
                }
            }
        }
        catch (...)
        {
        }
    }

    void InputSettings::ParseBind(
        const InputBindingProperty& property, std::string_view value)
    {
        const std::size_t split = value.find(':');
        const std::string type = TrimCopy(
            split == std::string_view::npos
                ? value
                : value.substr(0, split));
        const std::string name = split == std::string_view::npos
            ? std::string()
            : TrimCopy(value.substr(split + 1));

        if (type == "ScrollUp")
        {
            Rebind(property, ButtonTypeScrollUp, KeyUnknown, MouseLeft);
        }
        else if (type == "ScrollDown")
        {
            Rebind(property, ButtonTypeScrollDown, KeyUnknown, MouseLeft);
        }
        else if (type == "Mouse")
        {
            InputMouseButton button = MouseLeft;
            if (TryParseMouseButton(name, button))
            {
                Rebind(property, ButtonTypeMouse, KeyUnknown, button);
            }
        }
        else if (type == "Key")
        {
            InputKey key = KeyUnknown;
            if (TryParseKey(name, key))
            {
                Rebind(property, ButtonTypeKey, key, MouseLeft);
            }
        }
    }

    void InputSettings::Save()
    {
        try
        {
            std::vector<std::string> lines =
            {
                "# " + std::string(Branding::Name)
                    + " controls. Delete a line to go back to the default.",
                "sensitivity=" + FloatToCustom(MouseSensitivity(), 3),
                "invert_y=" + BoolToLower(InvertMouseY()),
                "invert_x=" + BoolToLower(InvertMouseX()),
                "scroll_all_weapons=" + BoolToLower(ScrollAllWeapons()),
                "pointer_jump_guard="
                    + BoolToLower(Input::PointerInput::GuardJumps()),
                "stylus_zone="
                    + BoolToLower(Input::StylusZone::Enabled()),
                "stylus_zone_opacity="
                    + FloatToCustom(Input::StylusZone::Opacity(), 3),
                "stylus_zone_rect="
                    + FloatToCustom(Input::StylusZone::Left(), 4) + ","
                    + FloatToCustom(Input::StylusZone::Top(), 4) + ","
                    + FloatToCustom(Input::StylusZone::Width(), 4),
                "chat_key="
                    + (ChatKey() == KeyUnknown
                        ? std::string("none")
                        : KeyToString(ChatKey())),
                "clip_key="
                    + (ClipKey() == KeyUnknown
                        ? std::string("none")
                        : KeyToString(ClipKey())),
                "clip_seconds="
                    + std::to_string(Network::DemoClip::Seconds()),
                "gamepad_deadzone="
                    + FloatToInvariant(GamepadDeadZone()),
                "gamepad_look="
                    + FloatToInvariant(GamepadLookSensitivity()),
                "gamepad_invert_y="
                    + BoolToLower(GamepadInvertY())
            };

            for (const Input::PadAction action : Input::PadBindings::Actions())
            {
                lines.push_back(Input::PadBindings::SettingKey(action)
                    + "="
                    + GamepadButtonsToString(Input::PadBindings::Get(action)));
            }

            Input::TouchSettings::WriteSettings(lines);

            for (const InputBindingProperty& property : Bindings())
            {
                const Entities::Keybind& bind = Bind(property);
                std::string value;
                if (bind.Type() == ButtonTypeMouse)
                {
                    value = "Mouse:" + MouseButtonToString(bind.MouseButton());
                }
                else if (bind.Type() == ButtonTypeScrollUp)
                {
                    value = "ScrollUp";
                }
                else if (bind.Type() == ButtonTypeScrollDown)
                {
                    value = "ScrollDown";
                }
                else
                {
                    value = "Key:" + KeyToString(bind.Key());
                }
                lines.push_back(std::string(property.Name) + "=" + value);
            }

            WriteAllLines(Path(), lines);
        }
        catch (...)
        {
        }
    }

    void InputSettings::Reset()
    {
        _creating = true;
        auto controls = Entities::PlayerControls::GetDefault();
        _current = std::make_unique<Entities::PlayerControls>(
            std::move(controls));
        _creating = false;
        MouseSensitivity(1.0F);
        InvertMouseY(false);
        InvertMouseX(false);
        ScrollAllWeapons(true);
        ChatKey(KeyT);
        ClipKey(KeyF10);
        Network::DemoClip::Seconds(10);
        Input::PadBindings::Reset();
        Input::TouchSettings::Reset();
        GamepadDeadZone(0.2F);
        GamepadLookSensitivity(1.0F);
        GamepadInvertY(false);
    }
}
