#include "NCSF.hpp"

#include <algorithm>
#include <atomic>
#include <array>
#include <bit>
#include <cassert>
#include <cerrno>
#include <climits>
#include <cstdlib>
#include <cstring>
#include <filesystem>
#include <limits>
#include <memory>
#include <stdexcept>
#include <string>
#include <string_view>
#include <system_error>
#include <utility>
#include <vector>

#include <zlib.h>

#if defined(_WIN32)
#define NOMINMAX
#include <fcntl.h>
#include <io.h>
#include <share.h>
#include <sys/stat.h>
#include <windows.h>
#else
#include <fcntl.h>
#include <iconv.h>
#include <sys/file.h>
#include <unistd.h>
#endif

namespace
{
    using ByteSpan = std::span<const std::uint8_t>;

    struct TextRange final
    {
        std::size_t Start = 0;
        std::size_t End = 0;
    };

    [[nodiscard]] std::int32_t CheckedManagedLength(std::size_t length)
    {
        if (length > static_cast<std::size_t>(std::numeric_limits<std::int32_t>::max()))
        {
            throw std::length_error("Managed span length exceeded Int32.MaxValue.");
        }
        return static_cast<std::int32_t>(length);
    }

    [[nodiscard]] constexpr std::int32_t UInt32ToInt32Unchecked(std::uint32_t value) noexcept
    {
        return std::bit_cast<std::int32_t>(value);
    }

    [[nodiscard]] constexpr std::int32_t ShiftLeftOneUnchecked(std::int32_t value) noexcept
    {
        const std::uint32_t shifted = static_cast<std::uint32_t>(value) << 1U;
        return std::bit_cast<std::int32_t>(shifted);
    }

    [[nodiscard]] constexpr std::int32_t AddInt32Unchecked(std::int32_t left, std::int32_t right) noexcept
    {
        const std::uint32_t sum = static_cast<std::uint32_t>(left) + static_cast<std::uint32_t>(right);
        return std::bit_cast<std::int32_t>(sum);
    }

    [[nodiscard]] ByteSpan Slice(ByteSpan span, std::int32_t start)
    {
        if (start < 0 || static_cast<std::size_t>(start) > span.size())
        {
            throw std::out_of_range("Specified argument was out of the range of valid values.");
        }
        return span.subspan(static_cast<std::size_t>(start));
    }

    [[nodiscard]] ByteSpan Slice(ByteSpan span, std::int32_t start, std::int32_t length)
    {
        if (start < 0 || length < 0)
        {
            throw std::out_of_range("Specified argument was out of the range of valid values.");
        }
        const std::size_t offset = static_cast<std::size_t>(start);
        const std::size_t count = static_cast<std::size_t>(length);
        if (offset > span.size() || count > span.size() - offset)
        {
            throw std::out_of_range("Specified argument was out of the range of valid values.");
        }
        return span.subspan(offset, count);
    }

    [[nodiscard]] std::uint32_t ReadUInt32LittleEndian(ByteSpan span)
    {
        if (span.size() < 4)
        {
            throw std::invalid_argument("Source was too short.");
        }
        return static_cast<std::uint32_t>(span[0])
            | (static_cast<std::uint32_t>(span[1]) << 8U)
            | (static_cast<std::uint32_t>(span[2]) << 16U)
            | (static_cast<std::uint32_t>(span[3]) << 24U);
    }

    void AppendUInt32LittleEndian(std::vector<std::uint8_t>& output, std::uint32_t value)
    {
        output.push_back(static_cast<std::uint8_t>(value));
        output.push_back(static_cast<std::uint8_t>(value >> 8U));
        output.push_back(static_cast<std::uint8_t>(value >> 16U));
        output.push_back(static_cast<std::uint8_t>(value >> 24U));
    }

    [[nodiscard]] std::uint32_t Crc32HashToUInt32(ByteSpan data)
    {
        uLong crc = crc32(0L, Z_NULL, 0);
        if (!data.empty())
        {
            crc = crc32(crc, reinterpret_cast<const Bytef*>(data.data()), static_cast<uInt>(data.size()));
        }
        return static_cast<std::uint32_t>(crc);
    }

