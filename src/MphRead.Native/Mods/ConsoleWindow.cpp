#include "ConsoleWindow.hpp"

#include <cstddef>
#include <cstdint>
#include <string_view>

#if defined(_WIN32)
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <algorithm>
#include <array>
#include <atomic>
#include <ios>
#include <iostream>
#include <memory>
#include <mutex>
#include <optional>
#include <stdexcept>
#include <streambuf>
#include <string>
#include <utility>
#include <vector>
#include <windows.h>
#endif

namespace
{
    [[nodiscard]] bool EqualsOrdinalIgnoreCaseAscii(
        std::string_view value, std::string_view asciiValue) noexcept
    {
        if (value.size() != asciiValue.size())
        {
            return false;
        }

        for (std::size_t index = 0; index < value.size(); ++index)
        {
            const auto left = static_cast<unsigned char>(value[index]);
            const auto right = static_cast<unsigned char>(asciiValue[index]);
            if (left > 0x7FU || right > 0x7FU)
            {
                return false;
            }

            if (left == right)
            {
                continue;
            }

            const unsigned char foldedLeft
                = left >= 'A' && left <= 'Z'
                ? static_cast<unsigned char>(left + ('a' - 'A'))
                : left;
            const unsigned char foldedRight
                = right >= 'A' && right <= 'Z'
                ? static_cast<unsigned char>(right + ('a' - 'A'))
                : right;
            if (foldedLeft != foldedRight
                || foldedLeft < static_cast<unsigned char>('a')
                || foldedLeft > static_cast<unsigned char>('z'))
            {
                return false;
            }
        }
        return true;
    }

#if defined(_WIN32)
    constexpr int StdInputHandle = -10;
    constexpr int StdOutputHandle = -11;
    constexpr int StdErrorHandle = -12;
    constexpr DWORD EnableVirtualTerminalProcessing = 0x0004U;
    constexpr std::size_t StreamWriterBufferSize = 1024;
    constexpr std::size_t StreamReaderBufferSize = 1024;

    [[noreturn]] void ThrowIoFailure(const char* operation, DWORD error)
    {
        throw std::ios_base::failure(
            std::string(operation) + " failed with Win32 error " + std::to_string(error));
    }

    [[nodiscard]] HANDLE GetStdHandleRaw(int handle) noexcept
    {
        return ::GetStdHandle(static_cast<DWORD>(handle));
    }

    [[nodiscard]] HANDLE GetStdHandleWithLastError(int handle) noexcept
    {
        // DllImport(SetLastError = true) clears the native last-error value
        // immediately before the unmanaged call on current .NET runtimes.
        ::SetLastError(ERROR_SUCCESS);
        return ::GetStdHandle(static_cast<DWORD>(handle));
    }

    [[nodiscard]] BOOL AttachConsoleWithLastError(int processId) noexcept
    {
        ::SetLastError(ERROR_SUCCESS);
        return ::AttachConsole(static_cast<DWORD>(processId));
    }

    [[nodiscard]] BOOL AllocConsoleWithLastError() noexcept
    {
        ::SetLastError(ERROR_SUCCESS);
        return ::AllocConsole();
    }

    [[nodiscard]] bool IsHandleRedirectedCore(int stdHandle) noexcept
    {
        const HANDLE handle = GetStdHandleRaw(stdHandle);

        // ConsolePal's GetFileType/GetConsoleMode interop declarations both
        // use SetLastError=true.
        ::SetLastError(ERROR_SUCCESS);
        const DWORD fileType = ::GetFileType(handle);
        if ((fileType & FILE_TYPE_CHAR) != FILE_TYPE_CHAR)
        {
            return true;
        }

        DWORD mode = 0;
        ::SetLastError(ERROR_SUCCESS);
        return ::GetConsoleMode(handle, &mode) == FALSE;
    }

    struct RedirectionCache final
    {
        // -1 = uninitialized, 0 = false, 1 = true. Console's .NET implementation
        // caches each redirected property independently after its first query.
        std::atomic<int> Input{-1};
        std::atomic<int> Output{-1};
        std::atomic<int> Error{-1};
    };

    RedirectionCache& RedirectedState() noexcept
    {
        static RedirectionCache state;
        return state;
    }

    [[nodiscard]] bool GetCachedRedirected(std::atomic<int>& slot, int stdHandle) noexcept
    {
        int value = slot.load(std::memory_order_acquire);
        if (value >= 0)
        {
            return value != 0;
        }

        const bool redirected = IsHandleRedirectedCore(stdHandle);
        slot.store(redirected ? 1 : 0, std::memory_order_release);

        // Match the managed property reading the cache after initialization;
        // a racing first query is allowed to replace the cached box.
        value = slot.load(std::memory_order_acquire);
        return value != 0;
    }

