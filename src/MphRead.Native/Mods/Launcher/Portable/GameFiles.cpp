#include "GameFiles.hpp"

#include "../../../Program.hpp"
#include "../../../Formats/Formats.hpp"
#include "../../../Utility/Extract.hpp"
#include "../../../NativeRuntime/System/IO.hpp"

#include <array>
#include <cerrno>
#include <chrono>
#include <condition_variable>
#include <cstddef>
#include <cstdint>
#include <cstdlib>
#include <cstring>
#include <exception>
#include <filesystem>
#include <fstream>
#include <functional>
#include <iostream>
#include <iterator>
#include <limits>
#include <memory>
#include <mutex>
#include <optional>
#include <stdexcept>
#include <streambuf>
#include <string>
#include <string_view>
#include <system_error>
#include <thread>
#include <utility>
#include <vector>

#if !defined(_WIN32)
#include <poll.h>
#include <pthread.h>
#endif

#if defined(_WIN32)
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>
#include <tlhelp32.h>
#elif defined(__APPLE__)
#include <fcntl.h>
#include <mach-o/dyld.h>
#include <signal.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>
#elif defined(__FreeBSD__)
#include <fcntl.h>
#include <limits.h>
#include <signal.h>
#include <sys/param.h>
#include <sys/stat.h>
#include <sys/sysctl.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>
#elif defined(__OpenBSD__)
#include <fcntl.h>
#include <signal.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>
#elif defined(__sun)
#include <fcntl.h>
#include <signal.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>
#elif defined(__linux__) || defined(__ANDROID__)
#include <fcntl.h>
#include <signal.h>
#if defined(__linux__) && !defined(__ANDROID__)
#include <sys/auxv.h>
#endif
#include <sys/stat.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>
#elif defined(__unix__)
#include <fcntl.h>
#include <signal.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>
#endif

using ::MphRead::NativeRuntime::DirectoryExists;
using ::MphRead::NativeRuntime::FileExists;
using ::MphRead::NativeRuntime::PathCombine;

namespace
{
    using Report = std::function<void(const std::string&)>;

    struct Utf8CodePoint final
    {
        std::uint32_t Value;
        std::size_t Length;
    };

    [[nodiscard]] std::optional<Utf8CodePoint> DecodeUtf8Forward(
        std::string_view text, std::size_t position) noexcept
    {
        if (position >= text.size())
        {
            return std::nullopt;
        }

        const auto first = static_cast<unsigned char>(text[position]);
        if (first <= 0x7FU)
        {
            return Utf8CodePoint{first, 1};
        }

        std::uint32_t value = 0;
        std::size_t length = 0;
        std::uint32_t minimum = 0;
        if ((first & 0xE0U) == 0xC0U)
        {
            value = first & 0x1FU;
            length = 2;
            minimum = 0x80U;
        }
        else if ((first & 0xF0U) == 0xE0U)
        {
            value = first & 0x0FU;
            length = 3;
            minimum = 0x800U;
        }
        else if ((first & 0xF8U) == 0xF0U)
        {
            value = first & 0x07U;
            length = 4;
            minimum = 0x10000U;
        }
        else
        {
            return std::nullopt;
        }

        if (position + length > text.size())
        {
            return std::nullopt;
        }
        for (std::size_t index = 1; index < length; ++index)
        {
            const auto next = static_cast<unsigned char>(text[position + index]);
            if ((next & 0xC0U) != 0x80U)
            {
                return std::nullopt;
            }
            value = (value << 6) | (next & 0x3FU);
        }
        if (value < minimum || value > 0x10FFFFU
            || (value >= 0xD800U && value <= 0xDFFFU))
        {
            return std::nullopt;
        }
        return Utf8CodePoint{value, length};
    }

    [[nodiscard]] std::optional<std::pair<Utf8CodePoint, std::size_t>>
        DecodeUtf8Backward(std::string_view text, std::size_t end) noexcept
    {
        if (end == 0 || end > text.size())
        {
            return std::nullopt;
        }

        std::size_t start = end - 1;
        while (start > 0
            && (static_cast<unsigned char>(text[start]) & 0xC0U) == 0x80U)
        {
            --start;
        }
        const std::optional<Utf8CodePoint> decoded = DecodeUtf8Forward(text, start);
        if (!decoded.has_value() || start + decoded->Length != end)
        {
            return std::nullopt;
        }
        return std::make_pair(*decoded, start);
    }

    [[nodiscard]] bool IsDotNetWhitespace(std::uint32_t value) noexcept
    {
        if (value >= 0x0009U && value <= 0x000DU)
        {
            return true;
        }
        switch (value)
        {
        case 0x0020U:
        case 0x0085U:
        case 0x00A0U:
        case 0x1680U:
        case 0x2000U:
        case 0x2001U:
        case 0x2002U:
        case 0x2003U:
        case 0x2004U:
        case 0x2005U:
        case 0x2006U:
        case 0x2007U:
        case 0x2008U:
        case 0x2009U:
        case 0x200AU:
        case 0x2028U:
        case 0x2029U:
        case 0x202FU:
        case 0x205FU:
        case 0x3000U:
            return true;
        default:
            return false;
        }
    }

    [[nodiscard]] std::string_view TrimDotNetWhitespace(
        std::string_view value) noexcept
    {
        std::size_t first = 0;
        std::size_t last = value.size();

        while (first < last)
        {
            const auto decoded = DecodeUtf8Forward(value.substr(0, last), first);
            if (!decoded.has_value() || !IsDotNetWhitespace(decoded->Value))
            {
                break;
            }
            first += decoded->Length;
        }
        while (last > first)
        {
            const auto decoded = DecodeUtf8Backward(value, last);
            if (!decoded.has_value() || !IsDotNetWhitespace(decoded->first.Value))
            {
                break;
            }
            last = decoded->second;
        }
        return value.substr(first, last - first);
    }

    [[nodiscard]] bool IsNullOrWhiteSpace(const std::string& value) noexcept
    {
        if (value.empty())
        {
            return true;
        }
        std::size_t position = 0;
        while (position < value.size())
        {
            const auto decoded = DecodeUtf8Forward(value, position);
            if (!decoded.has_value() || !IsDotNetWhitespace(decoded->Value))
            {
                return false;
            }
            position += decoded->Length;
        }
        return true;
    }

    [[nodiscard]] constexpr bool IsVersionIntegerWhitespace(
        unsigned char value) noexcept
    {
        return value == 0x20U || (value >= 0x09U && value <= 0x0DU);
    }

    [[nodiscard]] bool TryParseVersionComponent(
        std::string_view component, std::int32_t& result) noexcept
    {
        std::size_t first = 0;
        while (first < component.size()
            && IsVersionIntegerWhitespace(
                static_cast<unsigned char>(component[first])))
        {
            ++first;
        }
        if (first == component.size())
        {
            return false;
        }

        bool negative = false;
        if (component[first] == '+' || component[first] == '-')
        {
            negative = component[first] == '-';
            ++first;
            if (first == component.size())
            {
                return false;
            }
        }
        if (component[first] < '0' || component[first] > '9')
        {
            return false;
        }

        const std::uint64_t limit = negative ? 2147483648ULL : 2147483647ULL;
        std::uint64_t value = 0;
        std::size_t cursor = first;
        while (cursor < component.size()
            && component[cursor] >= '0' && component[cursor] <= '9')
        {
            const auto digit = static_cast<unsigned char>(component[cursor] - '0');
            if (value > limit / 10ULL
                || (value == limit / 10ULL && digit > limit % 10ULL))
            {
                return false;
            }
            value = value * 10ULL + digit;
            ++cursor;
        }

        while (cursor < component.size()
            && IsVersionIntegerWhitespace(
                static_cast<unsigned char>(component[cursor])))
        {
            ++cursor;
        }
        while (cursor < component.size() && component[cursor] == '\0')
        {
            ++cursor;
        }
        if (cursor != component.size() || negative)
        {
            return false;
        }

        result = static_cast<std::int32_t>(value);
        return true;
    }