    [[nodiscard]] std::vector<std::uint8_t> CompressSmallest(ByteSpan input)
    {
        z_stream stream{};
        const int init = deflateInit(&stream, Z_BEST_COMPRESSION);
        if (init != Z_OK)
        {
            throw std::runtime_error("Unable to initialize zlib compression.");
        }

        struct EndDeflate final
        {
            z_stream* Stream;
            ~EndDeflate() { deflateEnd(Stream); }
        } end{ &stream };

        const uLong bound = deflateBound(&stream, static_cast<uLong>(input.size()));
        if (bound > static_cast<uLong>(std::numeric_limits<std::int32_t>::max()))
        {
            throw std::length_error("Compressed program section exceeded managed array limits.");
        }
        std::vector<std::uint8_t> output(static_cast<std::size_t>(bound));

        stream.next_in = const_cast<Bytef*>(reinterpret_cast<const Bytef*>(input.data()));
        stream.avail_in = static_cast<uInt>(input.size());
        stream.next_out = reinterpret_cast<Bytef*>(output.data());
        stream.avail_out = static_cast<uInt>(output.size());

        const int result = deflate(&stream, Z_FINISH);
        if (result != Z_STREAM_END)
        {
            throw std::runtime_error("zlib compression failed.");
        }
        output.resize(static_cast<std::size_t>(stream.total_out));
        return output;
    }

    [[nodiscard]] std::size_t InflateInitialRead(ByteSpan compressed, std::span<std::uint8_t> output)
    {
        if (output.empty())
        {
            return 0;
        }

        z_stream stream{};
        const int init = inflateInit(&stream);
        if (init != Z_OK)
        {
            throw std::runtime_error("Unable to initialize zlib decompression.");
        }
        struct EndInflate final
        {
            z_stream* Stream;
            ~EndInflate() { inflateEnd(Stream); }
        } end{ &stream };

        stream.next_in = const_cast<Bytef*>(reinterpret_cast<const Bytef*>(compressed.data()));
        stream.avail_in = static_cast<uInt>(compressed.size());
        stream.next_out = reinterpret_cast<Bytef*>(output.data());
        stream.avail_out = static_cast<uInt>(output.size());

        const int result = inflate(&stream, Z_NO_FLUSH);
        if (result != Z_OK && result != Z_STREAM_END && result != Z_BUF_ERROR)
        {
            throw std::runtime_error("The archive entry was compressed using an unsupported compression method.");
        }
        return output.size() - static_cast<std::size_t>(stream.avail_out);
    }

    void InflateExactly(ByteSpan compressed, std::span<std::uint8_t> output)
    {
        if (output.empty())
        {
            return;
        }

        z_stream stream{};
        const int init = inflateInit(&stream);
        if (init != Z_OK)
        {
            throw std::runtime_error("Unable to initialize zlib decompression.");
        }
        struct EndInflate final
        {
            z_stream* Stream;
            ~EndInflate() { inflateEnd(Stream); }
        } end{ &stream };

        stream.next_in = const_cast<Bytef*>(reinterpret_cast<const Bytef*>(compressed.data()));
        stream.avail_in = static_cast<uInt>(compressed.size());
        stream.next_out = reinterpret_cast<Bytef*>(output.data());
        stream.avail_out = static_cast<uInt>(output.size());

        while (stream.avail_out != 0)
        {
            const uInt previousIn = stream.avail_in;
            const uInt previousOut = stream.avail_out;
            const int result = inflate(&stream, Z_NO_FLUSH);
            if (result == Z_STREAM_END)
            {
                if (stream.avail_out != 0)
                {
                    throw std::runtime_error("Unable to read beyond the end of the stream.");
                }
                break;
            }
            if (result != Z_OK)
            {
                throw std::runtime_error("The archive entry was compressed using an unsupported compression method.");
            }
            if (stream.avail_in == previousIn && stream.avail_out == previousOut)
            {
                throw std::runtime_error("Unable to read beyond the end of the stream.");
            }
            if (stream.avail_in == 0 && stream.avail_out != 0)
            {
                throw std::runtime_error("Unable to read beyond the end of the stream.");
            }
        }
    }

    [[nodiscard]] bool IsDotNetWhiteSpace(char16_t value) noexcept
    {
        return (value >= u'\u0009' && value <= u'\u000D')
            || value == u'\u0020'
            || value == u'\u0085'
            || value == u'\u00A0'
            || value == u'\u1680'
            || (value >= u'\u2000' && value <= u'\u200A')
            || value == u'\u2028'
            || value == u'\u2029'
            || value == u'\u202F'
            || value == u'\u205F'
            || value == u'\u3000';
    }

    [[nodiscard]] TextRange TrimRange(std::u16string_view value, std::size_t start, std::size_t end) noexcept
    {
        while (start < end && IsDotNetWhiteSpace(value[start]))
        {
            ++start;
        }
        while (end > start && IsDotNetWhiteSpace(value[end - 1]))
        {
            --end;
        }
        return { start, end };
    }

    [[nodiscard]] std::vector<TextRange> SplitAllRemoveEmptyTrim(
        std::u16string_view value,
        char16_t separator,
        std::size_t rangeStorageCount)
    {
        std::vector<TextRange> ranges;
        ranges.reserve(rangeStorageCount);
        std::size_t start = 0;
        while (true)
        {
            const std::size_t separatorIndex = value.find(separator, start);
            const std::size_t end = separatorIndex == std::u16string_view::npos ? value.size() : separatorIndex;
            const TextRange range = TrimRange(value, start, end);
            if (range.Start != range.End)
            {
                ranges.push_back(range);
            }
            if (separatorIndex == std::u16string_view::npos)
            {
                break;
            }
            start = separatorIndex + 1;
        }
        return ranges;
    }