    [[nodiscard]] bool IsOutputRedirected() noexcept
    {
        return GetCachedRedirected(RedirectedState().Output, StdOutputHandle);
    }

    [[nodiscard]] bool IsInputRedirected() noexcept
    {
        return GetCachedRedirected(RedirectedState().Input, StdInputHandle);
    }

    [[nodiscard]] bool IsErrorRedirected() noexcept
    {
        return GetCachedRedirected(RedirectedState().Error, StdErrorHandle);
    }

    struct EncodingCache final
    {
        std::mutex Mutex;
        std::optional<UINT> InputCodePage;
        std::optional<UINT> OutputCodePage;
    };

    EncodingCache& EncodingState()
    {
        static EncodingCache state;
        return state;
    }

    [[nodiscard]] UINT InputEncodingCodePage()
    {
        EncodingCache& state = EncodingState();
        std::lock_guard<std::mutex> lock(state.Mutex);
        if (!state.InputCodePage.has_value())
        {
            state.InputCodePage = ::GetConsoleCP();
        }
        return *state.InputCodePage;
    }

    [[nodiscard]] UINT OutputEncodingCodePage()
    {
        EncodingCache& state = EncodingState();
        std::lock_guard<std::mutex> lock(state.Mutex);
        if (!state.OutputCodePage.has_value())
        {
            state.OutputCodePage = ::GetConsoleOutputCP();
        }
        return *state.OutputCodePage;
    }

    struct StandardFile final
    {
        HANDLE Handle = nullptr;
        bool IsNull = true;
        bool UseFileApis = true;
        bool IsPipe = false;
    };

    [[nodiscard]] StandardFile OpenStandardOutputLikeDotNet(int stdHandle)
    {
        // Console.OpenStandardOutput/Error evaluates OutputEncoding first and
        // only evaluates the corresponding redirected property when the code
        // page is UTF-16 (1200). Preserve that short-circuit ordering.
        bool useFileApis = OutputEncodingCodePage() != 1200U;
        if (!useFileApis)
        {
            useFileApis = stdHandle == StdOutputHandle
                ? IsOutputRedirected()
                : IsErrorRedirected();
        }

        const HANDLE handle = GetStdHandleRaw(stdHandle);
        if (handle == nullptr || handle == INVALID_HANDLE_VALUE)
        {
            return StandardFile{};
        }

        // ConsolePal verifies writable stdout/stderr with a zero-byte WriteFile.
        unsigned char junkByte = 0x41U;
        DWORD bytesWritten = 0;
        ::SetLastError(ERROR_SUCCESS);
        if (::WriteFile(handle, &junkByte, 0, &bytesWritten, nullptr) == FALSE)
        {
            return StandardFile{};
        }

        ::SetLastError(ERROR_SUCCESS);
        const bool isPipe = ::GetFileType(handle) == FILE_TYPE_PIPE;
        return StandardFile{handle, false, useFileApis, isPipe};
    }

    [[nodiscard]] StandardFile OpenStandardInputLikeDotNet()
    {
        // Same short-circuit ordering as Console.OpenStandardInput().
        bool useFileApis = InputEncodingCodePage() != 1200U;
        if (!useFileApis)
        {
            useFileApis = IsInputRedirected();
        }

        const HANDLE handle = GetStdHandleRaw(StdInputHandle);
        if (handle == nullptr || handle == INVALID_HANDLE_VALUE)
        {
            return StandardFile{};
        }

        ::SetLastError(ERROR_SUCCESS);
        const bool isPipe = ::GetFileType(handle) == FILE_TYPE_PIPE;
        return StandardFile{handle, false, useFileApis, isPipe};
    }

    void AppendUtf8(std::string& output, std::uint32_t codePoint)
    {
        if (codePoint <= 0x7FU)
        {
            output.push_back(static_cast<char>(codePoint));
        }
        else if (codePoint <= 0x7FFU)
        {
            output.push_back(static_cast<char>(0xC0U | (codePoint >> 6)));
            output.push_back(static_cast<char>(0x80U | (codePoint & 0x3FU)));
        }
        else if (codePoint <= 0xFFFFU)
        {
            output.push_back(static_cast<char>(0xE0U | (codePoint >> 12)));
            output.push_back(static_cast<char>(0x80U | ((codePoint >> 6) & 0x3FU)));
            output.push_back(static_cast<char>(0x80U | (codePoint & 0x3FU)));
        }
        else
        {
            output.push_back(static_cast<char>(0xF0U | (codePoint >> 18)));
            output.push_back(static_cast<char>(0x80U | ((codePoint >> 12) & 0x3FU)));
            output.push_back(static_cast<char>(0x80U | ((codePoint >> 6) & 0x3FU)));
            output.push_back(static_cast<char>(0x80U | (codePoint & 0x3FU)));
        }
    }

