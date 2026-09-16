#include "AndroidConsole.hpp"

#if !defined(__ANDROID__)
#error "AndroidConsole is only valid for the Android native target."
#endif

#include <android/log.h>
#include <iostream>
#include <memory>
#include <string>
#include <utility>

namespace
{
    void AppendModifiedUtf8(std::string& output, char16_t value)
    {
        const std::uint16_t codeUnit = static_cast<std::uint16_t>(value);
        if (codeUnit == 0)
        {
            output.push_back(static_cast<char>(0xC0));
            output.push_back(static_cast<char>(0x80));
        }
        else if (codeUnit <= 0x7F)
        {
            output.push_back(static_cast<char>(codeUnit));
        }
        else if (codeUnit <= 0x7FF)
        {
            output.push_back(static_cast<char>(0xC0U | (codeUnit >> 6)));
            output.push_back(static_cast<char>(0x80U | (codeUnit & 0x3FU)));
        }
        else
        {
            output.push_back(static_cast<char>(0xE0U | (codeUnit >> 12)));
            output.push_back(static_cast<char>(0x80U | ((codeUnit >> 6) & 0x3FU)));
            output.push_back(static_cast<char>(0x80U | (codeUnit & 0x3FU)));
        }
    }

    std::string ToModifiedUtf8(std::u16string_view value)
    {
        std::string output;
        output.reserve(value.size());
        for (char16_t codeUnit : value)
        {
            AppendModifiedUtf8(output, codeUnit);
        }
        return output;
    }
}

namespace MphRead::Droid
{
    std::string_view AndroidConsole::Encoding() const noexcept
    {
        return "UTF-8";
    }

    void AndroidConsole::Install() noexcept
    {
        try
        {
            auto writer = std::make_unique<AndroidConsole>();
            bool outInstalled = false;
            try
            {
                std::cout.rdbuf(writer.get());
                outInstalled = true;

                std::cerr.tie(nullptr);
                std::cerr.unsetf(std::ios_base::unitbuf);
                std::cerr.rdbuf(writer.get());

                writer.release();
            }
            catch (...)
            {
                if (outInstalled)
                {
                    writer.release();
                }
                throw;
            }
        }
        catch (...)
        {
        }
    }

    void AndroidConsole::Write(char16_t value)
    {
        if (value == u'\n')
        {
            Flush();
            return;
        }
        if (value != u'\r')
        {
            _line.push_back(value);
        }
    }

    void AndroidConsole::Write(const std::optional<std::u16string_view>& value)
    {
        if (!value.has_value())
        {
            return;
        }
        for (char16_t character : *value)
        {
            Write(character);
        }
    }

    void AndroidConsole::WriteLine(const std::optional<std::u16string_view>& value)
    {
        Write(value);
        Flush();
    }

    void AndroidConsole::Flush()
    {
        FinishIncompleteStreamCharacter();
        if (_line.empty())
        {
            return;
        }
        const std::string message = ToModifiedUtf8(_line);
        __android_log_write(ANDROID_LOG_INFO, Tag.data(), message.c_str());
        _line.clear();
    }

    void AndroidConsole::AppendStreamCodePoint(std::uint32_t value)
    {
        if (value <= 0xFFFFU)
        {
            _line.push_back(static_cast<char16_t>(value));
            return;
        }
        value -= 0x10000U;
        _line.push_back(static_cast<char16_t>(0xD800U + (value >> 10)));
        _line.push_back(static_cast<char16_t>(0xDC00U + (value & 0x3FFU)));
    }

    void AndroidConsole::FinishIncompleteStreamCharacter()
    {
        if (_streamRemaining != 0)
        {
            _line.push_back(u'\uFFFD');
            _streamCodePoint = 0;
            _streamMinimum = 0;
            _streamRemaining = 0;
        }
    }

    void AndroidConsole::WriteStreamByte(unsigned char value)
    {
        for (;;)
        {
            if (_streamRemaining != 0)
            {
                if ((value & 0xC0U) == 0x80U)
                {
                    _streamCodePoint = (_streamCodePoint << 6) | (value & 0x3FU);
                    --_streamRemaining;
                    if (_streamRemaining == 0)
                    {
                        const std::uint32_t codePoint = _streamCodePoint;
                        const std::uint32_t minimum = _streamMinimum;
                        _streamCodePoint = 0;
                        _streamMinimum = 0;
                        if (codePoint < minimum || codePoint > 0x10FFFFU
                            || (codePoint >= 0xD800U && codePoint <= 0xDFFFU))
                        {
                            _line.push_back(u'\uFFFD');
                        }
                        else
                        {
                            AppendStreamCodePoint(codePoint);
                        }
                    }
                    return;
                }

                FinishIncompleteStreamCharacter();
                continue;
            }

            if (value == static_cast<unsigned char>('\n'))
            {
                Flush();
                return;
            }
            if (value == static_cast<unsigned char>('\r'))
            {
                return;
            }
            if (value <= 0x7FU)
            {
                _line.push_back(static_cast<char16_t>(value));
                return;
            }
            if ((value & 0xE0U) == 0xC0U)
            {
                _streamCodePoint = value & 0x1FU;
                _streamMinimum = 0x80U;
                _streamRemaining = 1;
                return;
            }
            if ((value & 0xF0U) == 0xE0U)
            {
                _streamCodePoint = value & 0x0FU;
                _streamMinimum = 0x800U;
                _streamRemaining = 2;
                return;
            }
            if ((value & 0xF8U) == 0xF0U)
            {
                _streamCodePoint = value & 0x07U;
                _streamMinimum = 0x10000U;
                _streamRemaining = 3;
                return;
            }

            _line.push_back(u'\uFFFD');
            return;
        }
    }

    AndroidConsole::int_type AndroidConsole::overflow(int_type value)
    {
        if (traits_type::eq_int_type(value, traits_type::eof()))
        {
            return traits_type::not_eof(value);
        }
        WriteStreamByte(static_cast<unsigned char>(traits_type::to_char_type(value)));
        return value;
    }

    std::streamsize AndroidConsole::xsputn(const char* data, std::streamsize count)
    {
        if (count <= 0)
        {
            return 0;
        }
        for (std::streamsize index = 0; index < count; ++index)
        {
            WriteStreamByte(static_cast<unsigned char>(data[index]));
        }
        return count;
    }

    int AndroidConsole::sync()
    {
        Flush();
        return 0;
    }
}