    [[nodiscard]] std::vector<TextRange> SplitTwoRemoveEmptyTrim(
        std::u16string_view value,
        char16_t separator)
    {
        std::vector<TextRange> ranges;
        ranges.reserve(2);

        std::size_t start = 0;
        while (true)
        {
            const std::size_t separatorIndex = value.find(separator, start);
            if (separatorIndex == std::u16string_view::npos)
            {
                const TextRange remainder = TrimRange(value, start, value.size());
                if (remainder.Start != remainder.End)
                {
                    ranges.push_back(remainder);
                }
                return ranges;
            }

            const TextRange candidate = TrimRange(value, start, separatorIndex);
            if (candidate.Start == candidate.End)
            {
                start = separatorIndex + 1;
                continue;
            }

            ranges.push_back(candidate);
            start = separatorIndex + 1;
            break;
        }

        while (true)
        {
            const std::size_t separatorIndex = value.find(separator, start);
            if (separatorIndex == std::u16string_view::npos)
            {
                break;
            }
            const TextRange candidate = TrimRange(value, start, separatorIndex);
            if (candidate.Start != candidate.End)
            {
                break;
            }
            start = separatorIndex + 1;
        }

        const TextRange remainder = TrimRange(value, start, value.size());
        if (remainder.Start != remainder.End)
        {
            ranges.push_back(remainder);
        }
        return ranges;
    }

    void AppendUtf8Scalar(std::vector<std::uint8_t>& output, std::uint32_t scalar)
    {
        if (scalar <= 0x7FU)
        {
            output.push_back(static_cast<std::uint8_t>(scalar));
        }
        else if (scalar <= 0x7FFU)
        {
            output.push_back(static_cast<std::uint8_t>(0xC0U | (scalar >> 6U)));
            output.push_back(static_cast<std::uint8_t>(0x80U | (scalar & 0x3FU)));
        }
        else if (scalar <= 0xFFFFU)
        {
            output.push_back(static_cast<std::uint8_t>(0xE0U | (scalar >> 12U)));
            output.push_back(static_cast<std::uint8_t>(0x80U | ((scalar >> 6U) & 0x3FU)));
            output.push_back(static_cast<std::uint8_t>(0x80U | (scalar & 0x3FU)));
        }
        else
        {
            output.push_back(static_cast<std::uint8_t>(0xF0U | (scalar >> 18U)));
            output.push_back(static_cast<std::uint8_t>(0x80U | ((scalar >> 12U) & 0x3FU)));
            output.push_back(static_cast<std::uint8_t>(0x80U | ((scalar >> 6U) & 0x3FU)));
            output.push_back(static_cast<std::uint8_t>(0x80U | (scalar & 0x3FU)));
        }
    }

    [[nodiscard]] std::vector<std::uint8_t> EncodeUtf8(std::u16string_view value)
    {
        std::vector<std::uint8_t> output;
        output.reserve(value.size());
        for (std::size_t index = 0; index < value.size(); ++index)
        {
            const std::uint32_t first = value[index];
            std::uint32_t scalar = first;
            if (first >= 0xD800U && first <= 0xDBFFU)
            {
                if (index + 1 < value.size())
                {
                    const std::uint32_t second = value[index + 1];
                    if (second >= 0xDC00U && second <= 0xDFFFU)
                    {
                        scalar = 0x10000U + ((first - 0xD800U) << 10U) + (second - 0xDC00U);
                        ++index;
                    }
                    else
                    {
                        scalar = 0xFFFDU;
                    }
                }
                else
                {
                    scalar = 0xFFFDU;
                }
            }
            else if (first >= 0xDC00U && first <= 0xDFFFU)
            {
                scalar = 0xFFFDU;
            }
            AppendUtf8Scalar(output, scalar);
        }
        return output;
    }

    void AppendUtf16Scalar(std::u16string& output, std::uint32_t scalar)
    {
        if (scalar <= 0xFFFFU)
        {
            output.push_back(static_cast<char16_t>(scalar));
        }
        else
        {
            scalar -= 0x10000U;
            output.push_back(static_cast<char16_t>(0xD800U + (scalar >> 10U)));
            output.push_back(static_cast<char16_t>(0xDC00U + (scalar & 0x3FFU)));
        }
    }