    void AppendReplacement(std::string& output)
    {
        AppendUtf8(output, 0xFFFDU);
    }

    class DotNetStreamWriterBuffer final : public std::streambuf
    {
    public:
        explicit DotNetStreamWriterBuffer(StandardFile file)
            : _file(file), _chars(StreamWriterBufferSize)
        {
            // new StreamWriter(stream) { AutoFlush = true } immediately calls
            // Flush(flushStream: true, flushEncoder: false). The console stream's
            // Flush has no native flushing side effect, so there is nothing else
            // to perform here.
        }

    protected:
        std::streamsize xsputn(const char* source, std::streamsize count) override
        {
            if (count <= 0)
            {
                return 0;
            }

            std::lock_guard<std::mutex> lock(_mutex);
            FeedNativeUtf8(source, static_cast<std::size_t>(count));
            FlushCharacters(false);
            return count;
        }

        int_type overflow(int_type value) override
        {
            if (traits_type::eq_int_type(value, traits_type::eof()))
            {
                return traits_type::not_eof(value);
            }

            const char byte = traits_type::to_char_type(value);
            std::lock_guard<std::mutex> lock(_mutex);
            FeedNativeUtf8(&byte, 1);
            FlushCharacters(false);
            return value;
        }

        int sync() override
        {
            std::lock_guard<std::mutex> lock(_mutex);
            if (!_nativeUtf8Pending.empty())
            {
                throw std::runtime_error("Invalid UTF-8 sequence in console output.");
            }
            FlushCharacters(true);
            return 0;
        }

    private:
        StandardFile _file;
        std::vector<char16_t> _chars;
        std::size_t _charCount = 0;
        std::optional<char16_t> _encoderHighSurrogate;
        std::string _nativeUtf8Pending;
        std::mutex _mutex;

        void PushUtf16(char16_t value)
        {
            if (_charCount == _chars.size())
            {
                FlushCharacters(false);
            }
            _chars[_charCount++] = value;
        }

        void PushCodePoint(std::uint32_t value)
        {
            if (value <= 0xFFFFU)
            {
                PushUtf16(static_cast<char16_t>(value));
                return;
            }

            value -= 0x10000U;
            PushUtf16(static_cast<char16_t>(0xD800U + (value >> 10)));
            PushUtf16(static_cast<char16_t>(0xDC00U + (value & 0x3FFU)));
        }

        void FeedNativeUtf8(const char* source, std::size_t count)
        {
            _nativeUtf8Pending.append(source, count);
            std::size_t index = 0;
            while (index < _nativeUtf8Pending.size())
            {
                const auto first = static_cast<unsigned char>(_nativeUtf8Pending[index]);
                std::size_t length = 0;
                std::uint32_t value = 0;
                std::uint32_t minimum = 0;

                if (first <= 0x7FU)
                {
                    length = 1;
                    value = first;
                }
                else if ((first & 0xE0U) == 0xC0U)
                {
                    length = 2;
                    value = first & 0x1FU;
                    minimum = 0x80U;
                }
                else if ((first & 0xF0U) == 0xE0U)
                {
                    length = 3;
                    value = first & 0x0FU;
                    minimum = 0x800U;
                }
                else if ((first & 0xF8U) == 0xF0U)
                {
                    length = 4;
                    value = first & 0x07U;
                    minimum = 0x10000U;
                }
                else
                {
                    throw std::runtime_error("Invalid UTF-8 sequence in console output.");
                }

                if (_nativeUtf8Pending.size() - index < length)
                {
                    break;
                }

                for (std::size_t offset = 1; offset < length; ++offset)
                {
                    const auto next = static_cast<unsigned char>(_nativeUtf8Pending[index + offset]);
                    if ((next & 0xC0U) != 0x80U)
                    {
                        throw std::runtime_error("Invalid UTF-8 sequence in console output.");
                    }
                    value = (value << 6) | (next & 0x3FU);
                }

                // Allow WTF-8 surrogate code units so the Native UTF-8 string
                // representation can express every UTF-16 C# string. The strict
                // StreamWriter encoder below rejects unpaired surrogates exactly
                // when it is asked to encode them.
                if (value < minimum || value > 0x10FFFFU)
                {
                    throw std::runtime_error("Invalid UTF-8 sequence in console output.");
                }

                PushCodePoint(value);
                index += length;
            }

            _nativeUtf8Pending.erase(0, index);
        }