    [[nodiscard]] std::optional<MphRead::Mods::Update::Version>
        TryParseManagedVersion(std::string_view text)
    {
        std::array<std::string_view, 4> parts{};
        std::size_t count = 0;
        std::size_t start = 0;
        for (;;)
        {
            if (count == parts.size())
            {
                return std::nullopt;
            }
            const std::size_t dot = text.find('.', start);
            parts[count++] = dot == std::string_view::npos
                ? text.substr(start)
                : text.substr(start, dot - start);
            if (dot == std::string_view::npos)
            {
                break;
            }
            start = dot + 1;
        }
        if (count < 2)
        {
            return std::nullopt;
        }

        std::array<std::int32_t, 4> values{};
        for (std::size_t index = 0; index < count; ++index)
        {
            if (!TryParseVersionComponent(parts[index], values[index]))
            {
                return std::nullopt;
            }
        }

        switch (count)
        {
        case 2:
            return MphRead::Mods::Update::Version(values[0], values[1]);
        case 3:
            return MphRead::Mods::Update::Version(
                values[0], values[1], values[2]);
        case 4:
            return MphRead::Mods::Update::Version(
                values[0], values[1], values[2], values[3]);
        default:
            return std::nullopt;
        }
    }

    void AppendUtf8(std::string& output, std::uint32_t value)
    {
        if (value > 0x10FFFFU || (value >= 0xD800U && value <= 0xDFFFU))
        {
            value = 0xFFFDU;
        }
        if (value <= 0x7FU)
        {
            output.push_back(static_cast<char>(value));
        }
        else if (value <= 0x7FFU)
        {
            output.push_back(static_cast<char>(0xC0U | (value >> 6)));
            output.push_back(static_cast<char>(0x80U | (value & 0x3FU)));
        }
        else if (value <= 0xFFFFU)
        {
            output.push_back(static_cast<char>(0xE0U | (value >> 12)));
            output.push_back(static_cast<char>(0x80U | ((value >> 6) & 0x3FU)));
            output.push_back(static_cast<char>(0x80U | (value & 0x3FU)));
        }
        else
        {
            output.push_back(static_cast<char>(0xF0U | (value >> 18)));
            output.push_back(static_cast<char>(0x80U | ((value >> 12) & 0x3FU)));
            output.push_back(static_cast<char>(0x80U | ((value >> 6) & 0x3FU)));
            output.push_back(static_cast<char>(0x80U | (value & 0x3FU)));
        }
    }

    [[nodiscard]] std::string DecodeUtf8Text(std::string_view bytes)
    {
        std::string output;
        output.reserve(bytes.size());
        for (std::size_t index = 0; index < bytes.size();)
        {
            const auto decoded = DecodeUtf8Forward(bytes, index);
            if (decoded.has_value())
            {
                output.append(bytes.substr(index, decoded->Length));
                index += decoded->Length;
            }
            else
            {
                AppendUtf8(output, 0xFFFDU);
                ++index;
            }
        }
        return output;
    }

    [[nodiscard]] std::string DecodeUtf16Text(
        std::string_view bytes, bool bigEndian)
    {
        std::string output;
        output.reserve(bytes.size());
        const auto readUnit = [&](std::size_t index) -> std::uint16_t
        {
            const auto first = static_cast<unsigned char>(bytes[index]);
            const auto second = static_cast<unsigned char>(bytes[index + 1]);
            return bigEndian
                ? static_cast<std::uint16_t>((first << 8) | second)
                : static_cast<std::uint16_t>(first | (second << 8));
        };

        std::size_t index = 0;
        while (index + 1 < bytes.size())
        {
            const std::uint16_t first = readUnit(index);
            index += 2;
            if (first >= 0xD800U && first <= 0xDBFFU)
            {
                if (index + 1 < bytes.size())
                {
                    const std::uint16_t second = readUnit(index);
                    if (second >= 0xDC00U && second <= 0xDFFFU)
                    {
                        index += 2;
                        AppendUtf8(output, 0x10000U
                            + ((static_cast<std::uint32_t>(first) - 0xD800U) << 10)
                            + (static_cast<std::uint32_t>(second) - 0xDC00U));
                        continue;
                    }
                }
                AppendUtf8(output, 0xFFFDU);
            }
            else if (first >= 0xDC00U && first <= 0xDFFFU)
            {
                AppendUtf8(output, 0xFFFDU);
            }
            else
            {
                AppendUtf8(output, first);
            }
        }
        if (index < bytes.size())
        {
            AppendUtf8(output, 0xFFFDU);
        }
        return output;
    }

    [[nodiscard]] std::string DecodeUtf32Text(
        std::string_view bytes, bool bigEndian)
    {
        std::string output;
        output.reserve(bytes.size());
        std::size_t index = 0;
        while (index + 3 < bytes.size())
        {
            const auto b0 = static_cast<unsigned char>(bytes[index]);
            const auto b1 = static_cast<unsigned char>(bytes[index + 1]);
            const auto b2 = static_cast<unsigned char>(bytes[index + 2]);
            const auto b3 = static_cast<unsigned char>(bytes[index + 3]);
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
            AppendUtf8(output, value);
        }
        if (index < bytes.size())
        {
            AppendUtf8(output, 0xFFFDU);
        }
        return output;
    }

#if defined(_WIN32)
    void AppendWtf8(std::string& output, std::uint32_t value)
    {
        if (value <= 0x7FU)
        {
            output.push_back(static_cast<char>(value));
        }
        else if (value <= 0x7FFU)
        {
            output.push_back(static_cast<char>(0xC0U | (value >> 6)));
            output.push_back(static_cast<char>(0x80U | (value & 0x3FU)));
        }
        else if (value <= 0xFFFFU)
        {
            output.push_back(static_cast<char>(0xE0U | (value >> 12)));
            output.push_back(static_cast<char>(0x80U | ((value >> 6) & 0x3FU)));
            output.push_back(static_cast<char>(0x80U | (value & 0x3FU)));
        }
        else
        {
            output.push_back(static_cast<char>(0xF0U | (value >> 18)));
            output.push_back(static_cast<char>(0x80U | ((value >> 12) & 0x3FU)));
            output.push_back(static_cast<char>(0x80U | ((value >> 6) & 0x3FU)));
            output.push_back(static_cast<char>(0x80U | (value & 0x3FU)));
        }
    }

    [[nodiscard]] std::string Utf8FromWide(const wchar_t* value, std::size_t length)
    {
        static_assert(sizeof(wchar_t) == sizeof(std::uint16_t));
        std::string result;
        result.reserve(length);
        for (std::size_t index = 0; index < length; ++index)
        {
            const std::uint32_t first = static_cast<std::uint16_t>(value[index]);
            if (first >= 0xD800U && first <= 0xDBFFU && index + 1 < length)
            {
                const std::uint32_t second = static_cast<std::uint16_t>(value[index + 1]);
                if (second >= 0xDC00U && second <= 0xDFFFU)
                {
                    AppendWtf8(result, 0x10000U
                        + ((first - 0xD800U) << 10) + (second - 0xDC00U));
                    ++index;
                    continue;
                }
            }
            AppendWtf8(result, first);
        }
        return result;
    }