    [[nodiscard]] std::u16string DecodeUtf8(ByteSpan input)
    {
        std::u16string output;
        output.reserve(input.size());

        std::size_t index = 0;
        while (index < input.size())
        {
            const std::uint8_t first = input[index];
            if (first <= 0x7FU)
            {
                output.push_back(static_cast<char16_t>(first));
                ++index;
                continue;
            }

            std::size_t length = 0;
            std::uint32_t scalar = 0;
            std::uint8_t secondMinimum = 0x80U;
            std::uint8_t secondMaximum = 0xBFU;
            if (first >= 0xC2U && first <= 0xDFU)
            {
                length = 2;
                scalar = first & 0x1FU;
            }
            else if (first >= 0xE0U && first <= 0xEFU)
            {
                length = 3;
                scalar = first & 0x0FU;
                if (first == 0xE0U) secondMinimum = 0xA0U;
                if (first == 0xEDU) secondMaximum = 0x9FU;
            }
            else if (first >= 0xF0U && first <= 0xF4U)
            {
                length = 4;
                scalar = first & 0x07U;
                if (first == 0xF0U) secondMinimum = 0x90U;
                if (first == 0xF4U) secondMaximum = 0x8FU;
            }
            else
            {
                output.push_back(u'\uFFFD');
                ++index;
                continue;
            }

            std::size_t consumed = 1;
            bool valid = true;
            for (std::size_t offset = 1; offset < length; ++offset)
            {
                if (index + offset >= input.size())
                {
                    valid = false;
                    break;
                }
                const std::uint8_t next = input[index + offset];
                const std::uint8_t minimum = offset == 1 ? secondMinimum : 0x80U;
                const std::uint8_t maximum = offset == 1 ? secondMaximum : 0xBFU;
                if (next < minimum || next > maximum)
                {
                    valid = false;
                    break;
                }
                scalar = (scalar << 6U) | (next & 0x3FU);
                ++consumed;
            }

            if (!valid)
            {
                output.push_back(u'\uFFFD');
                index += consumed;
                continue;
            }

            AppendUtf16Scalar(output, scalar);
            index += length;
        }
        return output;
    }

#if defined(_WIN32)
    [[nodiscard]] std::u16string DecodeCodePage(ByteSpan input, std::uint32_t codePage)
    {
        if (input.empty())
        {
            return {};
        }
        if (input.size() > static_cast<std::size_t>(std::numeric_limits<int>::max()))
        {
            throw std::length_error("Input exceeded native encoding limits.");
        }
        const int inputLength = static_cast<int>(input.size());
        const int required = MultiByteToWideChar(
            codePage,
            0,
            reinterpret_cast<const char*>(input.data()),
            inputLength,
            nullptr,
            0);
        if (required <= 0)
        {
            throw std::runtime_error("Unable to decode the configured system code page.");
        }
        std::wstring wide(static_cast<std::size_t>(required), L'\0');
        if (MultiByteToWideChar(
                codePage,
                0,
                reinterpret_cast<const char*>(input.data()),
                inputLength,
                wide.data(),
                required) != required)
        {
            throw std::runtime_error("Unable to decode the configured system code page.");
        }
        std::u16string result;
        result.reserve(wide.size());
        for (wchar_t character : wide)
        {
            result.push_back(static_cast<char16_t>(character));
        }
        return result;
    }
#else
    [[nodiscard]] std::string CodePageName(std::uint32_t codePage)
    {
        return "CP" + std::to_string(codePage);
    }

    [[nodiscard]] std::u16string DecodeCodePage(ByteSpan input, std::uint32_t codePage)
    {
        if (input.empty())
        {
            return {};
        }

        const std::string sourceName = CodePageName(codePage);
        iconv_t converter = iconv_open("UTF-16LE", sourceName.c_str());
        if (converter == reinterpret_cast<iconv_t>(-1))
        {
            throw std::runtime_error("Unable to open the configured system code page.");
        }
        struct CloseIconv final
        {
            iconv_t Converter;
            ~CloseIconv() { iconv_close(Converter); }
        } close{ converter };

        std::vector<char> bytes((input.size() + 1) * 4);
        char* inputPointer = reinterpret_cast<char*>(const_cast<std::uint8_t*>(input.data()));
        std::size_t inputRemaining = input.size();
        char* outputPointer = bytes.data();
        std::size_t outputRemaining = bytes.size();

        while (inputRemaining != 0)
        {
            const std::size_t result = iconv(
                converter,
                &inputPointer,
                &inputRemaining,
                &outputPointer,
                &outputRemaining);
            if (result != static_cast<std::size_t>(-1))
            {
                continue;
            }
            if (errno == E2BIG)
            {
                const std::size_t used = static_cast<std::size_t>(outputPointer - bytes.data());
                bytes.resize(bytes.size() * 2);
                outputPointer = bytes.data() + used;
                outputRemaining = bytes.size() - used;
                continue;
            }
            if (errno == EILSEQ || errno == EINVAL)
            {
                if (outputRemaining < 2)
                {
                    const std::size_t used = static_cast<std::size_t>(outputPointer - bytes.data());
                    bytes.resize(bytes.size() * 2);
                    outputPointer = bytes.data() + used;
                    outputRemaining = bytes.size() - used;
                }
                *outputPointer++ = static_cast<char>(0x3F);
                *outputPointer++ = static_cast<char>(0x00);
                outputRemaining -= 2;
                ++inputPointer;
                --inputRemaining;
                iconv(converter, nullptr, nullptr, nullptr, nullptr);
                continue;
            }
            throw std::runtime_error("Unable to decode the configured system code page.");
        }

        const std::size_t used = static_cast<std::size_t>(outputPointer - bytes.data());
        std::u16string output;
        output.resize(used / 2);
        for (std::size_t index = 0; index < output.size(); ++index)
        {
            const auto low = static_cast<std::uint8_t>(bytes[index * 2]);
            const auto high = static_cast<std::uint8_t>(bytes[index * 2 + 1]);
            output[index] = static_cast<char16_t>(
                static_cast<std::uint16_t>(low)
                | (static_cast<std::uint16_t>(high) << 8U));
        }
        return output;
    }
#endif