        void WriteUnderlying(const char* bytes, std::size_t count)
        {
            if (_file.IsNull || count == 0)
            {
                return;
            }

            BOOL success = FALSE;
            if (_file.UseFileApis)
            {
                DWORD bytesWritten = 0;
                ::SetLastError(ERROR_SUCCESS);
                success = ::WriteFile(
                    _file.Handle,
                    bytes,
                    static_cast<DWORD>(count),
                    &bytesWritten,
                    nullptr);
            }
            else
            {
                DWORD charsWritten = 0;
                ::SetLastError(ERROR_SUCCESS);
                success = ::WriteConsoleW(
                    _file.Handle,
                    bytes,
                    static_cast<DWORD>(count / 2U),
                    &charsWritten,
                    nullptr);
            }

            if (success != FALSE)
            {
                return;
            }

            const DWORD error = ::GetLastError();
            if (error == ERROR_NO_DATA
                || error == ERROR_BROKEN_PIPE
                || error == ERROR_PIPE_NOT_CONNECTED)
            {
                return;
            }
            ThrowIoFailure("console write", error);
        }

        void FlushCharacters(bool flushEncoder)
        {
            std::string encoded;
            encoded.reserve(_charCount * 3U + 4U);

            std::size_t index = 0;
            if (_encoderHighSurrogate.has_value())
            {
                if (_charCount == 0)
                {
                    if (flushEncoder)
                    {
                        throw std::runtime_error("Invalid UTF-16 surrogate in console output.");
                    }
                    return;
                }

                const char16_t low = _chars[0];
                if (low < 0xDC00U || low > 0xDFFFU)
                {
                    throw std::runtime_error("Invalid UTF-16 surrogate in console output.");
                }
                const std::uint32_t codePoint = 0x10000U
                    + ((static_cast<std::uint32_t>(*_encoderHighSurrogate) - 0xD800U) << 10)
                    + (static_cast<std::uint32_t>(low) - 0xDC00U);
                AppendUtf8(encoded, codePoint);
                _encoderHighSurrogate.reset();
                index = 1;
            }

            while (index < _charCount)
            {
                const char16_t first = _chars[index++];
                if (first >= 0xD800U && first <= 0xDBFFU)
                {
                    if (index == _charCount)
                    {
                        if (flushEncoder)
                        {
                            throw std::runtime_error("Invalid UTF-16 surrogate in console output.");
                        }
                        _encoderHighSurrogate = first;
                        break;
                    }

                    const char16_t second = _chars[index++];
                    if (second < 0xDC00U || second > 0xDFFFU)
                    {
                        throw std::runtime_error("Invalid UTF-16 surrogate in console output.");
                    }
                    const std::uint32_t codePoint = 0x10000U
                        + ((static_cast<std::uint32_t>(first) - 0xD800U) << 10)
                        + (static_cast<std::uint32_t>(second) - 0xDC00U);
                    AppendUtf8(encoded, codePoint);
                }
                else if (first >= 0xDC00U && first <= 0xDFFFU)
                {
                    throw std::runtime_error("Invalid UTF-16 surrogate in console output.");
                }
                else
                {
                    AppendUtf8(encoded, first);
                }
            }

            _charCount = 0;
            WriteUnderlying(encoded.data(), encoded.size());
        }
    };

    class DotNetStreamReaderBuffer final : public std::streambuf
    {
    public:
        explicit DotNetStreamReaderBuffer(StandardFile file)
            : _file(file), _raw(StreamReaderBufferSize)
        {
        }

    protected:
        int_type underflow() override
        {
            std::lock_guard<std::mutex> lock(_mutex);
            if (gptr() != nullptr && gptr() < egptr())
            {
                return traits_type::to_int_type(*gptr());
            }

            _decoded.clear();
            while (_decoded.empty() && !_eof)
            {
                FillDecoded();
            }

            if (_decoded.empty())
            {
                return traits_type::eof();
            }

            char* first = _decoded.data();
            setg(first, first, first + static_cast<std::ptrdiff_t>(_decoded.size()));
            return traits_type::to_int_type(*gptr());
        }

    private:
        enum class Encoding
        {
            Utf8,
            Utf16LittleEndian,
            Utf16BigEndian,
            Utf32LittleEndian,
            Utf32BigEndian
        };

        StandardFile _file;
        std::vector<char> _raw;
        std::string _preambleBuffer;
        std::size_t _preamblePosition = 0;
        bool _checkPreamble = true;
        bool _detectEncoding = true;
        std::string _decoderPending;
        std::string _decoded;
        Encoding _encoding = Encoding::Utf8;
        bool _eof = false;
        std::optional<std::uint16_t> _pendingHighSurrogate;
        std::mutex _mutex;