    [[nodiscard]] std::wstring WideFromWtf8(std::string_view value)
    {
        std::wstring result;
        result.reserve(value.size());
        for (std::size_t index = 0; index < value.size();)
        {
            const unsigned char first = static_cast<unsigned char>(value[index]);
            if (first <= 0x7FU)
            {
                result.push_back(static_cast<wchar_t>(first));
                ++index;
                continue;
            }

            std::uint32_t codePoint = 0xFFFDU;
            std::size_t length = 1;
            if (first >= 0xC2U && first <= 0xDFU && index + 1 < value.size())
            {
                const unsigned char b1 = static_cast<unsigned char>(value[index + 1]);
                if ((b1 & 0xC0U) == 0x80U)
                {
                    codePoint = ((first & 0x1FU) << 6) | (b1 & 0x3FU);
                    length = 2;
                }
            }
            else if (first >= 0xE0U && first <= 0xEFU && index + 2 < value.size())
            {
                const unsigned char b1 = static_cast<unsigned char>(value[index + 1]);
                const unsigned char b2 = static_cast<unsigned char>(value[index + 2]);
                if ((b1 & 0xC0U) == 0x80U && (b2 & 0xC0U) == 0x80U
                    && !(first == 0xE0U && b1 < 0xA0U))
                {
                    codePoint = ((first & 0x0FU) << 12)
                        | ((b1 & 0x3FU) << 6) | (b2 & 0x3FU);
                    length = 3;
                }
            }
            else if (first >= 0xF0U && first <= 0xF4U && index + 3 < value.size())
            {
                const unsigned char b1 = static_cast<unsigned char>(value[index + 1]);
                const unsigned char b2 = static_cast<unsigned char>(value[index + 2]);
                const unsigned char b3 = static_cast<unsigned char>(value[index + 3]);
                if ((b1 & 0xC0U) == 0x80U && (b2 & 0xC0U) == 0x80U
                    && (b3 & 0xC0U) == 0x80U
                    && !(first == 0xF0U && b1 < 0x90U)
                    && !(first == 0xF4U && b1 > 0x8FU))
                {
                    codePoint = ((first & 0x07U) << 18)
                        | ((b1 & 0x3FU) << 12)
                        | ((b2 & 0x3FU) << 6) | (b3 & 0x3FU);
                    length = 4;
                }
            }

            index += length;
            if (codePoint <= 0xFFFFU)
            {
                result.push_back(static_cast<wchar_t>(codePoint));
            }
            else
            {
                codePoint -= 0x10000U;
                result.push_back(static_cast<wchar_t>(0xD800U + (codePoint >> 10)));
                result.push_back(static_cast<wchar_t>(0xDC00U + (codePoint & 0x3FFU)));
            }
        }
        return result;
    }
#endif

    [[nodiscard]] std::filesystem::path PathFromManagedString(std::string_view value)
    {
#if defined(_WIN32)
        return std::filesystem::path(WideFromWtf8(value));
#else
        return std::filesystem::path(value);
#endif
    }

#if defined(__APPLE__) || defined(__OpenBSD__) || defined(__sun) \
    || defined(__linux__) || defined(__ANDROID__) \
    || (defined(__unix__) && !defined(__EMSCRIPTEN__) && !defined(__wasi__))
    [[nodiscard]] std::optional<std::string> RealPath(const char* path)
    {
        std::unique_ptr<char, decltype(&std::free)> resolved(
            realpath(path, nullptr), &std::free);
        if (!resolved)
        {
            return std::nullopt;
        }
        return DecodeUtf8Text(resolved.get());
    }
#endif

    [[nodiscard]] std::optional<std::string> ProcessPath()
    {
#if defined(_WIN32)
        std::vector<wchar_t> buffer(260);
        for (;;)
        {
            const DWORD length = GetModuleFileNameW(
                nullptr, buffer.data(), static_cast<DWORD>(buffer.size()));
            if (length == 0)
            {
                return std::nullopt;
            }
            if (length < buffer.size())
            {
                return Utf8FromWide(buffer.data(), length);
            }
            if (buffer.size() > static_cast<std::size_t>(
                    std::numeric_limits<DWORD>::max()) / 2U)
            {
                return std::nullopt;
            }
            buffer.resize(buffer.size() * 2U);
        }
#elif defined(__APPLE__)
        std::uint32_t size = 1;
        char probe = 0;
        if (_NSGetExecutablePath(&probe, &size) == 0 || size == 0)
        {
            return std::nullopt;
        }
        std::vector<char> buffer(size);
        if (_NSGetExecutablePath(buffer.data(), &size) != 0)
        {
            return std::nullopt;
        }
        return RealPath(buffer.data());
#elif defined(__FreeBSD__)
        static const int name[] = {CTL_KERN, KERN_PROC, KERN_PROC_PATHNAME, -1};
        char path[PATH_MAX];
        std::size_t length = sizeof(path);
        if (sysctl(name, 4, path, &length, nullptr, 0) != 0 || length == 0)
        {
            return std::nullopt;
        }
        return DecodeUtf8Text(std::string_view(
            path, path[length - 1] == '\0' ? length - 1 : length));
#elif defined(__OpenBSD__)
        return RealPath("/proc/curproc/exe");
#elif defined(__sun)
        const char* path = getexecname();
        return path == nullptr ? std::nullopt : RealPath(path);
#elif defined(__EMSCRIPTEN__) || defined(__wasi__)
        return std::nullopt;
#elif defined(__ANDROID__)
        return RealPath("/proc/self/exe");
#elif defined(__linux__)
        if (std::optional<std::string> path = RealPath("/proc/self/exe"))
        {
            return path;
        }
#if defined(AT_EXECFN)
        const auto executable = reinterpret_cast<const char*>(getauxval(AT_EXECFN));
        if (executable != nullptr)
        {
            return RealPath(executable);
        }
#endif
        return std::nullopt;
#elif defined(__unix__)
        return RealPath("/proc/curproc/exe");
#else
        return std::nullopt;
#endif
    }

    [[nodiscard]] std::string CurrentDirectoryWithSeparator()
    {
        const std::filesystem::path current = std::filesystem::current_path();
#if defined(_WIN32)
        std::string result = Utf8FromWide(current.native().data(), current.native().size());
        if (result.empty() || (result.back() != '\\' && result.back() != '/'))
        {
            result.push_back('\\');
        }
#else
        std::string result = DecodeUtf8Text(current.native());
        if (result.empty() || result.back() != '/')
        {
            result.push_back('/');
        }
#endif
        return result;
    }

    [[nodiscard]] std::string AppContextBaseDirectory()
    {
        const std::optional<std::string> processPath = ProcessPath();
        if (processPath.has_value())
        {
#if defined(_WIN32)
            const std::size_t separator = processPath->find_last_of("/\\");
#else
            const std::size_t separator = processPath->find_last_of('/');
#endif
            if (separator != std::string::npos)
            {
                return processPath->substr(0, separator + 1);
            }
        }
        return CurrentDirectoryWithSeparator();
    }