    [[nodiscard]] std::uint32_t CultureAnsiCodePage()
    {
#if defined(_WIN32)
        wchar_t localeName[LOCALE_NAME_MAX_LENGTH]{};
        if (LCIDToLocaleName(GetThreadLocale(), localeName, LOCALE_NAME_MAX_LENGTH, 0) != 0)
        {
            wchar_t codePage[16]{};
            if (GetLocaleInfoEx(
                    localeName,
                    LOCALE_IDEFAULTANSICODEPAGE,
                    codePage,
                    static_cast<int>(std::size(codePage))) > 1)
            {
                wchar_t* end = nullptr;
                const unsigned long value = std::wcstoul(codePage, &end, 10);
                if (end != codePage && value != 0 && value <= std::numeric_limits<std::uint32_t>::max())
                {
                    return static_cast<std::uint32_t>(value);
                }
            }
        }
        return 1252U;
#else
        const char* localeValue = std::getenv("LC_ALL");
        if (localeValue == nullptr || *localeValue == '\0')
        {
            localeValue = std::getenv("LC_CTYPE");
        }
        if (localeValue == nullptr || *localeValue == '\0')
        {
            localeValue = std::getenv("LANG");
        }
        std::string locale = localeValue == nullptr ? std::string{} : std::string(localeValue);
        std::transform(locale.begin(), locale.end(), locale.begin(), [](unsigned char c)
        {
            return static_cast<char>(c >= 'A' && c <= 'Z' ? c - 'A' + 'a' : c);
        });

        const auto begins = [&locale](std::string_view prefix)
        {
            return locale.rfind(prefix, 0) == 0;
        };
        if (begins("ja")) return 932U;
        if (begins("zh_cn") || begins("zh-cn") || begins("zh_sg") || begins("zh-sg")) return 936U;
        if (begins("zh")) return 950U;
        if (begins("ko")) return 949U;
        if (begins("th")) return 874U;
        if (begins("vi")) return 1258U;
        if (begins("el")) return 1253U;
        if (begins("tr") || begins("az")) return 1254U;
        if (begins("he")) return 1255U;
        if (begins("ar") || begins("fa") || begins("ur")) return 1256U;
        if (begins("et") || begins("lv") || begins("lt")) return 1257U;
        if (begins("ru") || begins("uk") || begins("be") || begins("bg") || begins("mk") || begins("sr")) return 1251U;
        if (begins("pl") || begins("cs") || begins("sk") || begins("hu") || begins("sl")
            || begins("hr") || begins("ro") || begins("sq") || begins("bs")) return 1250U;
        return 1252U;
#endif
    }