        [[nodiscard]] std::size_t ReadUnderlying()
        {
            if (_file.IsNull)
            {
                return 0;
            }

            if (_file.UseFileApis)
            {
                DWORD bytesRead = 0;
                ::SetLastError(ERROR_SUCCESS);
                if (::ReadFile(
                        _file.Handle,
                        _raw.data(),
                        static_cast<DWORD>(_raw.size()),
                        &bytesRead,
                        nullptr) != FALSE)
                {
                    return static_cast<std::size_t>(bytesRead);
                }

                const DWORD error = ::GetLastError();
                if (error == ERROR_NO_DATA || error == ERROR_BROKEN_PIPE)
                {
                    return 0;
                }
                ThrowIoFailure("console read", error);
            }

            std::array<std::uint16_t, StreamReaderBufferSize / 2U> wide{};
            DWORD charsRead = 0;
            ::SetLastError(ERROR_SUCCESS);
            if (::ReadConsoleW(
                    _file.Handle,
                    wide.data(),
                    static_cast<DWORD>(wide.size()),
                    &charsRead,
                    nullptr) == FALSE)
            {
                ThrowIoFailure("console read", ::GetLastError());
            }

            const std::size_t bytesRead = static_cast<std::size_t>(charsRead) * 2U;
            if (bytesRead > _raw.size())
            {
                throw std::runtime_error("ReadConsoleW returned an invalid character count.");
            }
            const auto* bytes = reinterpret_cast<const char*>(wide.data());
            std::copy(bytes, bytes + static_cast<std::ptrdiff_t>(bytesRead), _raw.data());
            return bytesRead;
        }

        void ResetDecoderForEncoding(Encoding encoding)
        {
            _encoding = encoding;
            _decoderPending.clear();
            _pendingHighSurrogate.reset();
        }

        void DetectEncoding(std::string& chunk)
        {
            if (!_detectEncoding || chunk.size() < 2)
            {
                return;
            }

            _detectEncoding = false;
            const auto byte = [&](std::size_t index) -> unsigned char
            {
                return static_cast<unsigned char>(chunk[index]);
            };
            const std::uint16_t firstTwo = static_cast<std::uint16_t>(
                byte(0) | (static_cast<std::uint16_t>(byte(1)) << 8));

            if (firstTwo == 0xFFFEU)
            {
                ResetDecoderForEncoding(Encoding::Utf16BigEndian);
                chunk.erase(0, 2);
            }
            else if (firstTwo == 0xFEFFU)
            {
                if (chunk.size() < 4 || byte(2) != 0 || byte(3) != 0)
                {
                    ResetDecoderForEncoding(Encoding::Utf16LittleEndian);
                    chunk.erase(0, 2);
                }
                else
                {
                    ResetDecoderForEncoding(Encoding::Utf32LittleEndian);
                    chunk.erase(0, 4);
                }
            }
            else if (chunk.size() >= 3
                && firstTwo == 0xBBEFU && byte(2) == 0xBFU)
            {
                ResetDecoderForEncoding(Encoding::Utf8);
                chunk.erase(0, 3);
            }
            else if (chunk.size() >= 4
                && firstTwo == 0
                && byte(2) == 0xFEU && byte(3) == 0xFFU)
            {
                ResetDecoderForEncoding(Encoding::Utf32BigEndian);
                chunk.erase(0, 4);
            }
            else if (chunk.size() == 2)
            {
                // StreamReader deliberately retries BOM detection after a
                // two-byte non-match on the next buffer fill.
                _detectEncoding = true;
            }
        }