    [[noreturn]] void ThrowReadFailure(std::string_view path, int error)
    {
#if defined(_WIN32)
        if (error == static_cast<int>(ERROR_ACCESS_DENIED)
            || error == static_cast<int>(ERROR_OPERATION_ABORTED))
        {
            throw std::runtime_error(
                "Could not read file: " + std::string(path) + " (error "
                + std::to_string(error) + ")");
        }
#else
        if (error == EACCES || error == EBADF || error == EPERM || error == ECANCELED)
        {
            throw std::runtime_error(std::system_error(
                error, std::generic_category()).what());
        }
        if (error == EFBIG)
        {
            throw std::out_of_range(std::system_error(
                error, std::generic_category()).what());
        }
#endif
        throw std::ios_base::failure(
            "Could not read file: " + std::string(path));
    }

    [[nodiscard]] std::string ReadAllBytesForText(std::string_view path)
    {
        const std::filesystem::path nativePath = PathFromManagedString(path);
        std::string bytes;
        std::array<char, 4096> buffer{};
#if defined(_WIN32)
        HANDLE handle = CreateFileW(
            nativePath.c_str(), GENERIC_READ, FILE_SHARE_READ, nullptr,
            OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL | FILE_FLAG_SEQUENTIAL_SCAN, nullptr);
        if (handle == INVALID_HANDLE_VALUE)
        {
            ThrowReadFailure(path, static_cast<int>(GetLastError()));
        }
        try
        {
            for (;;)
            {
                DWORD count = 0;
                if (!ReadFile(handle, buffer.data(), static_cast<DWORD>(buffer.size()), &count, nullptr))
                {
                    ThrowReadFailure(path, static_cast<int>(GetLastError()));
                }
                if (count == 0)
                {
                    break;
                }
                bytes.append(buffer.data(), static_cast<std::size_t>(count));
            }
        }
        catch (...)
        {
            CloseHandle(handle);
            throw;
        }
        CloseHandle(handle);
#else
        const int fd = ::open(nativePath.c_str(), O_RDONLY);
        if (fd < 0)
        {
            ThrowReadFailure(path, errno);
        }
        try
        {
            struct stat info{};
            if (::fstat(fd, &info) != 0)
            {
                ThrowReadFailure(path, errno);
            }
            if (S_ISDIR(info.st_mode))
            {
                ThrowReadFailure(path, EACCES);
            }
            for (;;)
            {
                const ssize_t count = ::read(fd, buffer.data(), buffer.size());
                if (count > 0)
                {
                    bytes.append(buffer.data(), static_cast<std::size_t>(count));
                    continue;
                }
                if (count == 0)
                {
                    break;
                }
                if (errno == EINTR)
                {
                    continue;
                }
                ThrowReadFailure(path, errno);
            }
        }
        catch (...)
        {
            (void)::close(fd);
            throw;
        }
        (void)::close(fd);
#endif
        return bytes;
    }

    [[nodiscard]] std::string ReadAllText(std::string_view path)
    {
        const std::string bytes = ReadAllBytesForText(path);

        const auto byteAt = [&](std::size_t index) -> unsigned char
        {
            return static_cast<unsigned char>(bytes[index]);
        };
        if (bytes.size() >= 4
            && byteAt(0) == 0xFFU && byteAt(1) == 0xFEU
            && byteAt(2) == 0x00U && byteAt(3) == 0x00U)
        {
            return DecodeUtf32Text(std::string_view(bytes).substr(4), false);
        }
        if (bytes.size() >= 4
            && byteAt(0) == 0x00U && byteAt(1) == 0x00U
            && byteAt(2) == 0xFEU && byteAt(3) == 0xFFU)
        {
            return DecodeUtf32Text(std::string_view(bytes).substr(4), true);
        }
        if (bytes.size() >= 3
            && byteAt(0) == 0xEFU && byteAt(1) == 0xBBU && byteAt(2) == 0xBFU)
        {
            return DecodeUtf8Text(std::string_view(bytes).substr(3));
        }
        if (bytes.size() >= 2 && byteAt(0) == 0xFFU && byteAt(1) == 0xFEU)
        {
            return DecodeUtf16Text(std::string_view(bytes).substr(2), false);
        }
        if (bytes.size() >= 2 && byteAt(0) == 0xFEU && byteAt(1) == 0xFFU)
        {
            return DecodeUtf16Text(std::string_view(bytes).substr(2), true);
        }
        return DecodeUtf8Text(bytes);
    }

    struct AsyncReportAbort final
    {
    };

    void ReportAsyncLine(const Report& report, const std::string& line)
    {
        try
        {
            report(line);
        }
        catch (...)
        {
            // .NET 9 AsyncStreamReader captures a user callback exception,
            // queues a ThreadPool work item that rethrows it unhandled, then
            // stops that asynchronous reader. A detached C++ thread preserves
            // the same asynchronous unhandled-exception boundary; the marker
            // is swallowed by ReaderThread so it never surfaces via RunSetup.
            const std::exception_ptr failure = std::current_exception();
            try
            {
                std::thread([failure]() { std::rethrow_exception(failure); }).detach();
            }
            catch (...)
            {
                std::terminate();
            }
            throw AsyncReportAbort{};
        }
    }

    void EmitManagedLines(std::string& pending, bool endOfStream, const Report& report)
    {
        std::size_t start = 0;
        std::size_t index = 0;
        while (index < pending.size())
        {
            if (pending[index] != '\r' && pending[index] != '\n')
            {
                ++index;
                continue;
            }

            if (pending[index] == '\r' && index + 1 == pending.size() && !endOfStream)
            {
                break;
            }

            const std::size_t end = index;
            if (pending[index] == '\r' && index + 1 < pending.size()
                && pending[index + 1] == '\n')
            {
                index += 2;
            }
            else
            {
                ++index;
            }
            ReportAsyncLine(report,
                DecodeUtf8Text(std::string_view(pending).substr(start, end - start)));
            start = index;
        }

        if (endOfStream && start < pending.size())
        {
            ReportAsyncLine(report,
                DecodeUtf8Text(std::string_view(pending).substr(start)));
            start = pending.size();
        }
        if (start != 0)
        {
            pending.erase(0, start);
        }
    }

#if defined(_WIN32)
    [[nodiscard]] std::string Win32Message(DWORD error)
    {
        wchar_t* buffer = nullptr;
        const DWORD length = FormatMessageW(
            FORMAT_MESSAGE_ALLOCATE_BUFFER | FORMAT_MESSAGE_FROM_SYSTEM
                | FORMAT_MESSAGE_IGNORE_INSERTS,
            nullptr, error, 0, reinterpret_cast<wchar_t*>(&buffer), 0, nullptr);
        if (length == 0 || buffer == nullptr)
        {
            return "Win32 error " + std::to_string(error);
        }
        std::wstring_view wide(buffer, length);
        while (!wide.empty() && (wide.back() == L'\r' || wide.back() == L'\n'))
        {
            wide.remove_suffix(1);
        }
        std::string result = Utf8FromWide(wide.data(), wide.size());
        LocalFree(buffer);
        return result;
    }