    class FileWriter final
    {
    public:
        explicit FileWriter(std::u16string_view filename)
        {
            if (filename.find(u'\0') != std::u16string_view::npos)
            {
                throw std::invalid_argument("Null character in path.");
            }
#if defined(_WIN32)
            const std::wstring path(
                reinterpret_cast<const wchar_t*>(filename.data()),
                filename.size());
            int descriptor = -1;
            const errno_t error = _wsopen_s(
                &descriptor,
                path.c_str(),
                _O_BINARY | _O_CREAT | _O_TRUNC | _O_WRONLY,
                _SH_DENYRW,
                _S_IREAD | _S_IWRITE);
            if (error != 0 || descriptor < 0)
            {
                throw std::system_error(static_cast<int>(error), std::generic_category(), "Unable to create output file");
            }
            _descriptor = descriptor;
#else
            const std::filesystem::path path{ std::u16string(filename) };
            const int descriptor = ::open(path.c_str(), O_CREAT | O_WRONLY | O_CLOEXEC, 0666);
            if (descriptor < 0)
            {
                throw std::system_error(errno, std::generic_category(), "Unable to create output file");
            }

            bool locked = false;
            if (::flock(descriptor, LOCK_EX | LOCK_NB) == 0)
            {
                locked = true;
            }
            else
            {
                const int lockError = errno;
                if (lockError == EWOULDBLOCK || lockError == EAGAIN)
                {
                    ::close(descriptor);
                    throw std::system_error(lockError, std::generic_category(), "Unable to create output file");
                }
            }

            if (::ftruncate(descriptor, 0) < 0)
            {
                const int truncateError = errno;
                if (truncateError != EBADF && truncateError != EINVAL)
                {
                    if (locked)
                    {
                        static_cast<void>(::flock(descriptor, LOCK_UN));
                    }
                    ::close(descriptor);
                    throw std::system_error(truncateError, std::generic_category(), "Unable to create output file");
                }
            }

            _descriptor = descriptor;
            _isLocked = locked;
#endif
        }

        FileWriter(const FileWriter&) = delete;
        FileWriter& operator=(const FileWriter&) = delete;

        ~FileWriter()
        {
            if (_descriptor >= 0)
            {
#if defined(_WIN32)
                _close(_descriptor);
#else
                if (_isLocked)
                {
                    static_cast<void>(::flock(_descriptor, LOCK_UN));
                    _isLocked = false;
                }
                ::close(_descriptor);
#endif
            }
        }

        void Write(ByteSpan data)
        {
            std::size_t offset = 0;
            while (offset < data.size())
            {
#if defined(_WIN32)
                const std::size_t remaining = data.size() - offset;
                const unsigned int request = static_cast<unsigned int>(std::min<std::size_t>(remaining, INT_MAX));
                const int written = _write(_descriptor, data.data() + offset, request);
                if (written < 0)
                {
                    throw std::system_error(errno, std::generic_category(), "Unable to write output file");
                }
                if (written == 0)
                {
                    throw std::runtime_error("Unable to write output file.");
                }
                offset += static_cast<std::size_t>(written);
#else
                const ssize_t written = ::write(_descriptor, data.data() + offset, data.size() - offset);
                if (written < 0)
                {
                    if (errno == EINTR)
                    {
                        continue;
                    }
                    throw std::system_error(errno, std::generic_category(), "Unable to write output file");
                }
                if (written == 0)
                {
                    throw std::runtime_error("Unable to write output file.");
                }
                offset += static_cast<std::size_t>(written);
#endif
            }
        }

    private:
        int _descriptor = -1;
        bool _isLocked = false;
    };
}

namespace NCSFCommon
{
    const std::uint32_t NCSF::SystemCodePageEncoding = CultureAnsiCodePage();

    void NCSF::MakeNCSF(
        std::u16string_view filename,
        std::span<const std::uint8_t> reservedSectionData,
        std::span<const std::uint8_t> programSectionData,
        const TagList* tags)
    {
        const std::int32_t reservedLength = CheckedManagedLength(reservedSectionData.size());
        static_cast<void>(CheckedManagedLength(programSectionData.size()));

        std::vector<std::uint8_t> compressedStorage;
        ByteSpan programCompressedData;
        if (!programSectionData.empty())
        {
            compressedStorage = CompressSmallest(programSectionData);
            programCompressedData = compressedStorage;
        }
        const std::int32_t compressedLength = CheckedManagedLength(programCompressedData.size());

        FileWriter file(filename);

        std::vector<std::uint8_t> header;
        header.reserve(16);
        header.insert(header.end(), PSFHeader.begin(), PSFHeader.end());
        header.push_back(0x25U);
        AppendUInt32LittleEndian(header, static_cast<std::uint32_t>(reservedLength));
        AppendUInt32LittleEndian(header, static_cast<std::uint32_t>(compressedLength));
        AppendUInt32LittleEndian(
            header,
            programCompressedData.empty() ? 0U : Crc32HashToUInt32(programCompressedData));
        file.Write(header);
        file.Write(reservedSectionData);
        file.Write(programCompressedData);

        if (tags != nullptr && tags->Count() != 0)
        {
            file.Write(TAGHeader);

            std::int32_t maximumNewlines = 0;
            for (const TagList::Item& tag : *tags)
            {
                const auto count = static_cast<std::int32_t>(std::count(tag.Value.begin(), tag.Value.end(), u'\n'));
                maximumNewlines = std::max(maximumNewlines, count);
            }
            const std::int32_t rangeStorageCount = AddInt32Unchecked(maximumNewlines, 1);
            if (rangeStorageCount <= 0)
            {
                throw std::length_error("Stack allocation length was invalid.");
            }

            for (const TagList::Item& tag : *tags)
            {
                const std::u16string_view valueSpan(tag.Value);
                const std::vector<TextRange> ranges = SplitAllRemoveEmptyTrim(
                    valueSpan,
                    u'\n',
                    static_cast<std::size_t>(rangeStorageCount));
                for (const TextRange& range : ranges)
                {
                    std::u16string line = tag.Name;
                    line.push_back(u'=');
                    line.append(valueSpan.substr(range.Start, range.End - range.Start));
                    const std::vector<std::uint8_t> encoded = EncodeUtf8(line);
                    file.Write(encoded);
                    static constexpr std::array<std::uint8_t, 1> Newline{ 0x0AU };
                    file.Write(Newline);
                }
            }
        }
    }