        void DecodeUtf8(bool flush)
        {
            std::size_t index = 0;
            while (index < _decoderPending.size())
            {
                const auto first = static_cast<unsigned char>(_decoderPending[index]);
                if (first <= 0x7FU)
                {
                    _decoded.push_back(static_cast<char>(first));
                    ++index;
                    continue;
                }

                std::size_t length = 0;
                std::uint32_t value = 0;
                std::uint32_t minimum = 0;
                if (first >= 0xC2U && first <= 0xDFU)
                {
                    length = 2;
                    value = first & 0x1FU;
                    minimum = 0x80U;
                }
                else if (first >= 0xE0U && first <= 0xEFU)
                {
                    length = 3;
                    value = first & 0x0FU;
                    minimum = 0x800U;
                }
                else if (first >= 0xF0U && first <= 0xF4U)
                {
                    length = 4;
                    value = first & 0x07U;
                    minimum = 0x10000U;
                }
                else
                {
                    AppendReplacement(_decoded);
                    ++index;
                    continue;
                }

                if (_decoderPending.size() - index < length)
                {
                    if (!flush)
                    {
                        break;
                    }
                    AppendReplacement(_decoded);
                    index = _decoderPending.size();
                    break;
                }

                std::size_t validPrefix = 1;
                bool invalid = false;
                for (std::size_t offset = 1; offset < length; ++offset)
                {
                    const auto next = static_cast<unsigned char>(_decoderPending[index + offset]);
                    if ((next & 0xC0U) != 0x80U)
                    {
                        invalid = true;
                        break;
                    }
                    if (offset == 1
                        && ((first == 0xE0U && next < 0xA0U)
                            || (first == 0xEDU && next >= 0xA0U)
                            || (first == 0xF0U && next < 0x90U)
                            || (first == 0xF4U && next > 0x8FU)))
                    {
                        invalid = true;
                        break;
                    }
                    value = (value << 6) | (next & 0x3FU);
                    ++validPrefix;
                }

                if (invalid || value < minimum || value > 0x10FFFFU
                    || (value >= 0xD800U && value <= 0xDFFFU))
                {
                    AppendReplacement(_decoded);
                    index += validPrefix;
                    continue;
                }

                AppendUtf8(_decoded, value);
                index += length;
            }
            _decoderPending.erase(0, index);
        }

        void DecodeUtf16(bool bigEndian, bool flush)
        {
            std::size_t index = 0;
            auto readUnit = [&](std::size_t offset) -> std::uint16_t
            {
                const auto first = static_cast<unsigned char>(_decoderPending[offset]);
                const auto second = static_cast<unsigned char>(_decoderPending[offset + 1]);
                return bigEndian
                    ? static_cast<std::uint16_t>((first << 8) | second)
                    : static_cast<std::uint16_t>(first | (second << 8));
            };

            if (_pendingHighSurrogate.has_value())
            {
                if (_decoderPending.size() < 2)
                {
                    if (flush)
                    {
                        AppendReplacement(_decoded);
                        _pendingHighSurrogate.reset();
                        if (!_decoderPending.empty())
                        {
                            AppendReplacement(_decoded);
                            _decoderPending.clear();
                        }
                    }
                    return;
                }

                const std::uint16_t low = readUnit(0);
                if (low >= 0xDC00U && low <= 0xDFFFU)
                {
                    const std::uint32_t codePoint = 0x10000U
                        + ((static_cast<std::uint32_t>(*_pendingHighSurrogate) - 0xD800U) << 10)
                        + (static_cast<std::uint32_t>(low) - 0xDC00U);
                    AppendUtf8(_decoded, codePoint);
                    index = 2;
                }
                else
                {
                    AppendReplacement(_decoded);
                }
                _pendingHighSurrogate.reset();
            }

            while (index + 1 < _decoderPending.size())
            {
                const std::uint16_t first = readUnit(index);
                index += 2;
                if (first >= 0xD800U && first <= 0xDBFFU)
                {
                    if (index + 1 >= _decoderPending.size())
                    {
                        if (flush)
                        {
                            AppendReplacement(_decoded);
                        }
                        else
                        {
                            _pendingHighSurrogate = first;
                        }
                        break;
                    }

                    const std::uint16_t second = readUnit(index);
                    if (second >= 0xDC00U && second <= 0xDFFFU)
                    {
                        index += 2;
                        const std::uint32_t codePoint = 0x10000U
                            + ((static_cast<std::uint32_t>(first) - 0xD800U) << 10)
                            + (static_cast<std::uint32_t>(second) - 0xDC00U);
                        AppendUtf8(_decoded, codePoint);
                    }
                    else
                    {
                        AppendReplacement(_decoded);
                    }
                }
                else if (first >= 0xDC00U && first <= 0xDFFFU)
                {
                    AppendReplacement(_decoded);
                }
                else
                {
                    AppendUtf8(_decoded, first);
                }
            }

            _decoderPending.erase(0, index);
            if (flush && !_decoderPending.empty())
            {
                AppendReplacement(_decoded);
                _decoderPending.clear();
            }
        }