    class UniqueHandle final
    {
    public:
        UniqueHandle() noexcept = default;
        explicit UniqueHandle(HANDLE value) noexcept : _value(value) {}
        UniqueHandle(const UniqueHandle&) = delete;
        UniqueHandle& operator=(const UniqueHandle&) = delete;
        UniqueHandle(UniqueHandle&& other) noexcept
            : _value(std::exchange(other._value, nullptr)) {}
        UniqueHandle& operator=(UniqueHandle&& other) noexcept
        {
            if (this != &other)
            {
                Reset();
                _value = std::exchange(other._value, nullptr);
            }
            return *this;
        }
        ~UniqueHandle() { Reset(); }
        [[nodiscard]] HANDLE Get() const noexcept { return _value; }
        [[nodiscard]] explicit operator bool() const noexcept
        {
            return _value != nullptr && _value != INVALID_HANDLE_VALUE;
        }
        HANDLE Release() noexcept { return std::exchange(_value, nullptr); }
        void Reset(HANDLE value = nullptr) noexcept
        {
            if (_value != nullptr && _value != INVALID_HANDLE_VALUE)
            {
                CloseHandle(_value);
            }
            _value = value;
        }
    private:
        HANDLE _value = nullptr;
    };

    [[nodiscard]] std::wstring QuoteWindowsArgument(const std::wstring& value)
    {
        if (!value.empty() && value.find_first_of(L" \t\n\v\"") == std::wstring::npos)
        {
            return value;
        }
        std::wstring result;
        result.push_back(L'"');
        std::size_t slashes = 0;
        for (wchar_t ch : value)
        {
            if (ch == L'\\')
            {
                ++slashes;
                continue;
            }
            if (ch == L'"')
            {
                result.append(slashes * 2 + 1, L'\\');
                result.push_back(L'"');
                slashes = 0;
                continue;
            }
            result.append(slashes, L'\\');
            slashes = 0;
            result.push_back(ch);
        }
        result.append(slashes * 2, L'\\');
        result.push_back(L'"');
        return result;
    }

    struct WindowsChild final
    {
        UniqueHandle Process;
        DWORD ProcessId = 0;
        UniqueHandle Input;
        UniqueHandle Output;
        UniqueHandle Error;
    };

    [[nodiscard]] WindowsChild StartChildWindows(
        const std::string& executable, const std::string& romPath,
        const std::string& workingDirectory)
    {
        SECURITY_ATTRIBUTES attributes{};
        attributes.nLength = sizeof(attributes);
        attributes.bInheritHandle = TRUE;

        HANDLE stdinReadRaw = nullptr;
        HANDLE stdinWriteRaw = nullptr;
        HANDLE stdoutReadRaw = nullptr;
        HANDLE stdoutWriteRaw = nullptr;
        HANDLE stderrReadRaw = nullptr;
        HANDLE stderrWriteRaw = nullptr;
        if (!CreatePipe(&stdinReadRaw, &stdinWriteRaw, &attributes, 0)
            || !CreatePipe(&stdoutReadRaw, &stdoutWriteRaw, &attributes, 0)
            || !CreatePipe(&stderrReadRaw, &stderrWriteRaw, &attributes, 0))
        {
            const DWORD error = GetLastError();
            if (stdinReadRaw) CloseHandle(stdinReadRaw);
            if (stdinWriteRaw) CloseHandle(stdinWriteRaw);
            if (stdoutReadRaw) CloseHandle(stdoutReadRaw);
            if (stdoutWriteRaw) CloseHandle(stdoutWriteRaw);
            if (stderrReadRaw) CloseHandle(stderrReadRaw);
            if (stderrWriteRaw) CloseHandle(stderrWriteRaw);
            throw std::runtime_error(Win32Message(error));
        }

        UniqueHandle stdinRead(stdinReadRaw);
        UniqueHandle stdinWrite(stdinWriteRaw);
        UniqueHandle stdoutRead(stdoutReadRaw);
        UniqueHandle stdoutWrite(stdoutWriteRaw);
        UniqueHandle stderrRead(stderrReadRaw);
        UniqueHandle stderrWrite(stderrWriteRaw);

        if (!SetHandleInformation(stdinWrite.Get(), HANDLE_FLAG_INHERIT, 0)
            || !SetHandleInformation(stdoutRead.Get(), HANDLE_FLAG_INHERIT, 0)
            || !SetHandleInformation(stderrRead.Get(), HANDLE_FLAG_INHERIT, 0))
        {
            throw std::runtime_error(Win32Message(GetLastError()));
        }

        const std::wstring executableWide = WideFromWtf8(executable);
        const std::wstring romWide = WideFromWtf8(romPath);
        std::wstring commandLine = QuoteWindowsArgument(executableWide)
            + L" " + QuoteWindowsArgument(romWide);
        std::vector<wchar_t> mutableCommand(commandLine.begin(), commandLine.end());
        mutableCommand.push_back(L'\0');
        const std::wstring directoryWide = WideFromWtf8(workingDirectory);

        STARTUPINFOW startup{};
        startup.cb = sizeof(startup);
        startup.dwFlags = STARTF_USESTDHANDLES;
        startup.hStdInput = stdinRead.Get();
        startup.hStdOutput = stdoutWrite.Get();
        startup.hStdError = stderrWrite.Get();
        PROCESS_INFORMATION process{};

        if (!CreateProcessW(
                executableWide.c_str(), mutableCommand.data(), nullptr, nullptr, TRUE,
                CREATE_NO_WINDOW, nullptr,
                workingDirectory.empty() ? nullptr : directoryWide.c_str(),
                &startup, &process))
        {
            throw std::runtime_error(Win32Message(GetLastError()));
        }

        UniqueHandle processHandle(process.hProcess);
        UniqueHandle threadHandle(process.hThread);
        stdinRead.Reset();
        stdoutWrite.Reset();
        stderrWrite.Reset();
        return WindowsChild{
            std::move(processHandle), process.dwProcessId, std::move(stdinWrite),
            std::move(stdoutRead), std::move(stderrRead)};
    }

    void ReadWindowsPipe(HANDLE handle, HANDLE cancel, const Report& report)
    {
        std::string pending;
        std::array<char, 4096> buffer{};
        for (;;)
        {
            const DWORD cancelState = WaitForSingleObject(cancel, 0);
            if (cancelState == WAIT_OBJECT_0)
            {
                return;
            }
            if (cancelState == WAIT_FAILED)
            {
                throw std::runtime_error(Win32Message(GetLastError()));
            }

            DWORD available = 0;
            if (!PeekNamedPipe(handle, nullptr, 0, nullptr, &available, nullptr))
            {
                const DWORD error = GetLastError();
                if (error == ERROR_BROKEN_PIPE)
                {
                    break;
                }
                throw std::runtime_error(Win32Message(error));
            }
            if (available == 0)
            {
                const DWORD wait = WaitForSingleObject(cancel, 1);
                if (wait == WAIT_OBJECT_0)
                {
                    return;
                }
                if (wait == WAIT_FAILED)
                {
                    throw std::runtime_error(Win32Message(GetLastError()));
                }
                continue;
            }

            DWORD read = 0;
            const DWORD requested = std::min<DWORD>(
                available, static_cast<DWORD>(buffer.size()));
            if (!ReadFile(handle, buffer.data(), requested, &read, nullptr))
            {
                const DWORD error = GetLastError();
                if (error == ERROR_BROKEN_PIPE)
                {
                    break;
                }
                throw std::runtime_error(Win32Message(error));
            }
            if (read == 0)
            {
                break;
            }
            pending.append(buffer.data(), read);
            EmitManagedLines(pending, false, report);
        }
        EmitManagedLines(pending, true, report);
    }

