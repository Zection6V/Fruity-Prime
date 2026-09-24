#include "Encoding.hpp"

#include <cstddef>
#include <cstdint>

namespace MphRead::NativeRuntime
{
    std::string Utf16ToUtf8(std::u16string_view value)
    {
        std::string result;
        result.reserve(value.size());
        for (std::size_t index = 0; index < value.size(); ++index)
        {
            std::uint32_t codePoint = static_cast<std::uint16_t>(value[index]);
            if (codePoint >= 0xD800U && codePoint <= 0xDBFFU)
            {
                if (index + 1 < value.size())
                {
                    const std::uint32_t low = static_cast<std::uint16_t>(value[index + 1]);
                    if (low >= 0xDC00U && low <= 0xDFFFU)
                    {
                        codePoint = 0x10000U + ((codePoint - 0xD800U) << 10)
                            + (low - 0xDC00U);
                        ++index;
                    }
                    else
                    {
                        codePoint = 0xFFFDU;
                    }
                }
                else
                {
                    codePoint = 0xFFFDU;
                }
            }
            else if (codePoint >= 0xDC00U && codePoint <= 0xDFFFU)
            {
                codePoint = 0xFFFDU;
            }

            if (codePoint <= 0x7FU)
            {
                result.push_back(static_cast<char>(codePoint));
            }
            else if (codePoint <= 0x7FFU)
            {
                result.push_back(static_cast<char>(0xC0U | (codePoint >> 6)));
                result.push_back(static_cast<char>(0x80U | (codePoint & 0x3FU)));
            }
            else if (codePoint <= 0xFFFFU)
            {
                result.push_back(static_cast<char>(0xE0U | (codePoint >> 12)));
                result.push_back(static_cast<char>(0x80U | ((codePoint >> 6) & 0x3FU)));
                result.push_back(static_cast<char>(0x80U | (codePoint & 0x3FU)));
            }
            else
            {
                result.push_back(static_cast<char>(0xF0U | (codePoint >> 18)));
                result.push_back(static_cast<char>(0x80U | ((codePoint >> 12) & 0x3FU)));
                result.push_back(static_cast<char>(0x80U | ((codePoint >> 6) & 0x3FU)));
                result.push_back(static_cast<char>(0x80U | (codePoint & 0x3FU)));
            }
        }
        return result;
    }

    std::u16string Utf8ToUtf16(std::string_view value)
    {
        std::u16string result;
        result.reserve(value.size());
        std::size_t index = 0;
        while (index < value.size())
        {
            const std::uint8_t first = static_cast<std::uint8_t>(value[index]);
            std::uint32_t codePoint = 0;
            std::size_t length = 0;
            std::uint32_t minimum = 0;
            if (first <= 0x7FU)
            {
                codePoint = first;
                length = 1;
            }
            else if ((first & 0xE0U) == 0xC0U)
            {
                codePoint = first & 0x1FU;
                length = 2;
                minimum = 0x80U;
            }
            else if ((first & 0xF0U) == 0xE0U)
            {
                codePoint = first & 0x0FU;
                length = 3;
                minimum = 0x800U;
            }
            else if ((first & 0xF8U) == 0xF0U)
            {
                codePoint = first & 0x07U;
                length = 4;
                minimum = 0x10000U;
            }
            else
            {
                result.push_back(u'\uFFFD');
                ++index;
                continue;
            }

            if (index + length > value.size())
            {
                result.push_back(u'\uFFFD');
                ++index;
                continue;
            }

            bool valid = true;
            for (std::size_t offset = 1; offset < length; ++offset)
            {
                const std::uint8_t next = static_cast<std::uint8_t>(value[index + offset]);
                if ((next & 0xC0U) != 0x80U)
                {
                    valid = false;
                    break;
                }
                codePoint = (codePoint << 6) | (next & 0x3FU);
            }
            if (!valid || codePoint < minimum || codePoint > 0x10FFFFU
                || (codePoint >= 0xD800U && codePoint <= 0xDFFFU))
            {
                result.push_back(u'\uFFFD');
                ++index;
                continue;
            }

            if (codePoint <= 0xFFFFU)
            {
                result.push_back(static_cast<char16_t>(codePoint));
            }
            else
            {
                codePoint -= 0x10000U;
                result.push_back(static_cast<char16_t>(0xD800U + (codePoint >> 10)));
                result.push_back(static_cast<char16_t>(0xDC00U + (codePoint & 0x3FFU)));
            }
            index += length;
        }
        return result;
    }
}