        void DecodeUtf32(bool bigEndian, bool flush)
        {
            std::size_t index = 0;
            while (index + 3 < _decoderPending.size())
            {
                const auto b0 = static_cast<unsigned char>(_decoderPending[index]);
                const auto b1 = static_cast<unsigned char>(_decoderPending[index + 1]);
                const auto b2 = static_cast<unsigned char>(_decoderPending[index + 2]);
                const auto b3 = static_cast<unsigned char>(_decoderPending[index + 3]);
                index += 4;

                const std::uint32_t value = bigEndian
                    ? (static_cast<std::uint32_t>(b0) << 24)
                        | (static_cast<std::uint32_t>(b1) << 16)
                        | (static_cast<std::uint32_t>(b2) << 8)
                        | static_cast<std::uint32_t>(b3)
                    : static_cast<std::uint32_t>(b0)
                        | (static_cast<std::uint32_t>(b1) << 8)
                        | (static_cast<std::uint32_t>(b2) << 16)
                        | (static_cast<std::uint32_t>(b3) << 24);

                if (value > 0x10FFFFU || (value >= 0xD800U && value <= 0xDFFFU))
                {
                    AppendReplacement(_decoded);
                }
                else
                {
                    AppendUtf8(_decoded, value);
                }
            }

            _decoderPending.erase(0, index);
            if (flush && !_decoderPending.empty())
            {
                AppendReplacement(_decoded);
                _decoderPending.clear();
            }
        }

        void DecodePending(bool flush)
        {
            switch (_encoding)
            {
            case Encoding::Utf8:
                DecodeUtf8(flush);
                break;
            case Encoding::Utf16LittleEndian:
                DecodeUtf16(false, flush);
                break;
            case Encoding::Utf16BigEndian:
                DecodeUtf16(true, flush);
                break;
            case Encoding::Utf32LittleEndian:
                DecodeUtf32(false, flush);
                break;
            case Encoding::Utf32BigEndian:
                DecodeUtf32(true, flush);
                break;
            }
        }

        void ProcessChunk(std::string chunk)
        {
            DetectEncoding(chunk);
            _decoderPending.append(chunk);
            DecodePending(false);
        }

        void FillDecoded()
        {
            const std::size_t bytesRead = ReadUnderlying();
            if (bytesRead == 0)
            {
                _eof = true;
                if (_checkPreamble)
                {
                    // StreamReader reaches EOF before another preamble check;
                    // a partial UTF-8 BOM is decoded as ordinary UTF-8 input.
                    _decoderPending.append(_preambleBuffer);
                    _preambleBuffer.clear();
                    _checkPreamble = false;
                }
                DecodePending(true);
                return;
            }

            std::string chunk(_raw.data(), bytesRead);
            if (!_checkPreamble)
            {
                ProcessChunk(std::move(chunk));
                return;
            }

            _preambleBuffer.append(chunk);
            constexpr std::array<unsigned char, 3> Utf8Preamble{0xEFU, 0xBBU, 0xBFU};
            const std::size_t length = std::min(_preambleBuffer.size(), Utf8Preamble.size());
            for (std::size_t index = _preamblePosition; index < length; ++index)
            {
                if (static_cast<unsigned char>(_preambleBuffer[index]) != Utf8Preamble[index])
                {
                    std::string buffered = std::move(_preambleBuffer);
                    _preambleBuffer.clear();
                    _preamblePosition = 0;
                    _checkPreamble = false;
                    ProcessChunk(std::move(buffered));
                    return;
                }
            }

            _preamblePosition = length;
            if (_preamblePosition == Utf8Preamble.size())
            {
                std::string remainder = _preambleBuffer.substr(Utf8Preamble.size());
                _preambleBuffer.clear();
                _preamblePosition = 0;
                _checkPreamble = false;
                _detectEncoding = false;
                ResetDecoderForEncoding(Encoding::Utf8);
                _decoderPending.append(remainder);
                DecodePending(false);
            }
        }
    };

    struct ConsoleBindings final
    {
        std::mutex Mutex;
        std::unique_ptr<DotNetStreamWriterBuffer> Output;
        std::unique_ptr<DotNetStreamWriterBuffer> Error;
        std::unique_ptr<DotNetStreamReaderBuffer> Input;
    };

    ConsoleBindings& Bindings()
    {
        // Console.Out/Error/In remain rooted static state for process lifetime.
        // Keep the Native stream buffers alive through iostream shutdown too.
        static ConsoleBindings* bindings = new ConsoleBindings();
        return *bindings;
    }

    void SetConsoleOut(std::unique_ptr<DotNetStreamWriterBuffer> writer)
    {
        ConsoleBindings& bindings = Bindings();
        std::lock_guard<std::mutex> lock(bindings.Mutex);
        DotNetStreamWriterBuffer* raw = writer.get();
        std::unique_ptr<DotNetStreamWriterBuffer> previous = std::move(bindings.Output);
        std::cout.exceptions(std::ios_base::goodbit);
        std::cout.rdbuf(raw);
        std::cout.clear();
        std::cout.unsetf(std::ios_base::unitbuf);
        std::cout.exceptions(std::ios_base::badbit);
        bindings.Output = std::move(writer);
    }