    void WriteWindowsInput(HANDLE handle)
    {
        static constexpr std::string_view text = "y\r\n\r\n";
        std::size_t offset = 0;
        while (offset < text.size())
        {
            DWORD written = 0;
            if (!WriteFile(handle, text.data() + offset,
                    static_cast<DWORD>(text.size() - offset), &written, nullptr))
            {
                const DWORD error = GetLastError();
                if (error == ERROR_BROKEN_PIPE || error == ERROR_NO_DATA)
                {
                    return;
                }
                throw std::ios_base::failure(Win32Message(error));
            }
            offset += written;
        }
    }

    void KillWindowsTree(DWORD rootProcessId)
    {
        std::vector<std::pair<DWORD, DWORD>> processes;
        UniqueHandle snapshot(CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0));
        if (snapshot)
        {
            PROCESSENTRY32W entry{};
            entry.dwSize = sizeof(entry);
            if (Process32FirstW(snapshot.Get(), &entry))
            {
                do
                {
                    processes.emplace_back(entry.th32ProcessID, entry.th32ParentProcessID);
                }
                while (Process32NextW(snapshot.Get(), &entry));
            }
        }

        std::vector<DWORD> ordered;
        std::function<void(DWORD)> addChildren = [&](DWORD parent)
        {
            for (const auto& [pid, ppid] : processes)
            {
                if (ppid == parent && pid != parent)
                {
                    addChildren(pid);
                    ordered.push_back(pid);
                }
            }
        };
        addChildren(rootProcessId);
        ordered.push_back(rootProcessId);
        for (DWORD pid : ordered)
        {
            UniqueHandle process(OpenProcess(PROCESS_TERMINATE, FALSE, pid));
            if (process && !TerminateProcess(process.Get(), 1) && pid == rootProcessId)
            {
                throw std::runtime_error(Win32Message(GetLastError()));
            }
        }
    }
#else
    class UniqueFd final
    {
    public:
        UniqueFd() noexcept = default;
        explicit UniqueFd(int value) noexcept : _value(value) {}
        UniqueFd(const UniqueFd&) = delete;
        UniqueFd& operator=(const UniqueFd&) = delete;
        UniqueFd(UniqueFd&& other) noexcept : _value(std::exchange(other._value, -1)) {}
        UniqueFd& operator=(UniqueFd&& other) noexcept
        {
            if (this != &other)
            {
                Reset();
                _value = std::exchange(other._value, -1);
            }
            return *this;
        }
        ~UniqueFd() { Reset(); }
        [[nodiscard]] int Get() const noexcept { return _value; }
        [[nodiscard]] explicit operator bool() const noexcept { return _value >= 0; }
        void Reset(int value = -1) noexcept
        {
            if (_value >= 0)
            {
                (void)::close(_value);
            }
            _value = value;
        }
    private:
        int _value = -1;
    };

    struct PosixChild final
    {
        pid_t Pid = -1;
        UniqueFd Input;
        UniqueFd Output;
        UniqueFd Error;
    };

    [[nodiscard]] std::string ErrnoMessage(int error)
    {
        return std::system_error(error, std::generic_category()).what();
    }

    [[nodiscard]] PosixChild StartChildPosix(
        const std::string& executable, const std::string& romPath,
        const std::string& workingDirectory)
    {
        int inputPipe[2] = {-1, -1};
        int outputPipe[2] = {-1, -1};
        int errorPipe[2] = {-1, -1};
        int startupPipe[2] = {-1, -1};
        if (::pipe(inputPipe) != 0 || ::pipe(outputPipe) != 0
            || ::pipe(errorPipe) != 0 || ::pipe(startupPipe) != 0)
        {
            const int error = errno;
            for (int fd : inputPipe) if (fd >= 0) (void)::close(fd);
            for (int fd : outputPipe) if (fd >= 0) (void)::close(fd);
            for (int fd : errorPipe) if (fd >= 0) (void)::close(fd);
            for (int fd : startupPipe) if (fd >= 0) (void)::close(fd);
            throw std::runtime_error(ErrnoMessage(error));
        }
        if (::fcntl(startupPipe[1], F_SETFD, FD_CLOEXEC) != 0)
        {
            const int error = errno;
            for (int fd : inputPipe) (void)::close(fd);
            for (int fd : outputPipe) (void)::close(fd);
            for (int fd : errorPipe) (void)::close(fd);
            for (int fd : startupPipe) (void)::close(fd);
            throw std::runtime_error(ErrnoMessage(error));
        }

        const pid_t pid = ::fork();
        if (pid < 0)
        {
            const int error = errno;
            for (int fd : inputPipe) (void)::close(fd);
            for (int fd : outputPipe) (void)::close(fd);
            for (int fd : errorPipe) (void)::close(fd);
            for (int fd : startupPipe) (void)::close(fd);
            throw std::runtime_error(ErrnoMessage(error));
        }
        if (pid == 0)
        {
            (void)::close(startupPipe[0]);
            const auto failStartup = [&](int error) noexcept
            {
                const char* data = reinterpret_cast<const char*>(&error);
                std::size_t offset = 0;
                while (offset < sizeof(error))
                {
                    const ssize_t written = ::write(
                        startupPipe[1], data + offset, sizeof(error) - offset);
                    if (written > 0)
                    {
                        offset += static_cast<std::size_t>(written);
                    }
                    else if (written < 0 && errno == EINTR)
                    {
                        continue;
                    }
                    else
                    {
                        break;
                    }
                }
                _exit(127);
            };

            if (!workingDirectory.empty() && ::chdir(workingDirectory.c_str()) != 0)
            {
                failStartup(errno);
            }
            if (::dup2(inputPipe[0], STDIN_FILENO) < 0
                || ::dup2(outputPipe[1], STDOUT_FILENO) < 0
                || ::dup2(errorPipe[1], STDERR_FILENO) < 0)
            {
                failStartup(errno);
            }
            for (int fd : inputPipe) (void)::close(fd);
            for (int fd : outputPipe) (void)::close(fd);
            for (int fd : errorPipe) (void)::close(fd);
            char* const argv[] = {
                const_cast<char*>(executable.c_str()),
                const_cast<char*>(romPath.c_str()),
                nullptr
            };
            ::execv(executable.c_str(), argv);
            failStartup(errno);
        }

        (void)::close(startupPipe[1]);
        (void)::close(inputPipe[0]);
        (void)::close(outputPipe[1]);
        (void)::close(errorPipe[1]);

        int startupError = 0;
        char* startupBytes = reinterpret_cast<char*>(&startupError);
        std::size_t startupRead = 0;
        for (;;)
        {
            const ssize_t count = ::read(
                startupPipe[0], startupBytes + startupRead,
                sizeof(startupError) - startupRead);
            if (count > 0)
            {
                startupRead += static_cast<std::size_t>(count);
                if (startupRead == sizeof(startupError))
                {
                    break;
                }
                continue;
            }
            if (count == 0)
            {
                break;
            }
            if (errno == EINTR)
            {
                continue;
            }
            startupError = errno;
            startupRead = sizeof(startupError);
            break;
        }
        (void)::close(startupPipe[0]);

        if (startupRead != 0)
        {
            int status = 0;
            while (::waitpid(pid, &status, 0) < 0 && errno == EINTR)
            {
            }
            (void)::close(inputPipe[1]);
            (void)::close(outputPipe[0]);
            (void)::close(errorPipe[0]);
            throw std::runtime_error(ErrnoMessage(startupError));
        }

        return PosixChild{
            pid, UniqueFd(inputPipe[1]), UniqueFd(outputPipe[0]), UniqueFd(errorPipe[0])};
    }

    void ReadPosixPipe(int fd, int cancelFd, const Report& report)
    {
        std::string pending;
        std::array<char, 4096> buffer{};
        std::array<pollfd, 2> descriptors{{
            pollfd{fd, POLLIN | POLLHUP | POLLERR, 0},
            pollfd{cancelFd, POLLIN | POLLHUP | POLLERR, 0}}};
        for (;;)
        {
            const int ready = ::poll(descriptors.data(), descriptors.size(), -1);
            if (ready < 0)
            {
                if (errno == EINTR)
                {
                    continue;
                }
                throw std::runtime_error(ErrnoMessage(errno));
            }
            if (descriptors[1].revents != 0)
            {
                return;
            }
            if ((descriptors[0].revents & POLLNVAL) != 0)
            {
                throw std::runtime_error(ErrnoMessage(EBADF));
            }
            if (descriptors[0].revents == 0)
            {
                continue;
            }

            const ssize_t count = ::read(fd, buffer.data(), buffer.size());
            if (count > 0)
            {
                pending.append(buffer.data(), static_cast<std::size_t>(count));
                EmitManagedLines(pending, false, report);
                continue;
            }
            if (count == 0)
            {
                break;
            }
            if (errno == EINTR)
            {
                continue;
            }
            throw std::runtime_error(ErrnoMessage(errno));
        }
        EmitManagedLines(pending, true, report);
    }

    void WritePosixInput(int fd)
    {
        static constexpr std::string_view text = "y\n\n";
        struct sigaction ignore{};
        struct sigaction previous{};
        ignore.sa_handler = SIG_IGN;
        sigemptyset(&ignore.sa_mask);
        const bool changed = ::sigaction(SIGPIPE, &ignore, &previous) == 0;

        std::size_t offset = 0;
        while (offset < text.size())
        {
            const ssize_t written = ::write(fd, text.data() + offset, text.size() - offset);
            if (written > 0)
            {
                offset += static_cast<std::size_t>(written);
                continue;
            }
            if (written < 0 && errno == EINTR)
            {
                continue;
            }
            const int error = errno;
            if (changed)
            {
                (void)::sigaction(SIGPIPE, &previous, nullptr);
            }
            if (error == EPIPE)
            {
                return;
            }
            throw std::ios_base::failure(ErrnoMessage(error));
        }
        if (changed)
        {
            (void)::sigaction(SIGPIPE, &previous, nullptr);
        }
    }
