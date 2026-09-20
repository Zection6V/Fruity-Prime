#include "AndroidConsole.hpp"

#if !defined(__ANDROID__)
#error "AndroidConsole is only valid for the Android native target."
#endif

#include <android/log.h>

#include <cstdint>
#include <iostream>
#include <memory>
#include <streambuf>
#include <string>
#include <string_view>

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

    class ConsoleStreamBuffer final : public std::streambuf
    {
    public:
        explicit ConsoleStreamBuffer(MphRead::Droid::AndroidConsole& writer) noexcept
            : _writer(writer)
        {
        }

    private:
        MphRead::Droid::AndroidConsole& _writer;
        std::uint32_t _codePoint = 0;
        std::uint32_t _minimum = 0;
        unsigned int _remaining = 0;

        void AppendCodePoint(std::uint32_t value)
        {
            if (value <= 0xFFFFU)
            {
                _writer.Write(static_cast<char16_t>(value));
                return;
            }

            value -= 0x10000U;
            _writer.Write(static_cast<char16_t>(0xD800U + (value >> 10)));
            _writer.Write(static_cast<char16_t>(0xDC00U + (value & 0x3FFU)));
        }

        void FinishIncompleteCharacter()
        {
            if (_remaining != 0)
            {
                _writer.Write(u'\uFFFD');
                _codePoint = 0;
                _minimum = 0;
                _remaining = 0;
            }
        }

        void WriteByte(unsigned char value)
        {
            for (;;)
            {
                if (_remaining != 0)
                {
                    if ((value & 0xC0U) == 0x80U)
                    {
                        _codePoint = (_codePoint << 6) | (value & 0x3FU);
                        --_remaining;
                        if (_remaining == 0)
                        {
                            const std::uint32_t codePoint = _codePoint;
                            const std::uint32_t minimum = _minimum;
                            _codePoint = 0;
                            _minimum = 0;
                            if (codePoint < minimum || codePoint > 0x10FFFFU
                                || (codePoint >= 0xD800U && codePoint <= 0xDFFFU))
                            {
                                _writer.Write(u'\uFFFD');
                            }
                            else
                            {
                                AppendCodePoint(codePoint);
                            }
                        }
                        return;
                    }

                    FinishIncompleteCharacter();
                    continue;
                }

                if (value <= 0x7FU)
                {
                    _writer.Write(static_cast<char16_t>(value));
                    return;
                }
                if ((value & 0xE0U) == 0xC0U)
                {
                    _codePoint = value & 0x1FU;
                    _minimum = 0x80U;
                    _remaining = 1;
                    return;
                }
                if ((value & 0xF0U) == 0xE0U)
                {
                    _codePoint = value & 0x0FU;
                    _minimum = 0x800U;
                    _remaining = 2;
                    return;
                }
                if ((value & 0xF8U) == 0xF0U)
                {
                    _codePoint = value & 0x07U;
                    _minimum = 0x10000U;
                    _remaining = 3;
                    return;
                }

                _writer.Write(u'\uFFFD');
                return;
            }
        }

        int_type overflow(int_type value) override
        {
            if (traits_type::eq_int_type(value, traits_type::eof()))
            {
                return traits_type::not_eof(value);
            }
            WriteByte(static_cast<unsigned char>(traits_type::to_char_type(value)));
            return value;
        }

        std::streamsize xsputn(const char* data, std::streamsize count) override
        {
            if (count <= 0)
            {
                return 0;
            }
            for (std::streamsize index = 0; index < count; ++index)
            {
                WriteByte(static_cast<unsigned char>(data[index]));
            }
            return count;
        }

        int sync() override
        {
            FinishIncompleteCharacter();
            _writer.Flush();
            return 0;
        }
    };

    struct InstalledConsole final
    {
        MphRead::Droid::AndroidConsole Writer;
        ConsoleStreamBuffer Buffer;

        InstalledConsole()
            : Buffer(Writer)
        {
        }
    };
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
            auto installed = std::make_unique<InstalledConsole>();
            bool outInstalled = false;
            try
            {
                std::cout.rdbuf(&installed->Buffer);
                outInstalled = true;

                std::cerr.rdbuf(&installed->Buffer);
                std::cerr.tie(nullptr);
                std::cerr.unsetf(std::ios_base::unitbuf);

                installed.release();
            }
            catch (...)
            {
                if (outInstalled)
                {
                    installed.release();
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
        if (_line.empty())
        {
            return;
        }

        const std::string message = ToModifiedUtf8(_line);
        __android_log_write(ANDROID_LOG_INFO, Tag.data(), message.c_str());
        _line.clear();
    }
}