    void SetConsoleError(std::unique_ptr<DotNetStreamWriterBuffer> writer)
    {
        ConsoleBindings& bindings = Bindings();
        std::lock_guard<std::mutex> lock(bindings.Mutex);
        DotNetStreamWriterBuffer* raw = writer.get();
        std::unique_ptr<DotNetStreamWriterBuffer> previous = std::move(bindings.Error);
        std::cerr.exceptions(std::ios_base::goodbit);
        std::cerr.rdbuf(raw);
        std::cerr.clear();
        std::cerr.unsetf(std::ios_base::unitbuf);
        std::cerr.tie(nullptr);
        std::cerr.exceptions(std::ios_base::badbit);
        bindings.Error = std::move(writer);
    }

    void SetConsoleIn(std::unique_ptr<DotNetStreamReaderBuffer> reader)
    {
        ConsoleBindings& bindings = Bindings();
        std::lock_guard<std::mutex> lock(bindings.Mutex);
        DotNetStreamReaderBuffer* raw = reader.get();
        std::unique_ptr<DotNetStreamReaderBuffer> previous = std::move(bindings.Input);
        std::cin.exceptions(std::ios_base::goodbit);
        std::cin.rdbuf(raw);
        std::cin.clear();
        std::cin.tie(nullptr);
        std::cin.exceptions(std::ios_base::badbit);
        bindings.Input = std::move(reader);
    }
#endif
}

namespace MphRead
{
    namespace Mods
    {
        bool ConsoleWindow::OwnsItsConsole()
        {
#if !defined(_WIN32)
            return false;
#else
            if (IsOutputRedirected())
            {
                return false;
            }
            try
            {
                // Sized for the answer, not for the truth: any count above one
                // means somebody else is attached, and which processes those
                // are does not matter here.
                std::vector<DWORD> processes(4);
                return ::GetConsoleProcessList(
                    processes.data(), static_cast<DWORD>(processes.size())) == 1U;
            }
            catch (const std::exception&)
            {
                return false;
            }
#endif
        }

        void ConsoleWindow::Prepare(const std::vector<std::string>& args)
        {
#if !defined(_WIN32)
            (void)args;
            return;
#else
            const bool forced = HasFlag(args, "console");
            // No arguments means the launcher, and the launcher is a window.
            const bool guiOnly = args.empty() || HasFlag(args, "launcher");
            if (guiOnly && !forced)
            {
                return;
            }
            Show();
#endif
        }

        void ConsoleWindow::Show()
        {
#if !defined(_WIN32)
            return;
#else
            if (IsOutputRedirected() || IsInputRedirected())
            {
                // A parent is capturing us -- the launcher's extraction step
                // does exactly this. The streams already work; a console
                // window here would be a flash of black for nothing.
                return;
            }
            if (::GetConsoleWindow() != nullptr)
            {
                (void)::ShowWindow(::GetConsoleWindow(), 5); // SW_SHOW
                return;
            }
            if (AttachConsoleWithLastError(_attachParentProcess) == FALSE
                && AllocConsoleWithLastError() == FALSE)
            {
                return;
            }
            Rebind();
#endif
        }

        void ConsoleWindow::Rebind()
        {
#if defined(_WIN32)
            try
            {
                auto output = std::make_unique<DotNetStreamWriterBuffer>(
                    OpenStandardOutputLikeDotNet(StdOutputHandle));
                SetConsoleOut(std::move(output));

                auto error = std::make_unique<DotNetStreamWriterBuffer>(
                    OpenStandardOutputLikeDotNet(StdErrorHandle));
                SetConsoleError(std::move(error));

                auto input = std::make_unique<DotNetStreamReaderBuffer>(
                    OpenStandardInputLikeDotNet());
                SetConsoleIn(std::move(input));

                // The escape-sequence mode ConsoleSetup asks for, re-applied:
                // it ran before this console existed.
                const HANDLE handle = GetStdHandleWithLastError(StdOutputHandle);
                DWORD mode = 0;
                if (::GetConsoleMode(handle, &mode) != FALSE)
                {
                    (void)::SetConsoleMode(handle, mode | EnableVirtualTerminalProcessing);
                }
            }
            catch (const std::ios_base::failure&)
            {
                // Nothing to print to is survivable; a crash here is not.
            }
#endif
        }

        bool ConsoleWindow::HasFlag(
            const std::vector<std::string>& args, std::string_view name)
        {
            for (const std::string& argument : args)
            {
                std::size_t first = 0;
                while (first < argument.size() && argument[first] == '-')
                {
                    ++first;
                }

                if (EqualsOrdinalIgnoreCaseAscii(
                        std::string_view(argument).substr(first), name))
                {
                    return true;
                }
            }
            return false;
        }
    }
}