    void NCSF::CheckForValidPSF(std::span<const std::uint8_t> span, std::uint8_t versionByte)
    {
#ifndef NDEBUG
        const std::int32_t fileSize = CheckedManagedLength(span.size());
        assert(fileSize >= 4);
        assert(std::equal(span.begin(), span.begin() + 3, PSFHeader.begin(), PSFHeader.end()));
        assert(span[3] == versionByte);
        assert(fileSize >= 0x10);
#else
        static_cast<void>(CheckedManagedLength(span.size()));
        static_cast<void>(versionByte);
#endif

        const std::uint32_t reservedSize = ReadUInt32LittleEndian(Slice(span, 0x04));
        const std::uint32_t programCompressedSize = ReadUInt32LittleEndian(Slice(span, 0x08));

#ifndef NDEBUG
        const std::uint32_t reservedEnd = reservedSize + 0x10U;
        assert(reservedSize == 0U || static_cast<std::int64_t>(fileSize) >= static_cast<std::int64_t>(reservedEnd));

        const std::uint32_t programEnd = reservedSize + programCompressedSize + 0x10U;
        assert(programCompressedSize == 0U || static_cast<std::int64_t>(fileSize) >= static_cast<std::int64_t>(programEnd));
#else
        static_cast<void>(reservedSize);
        static_cast<void>(programCompressedSize);
#endif
    }

    std::vector<std::uint8_t> NCSF::GetProgramSectionFromPSF(
        std::span<const std::uint8_t> span,
        std::uint8_t versionByte,
        std::uint32_t programHeaderSize,
        std::uint32_t programSectionOffset,
        bool addHeaderSize)
    {
        CheckForValidPSF(span, versionByte);

        const std::uint32_t reservedSize = ReadUInt32LittleEndian(Slice(span, 0x04));
        const std::uint32_t programCompressedSize = ReadUInt32LittleEndian(Slice(span, 0x08));
        if (programCompressedSize == 0U)
        {
            return {};
        }

        const std::int32_t compressedLength = UInt32ToInt32Unchecked(programCompressedSize);
        if (compressedLength < 0)
        {
            throw std::out_of_range("Capacity must be non-negative.");
        }
        std::vector<std::uint8_t> compressed(static_cast<std::size_t>(compressedLength));

        const std::int32_t headerLength = UInt32ToInt32Unchecked(programHeaderSize);
        if (headerLength < 0)
        {
            throw std::length_error("Stack allocation length was invalid.");
        }
        std::unique_ptr<std::uint8_t[]> initialStorage;
        if (headerLength != 0)
        {
            initialStorage = std::unique_ptr<std::uint8_t[]>(new std::uint8_t[static_cast<std::size_t>(headerLength)]);
        }
        std::span<std::uint8_t> initialBytes(initialStorage.get(), static_cast<std::size_t>(headerLength));

        const std::uint32_t compressedStartUnsigned = reservedSize + 0x10U;
        const std::int32_t compressedStart = UInt32ToInt32Unchecked(compressedStartUnsigned);
        const ByteSpan compressedSource = Slice(span, compressedStart, compressedLength);
        std::copy(compressedSource.begin(), compressedSource.end(), compressed.begin());

        static_cast<void>(InflateInitialRead(compressed, initialBytes));

        const std::int32_t sectionOffset = UInt32ToInt32Unchecked(programSectionOffset);
        if (sectionOffset < 0 || static_cast<std::size_t>(sectionOffset) > initialBytes.size())
        {
            throw std::out_of_range("Specified argument was out of the range of valid values.");
        }
        const ByteSpan initialReadOnly(initialBytes.data(), initialBytes.size());
        const std::uint32_t uncompressedSectionSize = ReadUInt32LittleEndian(
            Slice(initialReadOnly, sectionOffset));
        const std::uint32_t size = uncompressedSectionSize + (addHeaderSize ? programHeaderSize : 0U);
        if (size > static_cast<std::uint32_t>(std::numeric_limits<std::int32_t>::max()))
        {
            throw std::length_error("Array dimensions exceeded supported range.");
        }

        std::vector<std::uint8_t> bytes(static_cast<std::size_t>(size));
        InflateExactly(compressed, bytes);
        return bytes;
    }