#endif

    class ReaderThread final
    {
    public:
        template <typename Reader>
        explicit ReaderThread(Reader&& reader)
        {
#if defined(_WIN32)
            _cancel.Reset(CreateEventW(nullptr, TRUE, FALSE, nullptr));
            if (!_cancel)
            {
                throw std::runtime_error(Win32Message(GetLastError()));
            }
            const HANDLE cancel = _cancel.Get();
            _thread = std::thread(
                [cancel, reader = std::forward<Reader>(reader)]() mutable
                {
                    try
                    {
                        reader(cancel);
                    }
                    catch (...)
                    {
                        // Process.BeginOutputReadLine/BeginErrorReadLine do not
                        // surface asynchronous reader failures through RunSetup.
                    }
                });
#else
            int cancelPipe[2] = {-1, -1};
            if (::pipe(cancelPipe) != 0)
            {
                throw std::runtime_error(ErrnoMessage(errno));
            }
            _cancelRead.Reset(cancelPipe[0]);
            _cancelWrite.Reset(cancelPipe[1]);
            const int cancelFd = _cancelRead.Get();
            _thread = std::thread(
                [cancelFd, reader = std::forward<Reader>(reader)]() mutable
                {
                    try
                    {
                        reader(cancelFd);
                    }
                    catch (...)
                    {
                        // Process.BeginOutputReadLine/BeginErrorReadLine do not
                        // surface asynchronous reader failures through RunSetup.
                    }
                });
#endif
        }

        ReaderThread(const ReaderThread&) = delete;
        ReaderThread& operator=(const ReaderThread&) = delete;
        ReaderThread(ReaderThread&&) = delete;
        ReaderThread& operator=(ReaderThread&&) = delete;

        ~ReaderThread()
        {
            CancelAndJoin();
        }

        void Join() noexcept
        {
            CancelAndJoin();
        }

    private:
        void CancelAndJoin() noexcept
        {
            if (!_thread.joinable())
            {
                return;
            }
#if defined(_WIN32)
            (void)SetEvent(_cancel.Get());
#else
            const char byte = 0;
            while (::write(_cancelWrite.Get(), &byte, 1) < 0 && errno == EINTR)
            {
            }
#endif
            _thread.join();
        }

#if defined(_WIN32)
        UniqueHandle _cancel;
#else
        UniqueFd _cancelRead;
        UniqueFd _cancelWrite;
#endif
        std::thread _thread;
    };

#if !defined(_WIN32) && !defined(__ANDROID__)
    class PosixWaitThread final
    {
    public:
        template <typename Action>
        explicit PosixWaitThread(Action&& action)
            : _thread(std::forward<Action>(action))
        {
        }

        PosixWaitThread(const PosixWaitThread&) = delete;
        PosixWaitThread& operator=(const PosixWaitThread&) = delete;
        PosixWaitThread(PosixWaitThread&&) = delete;
        PosixWaitThread& operator=(PosixWaitThread&&) = delete;

        ~PosixWaitThread()
        {
            CancelAndJoin();
        }

        void Join()
        {
            if (_thread.joinable())
            {
                _thread.join();
            }
        }

        void Detach() noexcept
        {
            if (_thread.joinable())
            {
                _thread.detach();
            }
        }

    private:
        void CancelAndJoin() noexcept
        {
            if (!_thread.joinable())
            {
                return;
            }
            (void)::pthread_cancel(_thread.native_handle());
            _thread.join();
        }

        std::thread _thread;
    };
#endif

}

namespace MphRead::Mods::Launcher
{
    class GameFiles::ReportWriter final : public std::streambuf
    {
    public:
        explicit ReportWriter(const Report& report) : _report(report) {}

    protected:
        int_type overflow(int_type value) override
        {
            if (traits_type::eq_int_type(value, traits_type::eof()))
            {
                return traits_type::not_eof(value);
            }
            WriteCharacter(traits_type::to_char_type(value));
            return value;
        }

        std::streamsize xsputn(const char* text, std::streamsize count) override
        {
            for (std::streamsize index = 0; index < count; ++index)
            {
                WriteCharacter(text[index]);
            }
            return count;
        }

        int sync() override
        {
            return 0;
        }

    private:
        void WriteCharacter(char value)
        {
            if (value == '\n')
            {
                while (!_line.empty() && _line.back() == '\r')
                {
                    _line.pop_back();
                }
                _report(_line);
                _line.clear();
                return;
            }
            _line.push_back(value);
        }

        const Report& _report;
        std::string _line;
    };

    std::string GameFiles::_root = AppContextBaseDirectory();
    const MphRead::Mods::Update::Version GameFiles::_minExtractVersion(0, 19, 0, 0);

    std::string GameFiles::Root()
    {
        return _root;
    }

    void GameFiles::Root(std::string value)
    {
        _root = std::move(value);
    }

    std::string GameFiles::PathsFile()
    {
        return PathCombine(_root, "paths.txt");
    }