    std::optional<std::int32_t> NCSF::FindOffsetsInFile(
        std::span<const std::uint8_t> search,
        std::span<const std::uint8_t> memory)
    {
        const std::int32_t m = CheckedManagedLength(search.size());
        const std::int32_t n = CheckedManagedLength(memory.size());
        std::array<std::int32_t, 256> B{};
        std::int32_t s = 1;
        if (m == 0)
        {
            static std::atomic<std::uint32_t> emptyPatternProgress{ 0U };
            for (;;)
            {
                static_cast<void>(emptyPatternProgress.fetch_add(0U, std::memory_order_relaxed));
            }
        }
        for (std::int32_t i = m - 1; i >= 0; --i)
        {
            B[search[static_cast<std::size_t>(i)]] |= s;
            s = ShiftLeftOneUnchecked(s);
        }

        std::int32_t j = 0;
        while (j <= n - m)
        {
            std::int32_t i = m - 1;
            std::int32_t last = m;
            std::int32_t d = ~0;
            while (i >= 0 && d != 0)
            {
                const std::int32_t memoryIndex = AddInt32Unchecked(j, i);
                if (memoryIndex < 0 || memoryIndex >= n)
                {
                    throw std::out_of_range("Index was outside the bounds of the array.");
                }
                d &= B[memory[static_cast<std::size_t>(memoryIndex)]];
                --i;
                if (d != 0)
                {
                    if (i >= 0)
                    {
                        last = i + 1;
                    }
                    else
                    {
                        return j;
                    }
                }
                d = ShiftLeftOneUnchecked(d);
            }
            j = AddInt32Unchecked(j, last);
        }
        return std::nullopt;
    }

    TagList NCSF::GetTagsFromPSFWithEncoding(
        std::span<const std::uint8_t> span,
        EncodingKind encoding)
    {
        TagList tags;
        const std::u16string rawTags = encoding == EncodingKind::Utf8
            ? DecodeUtf8(span)
            : DecodeCodePage(span, SystemCodePageEncoding);

        const auto newlineCount = static_cast<std::size_t>(std::count(rawTags.begin(), rawTags.end(), u'\n'));
        const std::vector<TextRange> tagPairRanges = SplitAllRemoveEmptyTrim(
            rawTags,
            u'\n',
            newlineCount + 1U);
        for (const TextRange& pairRange : tagPairRanges)
        {
            const std::u16string_view tag(rawTags.data() + pairRange.Start, pairRange.End - pairRange.Start);
            const std::vector<TextRange> tagRanges = SplitTwoRemoveEmptyTrim(tag, u'=');
            if (tagRanges.size() == 2)
            {
                const std::u16string nameStr(tag.substr(
                    tagRanges[0].Start,
                    tagRanges[0].End - tagRanges[0].Start));
                const std::u16string valueStr(tag.substr(
                    tagRanges[1].Start,
                    tagRanges[1].End - tagRanges[1].Start));

                TagList::Item existingTag;
                std::u16string finalValue = valueStr;
                if (tags.TryGetValue(nameStr, existingTag))
                {
                    finalValue = existingTag.Value;
                    finalValue.push_back(u'\n');
                    finalValue.append(valueStr);
                }
                tags.AddOrReplace({ nameStr, std::move(finalValue) });
            }
        }
        return tags;
    }

    TagList NCSF::GetTagsFromPSF(
        std::span<const std::uint8_t> memory,
        std::uint8_t versionByte)
    {
        CheckForValidPSF(memory, versionByte);

        const std::optional<std::int32_t> found = FindOffsetsInFile(TAGHeader, memory);
        const std::int32_t tagOffset = found.value_or(-1);
        if (tagOffset != -1)
        {
            const std::int32_t tagStart = AddInt32Unchecked(
                tagOffset,
                static_cast<std::int32_t>(TAGHeader.size()));
            const ByteSpan tagsSpan = Slice(memory, tagStart);
            TagList tags = GetTagsFromPSFWithEncoding(tagsSpan, EncodingKind::SystemCodePage);
            if (tags.Contains(u"utf8"))
            {
                tags = GetTagsFromPSFWithEncoding(tagsSpan, EncodingKind::Utf8);
            }
            return tags;
        }
        return {};
    }

    std::int16_t NCSF::ConvertScale(std::int32_t scale)
    {
        if ((scale & 0x80) != 0)
        {
            scale = 0x7F;
        }
        if (scale < 0 || scale >= static_cast<std::int32_t>(convertScaleLookupTable.size()))
        {
            throw std::out_of_range("Index was outside the bounds of the array.");
        }
        return convertScaleLookupTable[static_cast<std::size_t>(scale)];
    }
}