    bool GameFiles::Ready()
    {
        return !Problem().has_value();
    }

    std::optional<std::string> GameFiles::Problem()
    {
        const std::string pathsFile = PathsFile();
        if (!FileExists(pathsFile))
        {
            return "No game files yet";
        }

        try
        {
            std::string text = ReadAllText(pathsFile);
            const std::size_t newline = text.find('\n');
            if (newline != std::string::npos)
            {
                text.resize(newline);
            }
            const std::string_view first = TrimDotNetWhitespace(text);
            const std::optional<MphRead::Mods::Update::Version> extracted
                = TryParseManagedVersion(first);
            if (!extracted.has_value() || !(*extracted >= _minExtractVersion))
            {
                return "The extracted files are from an older version -- set up again";
            }
        }
        catch (const std::ios_base::failure&)
        {
            return "paths.txt could not be read";
        }

        try
        {
            ApplyPaths();
            const std::string root = Paths::FileSystem();
            if (IsNullOrWhiteSpace(root) || !DirectoryExists(root))
            {
                return "The extracted files are missing -- set up again";
            }
        }
        catch (...)
        {
            return "No Metroid Prime Hunters files are configured";
        }
        return std::nullopt;
    }

    std::string GameFiles::Describe()
    {
        const std::optional<std::string> problem = Problem();
        if (problem.has_value())
        {
            return *problem;
        }
        try
        {
            return "Ready -- " + Paths::MphKey;
        }
        catch (...)
        {
            return "Ready";
        }
    }

    bool GameFiles::RunSetup(const std::string& romPath, const Report& report)
    {
        if (InProcessSetup())
        {
            return RunSetupHere(romPath, report);
        }

        const std::optional<std::string> executable = ProcessPath();
        if (!executable.has_value())
        {
            report("Could not find the MphRead executable.");
            return false;
        }

        try
        {
#if defined(_WIN32)
            WindowsChild child = StartChildWindows(*executable, romPath, _root);
            const HANDLE outputHandle = child.Output.Get();
            const HANDLE errorHandle = child.Error.Get();
            ReaderThread outputThread(
                [outputHandle, &report](HANDLE cancel)
                {
                    ReadWindowsPipe(outputHandle, cancel, report);
                });
            ReaderThread errorThread(
                [errorHandle, &report](HANDLE cancel)
                {
                    ReadWindowsPipe(errorHandle, cancel, report);
                });

            try
            {
                WriteWindowsInput(child.Input.Get());
            }
            catch (const std::ios_base::failure&)
            {
            }

            const DWORD wait = WaitForSingleObject(child.Process.Get(), 10U * 60U * 1000U);
            if (wait == WAIT_TIMEOUT)
            {
                KillWindowsTree(child.ProcessId);
                report("The extraction took too long and was stopped.");
                outputThread.Join();
                errorThread.Join();
                return false;
            }
            if (wait == WAIT_FAILED)
            {
                const std::string message = Win32Message(GetLastError());
                child.Output.Reset();
                child.Error.Reset();
                outputThread.Join();
                errorThread.Join();
                throw std::runtime_error(message);
            }
            outputThread.Join();
            errorThread.Join();
#elif defined(__ANDROID__)
            return RunSetupHere(romPath, report);
#else
            PosixChild child = StartChildPosix(*executable, romPath, _root);
            const int outputFd = child.Output.Get();
            const int errorFd = child.Error.Get();
            ReaderThread outputThread(
                [outputFd, &report](int cancelFd)
                {
                    ReadPosixPipe(outputFd, cancelFd, report);
                });
            ReaderThread errorThread(
                [errorFd, &report](int cancelFd)
                {
                    ReadPosixPipe(errorFd, cancelFd, report);
                });

            try
            {
                WritePosixInput(child.Input.Get());
            }
            catch (const std::ios_base::failure&)
            {
            }

            struct WaitState final
            {
                std::mutex Mutex;
                std::condition_variable Condition;
                bool Exited = false;
                std::optional<int> Error;
            };
            const std::shared_ptr<WaitState> waitState = std::make_shared<WaitState>();
            PosixWaitThread waitThread([pid = child.Pid, waitState]()
            {
                int status = 0;
                for (;;)
                {
                    const pid_t waited = ::waitpid(pid, &status, 0);
                    if (waited == pid)
                    {
                        break;
                    }
                    if (waited < 0 && errno == EINTR)
                    {
                        continue;
                    }
                    if (waited < 0)
                    {
                        waitState->Error = errno;
                        break;
                    }
                }
                {
                    std::lock_guard lock(waitState->Mutex);
                    waitState->Exited = true;
                }
                waitState->Condition.notify_one();
            });

            bool timedOut = false;
            {
                std::unique_lock lock(waitState->Mutex);
                timedOut = !waitState->Condition.wait_for(
                    lock, std::chrono::minutes(10),
                    [&]() { return waitState->Exited; });
            }
            if (timedOut)
            {
                const int stopResult = ::kill(child.Pid, SIGSTOP);
                if (stopResult != 0)
                {
                    const int stopError = errno;
                    if (stopError != ESRCH)
                    {
                        throw std::runtime_error(ErrnoMessage(stopError));
                    }
                }
                else if (::kill(child.Pid, SIGKILL) != 0 && errno != ESRCH)
                {
                    throw std::runtime_error(ErrnoMessage(errno));
                }
                report("The extraction took too long and was stopped.");
                outputThread.Join();
                errorThread.Join();
                waitThread.Detach();
                return false;
            }

            waitThread.Join();
            outputThread.Join();
            errorThread.Join();
            if (waitState->Error.has_value())
            {
                throw std::runtime_error(ErrnoMessage(*waitState->Error));
            }
#endif
        }
        catch (const std::exception& exception)
        {
            report("The extraction failed: " + std::string(exception.what()));
            return false;
        }

        const std::optional<std::string> problem = Problem();
        if (problem.has_value())
        {
            report(*problem);
            return false;
        }
        return true;
    }

    void GameFiles::ApplyPaths()
    {
        Paths::UpdatePaths();
        Paths::ChooseMphPath();
        Paths::ChooseFhPath();
    }

    bool GameFiles::InProcessSetup() noexcept
    {
#if defined(__ANDROID__)
        return true;
#else
        return false;
#endif
    }

    bool GameFiles::RunSetupHere(const std::string& romPath, const Report& report)
    {
        ReportWriter writer(report);
        const std::ios::iostate previousState = std::cout.rdstate();
        const std::ios::iostate previousExceptions = std::cout.exceptions();
        std::streambuf* const previous = std::cout.rdbuf(&writer);
        std::cout.clear();
        std::cout.exceptions(previousExceptions | std::ios::badbit);

        const auto restore = [&]()
        {
            std::cout.exceptions(std::ios::goodbit);
            std::cout.rdbuf(previous);
            std::cout.clear(previousState);
            std::cout.exceptions(previousExceptions);
        };

        try
        {
            Extract::Setup(romPath);
        }
        catch (const std::exception& exception)
        {
            try
            {
                report("The extraction failed: " + std::string(exception.what()));
            }
            catch (...)
            {
                restore();
                throw;
            }
            restore();
            return false;
        }
        catch (...)
        {
            restore();
            throw;
        }
        restore();

        const std::optional<std::string> problem = Problem();
        if (problem.has_value())
        {
            report(*problem);
            return false;
        }
        return true;
    }

}
