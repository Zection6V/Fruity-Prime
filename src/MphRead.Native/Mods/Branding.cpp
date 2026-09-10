#include "Branding.hpp"
#include "Update/BuildVersion.hpp"

#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <limits>
#include <memory>
#include <optional>
#include <stdexcept>
#include <string>
#include <string_view>
#include <vector>

#if defined(_WIN32)
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>
#elif defined(__APPLE__)
#include <limits.h>
#include <mach-o/dyld.h>
#include <stdlib.h>
#elif defined(__FreeBSD__)
#include <limits.h>
#include <sys/param.h>
#include <sys/sysctl.h>
#include <sys/types.h>
#elif defined(__OpenBSD__)
#include <limits.h>
#include <sys/stat.h>
#include <sys/sysctl.h>
#include <sys/types.h>
#include <unistd.h>
#include <stdlib.h>
#elif defined(__sun)
#include <stdlib.h>
#elif defined(__linux__)
#include <stdlib.h>
#include <sys/auxv.h>
#elif defined(__unix__)
#include <stdlib.h>
#endif

namespace
{
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
                    const std::uint32_t codePoint
                        = 0x10000U + ((first - 0xD800U) << 10) + (second - 0xDC00U);
                    AppendWtf8(result, codePoint);
                    ++index;
                    continue;
                }
            }

            // Preserve lone UTF-16 surrogate code units losslessly as WTF-8.
            // C# strings can contain them; treating them as a conversion error
            // would add an exception that Environment.ProcessPath does not add.
            AppendWtf8(result, first);
        }
        return result;
    }

    [[nodiscard]] bool IsWindowsDirectorySeparator(char value) noexcept
    {
        return value == '\\' || value == '/';
    }

    [[nodiscard]] bool IsWindowsDriveChar(char value) noexcept
    {
        unsigned char lower = static_cast<unsigned char>(value);
        lower = static_cast<unsigned char>(lower | 0x20U);
        return lower >= static_cast<unsigned char>('a') && lower <= static_cast<unsigned char>('z');
    }

    [[nodiscard]] bool IsWindowsExtendedPath(std::string_view path) noexcept
    {
        return path.size() >= 4
            && path[0] == '\\'
            && (path[1] == '\\' || path[1] == '?')
            && path[2] == '?'
            && path[3] == '\\';
    }

    [[nodiscard]] bool IsWindowsDevicePath(std::string_view path) noexcept
    {
        return IsWindowsExtendedPath(path)
            || (path.size() >= 4
                && IsWindowsDirectorySeparator(path[0])
                && IsWindowsDirectorySeparator(path[1])
                && (path[2] == '.' || path[2] == '?')
                && IsWindowsDirectorySeparator(path[3]));
    }

    [[nodiscard]] bool IsWindowsDeviceUncPath(std::string_view path) noexcept
    {
        return path.size() >= 8
            && IsWindowsDevicePath(path)
            && IsWindowsDirectorySeparator(path[7])
            && path[4] == 'U'
            && path[5] == 'N'
            && path[6] == 'C';
    }

    [[nodiscard]] std::size_t GetWindowsRootLength(std::string_view path) noexcept
    {
        constexpr std::size_t DevicePrefixLength = 4;
        constexpr std::size_t UncPrefixLength = 2;
        constexpr std::size_t UncExtendedPrefixLength = 8;

        const std::size_t pathLength = path.size();
        std::size_t index = 0;

        const bool deviceSyntax = IsWindowsDevicePath(path);
        const bool deviceUnc = deviceSyntax && IsWindowsDeviceUncPath(path);

        if ((!deviceSyntax || deviceUnc)
            && pathLength > 0
            && IsWindowsDirectorySeparator(path[0]))
        {
            if (deviceUnc
                || (pathLength > 1 && IsWindowsDirectorySeparator(path[1])))
            {
                index = deviceUnc ? UncExtendedPrefixLength : UncPrefixLength;

                int separatorsRemaining = 2;
                while (index < pathLength
                    && (!IsWindowsDirectorySeparator(path[index]) || --separatorsRemaining > 0))
                {
                    ++index;
                }
            }
            else
            {
                index = 1;
            }
        }
        else if (deviceSyntax)
        {
            index = DevicePrefixLength;
            while (index < pathLength && !IsWindowsDirectorySeparator(path[index]))
            {
                ++index;
            }

            if (index < pathLength
                && index > DevicePrefixLength
                && IsWindowsDirectorySeparator(path[index]))
            {
                ++index;
            }
        }
        else if (pathLength >= 2
            && path[1] == ':'
            && IsWindowsDriveChar(path[0]))
        {
            index = 2;
            if (pathLength > 2 && IsWindowsDirectorySeparator(path[2]))
            {
                ++index;
            }
        }

        return index;
    }
#endif

#if !defined(_WIN32)
    void AppendUtf8Replacement(std::string& output)
    {
        output.append("\xEF\xBF\xBD", 3);
    }

    [[nodiscard]] bool IsUtf8Continuation(unsigned char value) noexcept
    {
        return (value & 0xC0U) == 0x80U;
    }

    [[nodiscard]] std::string DecodeUtf8LikeDotNet(std::string_view input)
    {
        std::string output;
        output.reserve(input.size());

        std::size_t index = 0;
        while (index < input.size())
        {
            const unsigned char first = static_cast<unsigned char>(input[index]);
            if (first <= 0x7FU)
            {
                output.push_back(static_cast<char>(first));
                ++index;
                continue;
            }

            if (first < 0xC2U || first > 0xF4U)
            {
                AppendUtf8Replacement(output);
                ++index;
                continue;
            }

            if (index + 1 >= input.size())
            {
                AppendUtf8Replacement(output);
                break;
            }

            const unsigned char second = static_cast<unsigned char>(input[index + 1]);
            if (!IsUtf8Continuation(second))
            {
                AppendUtf8Replacement(output);
                ++index;
                continue;
            }

            if ((first == 0xE0U && second < 0xA0U)
                || (first == 0xEDU && second >= 0xA0U)
                || (first == 0xF0U && second < 0x90U)
                || (first == 0xF4U && second > 0x8FU))
            {
                AppendUtf8Replacement(output);
                ++index;
                continue;
            }

            if (first <= 0xDFU)
            {
                output.append(input.substr(index, 2));
                index += 2;
                continue;
            }

            if (index + 2 >= input.size())
            {
                AppendUtf8Replacement(output);
                break;
            }

            const unsigned char third = static_cast<unsigned char>(input[index + 2]);
            if (!IsUtf8Continuation(third))
            {
                AppendUtf8Replacement(output);
                index += 2;
                continue;
            }

            if (first <= 0xEFU)
            {
                output.append(input.substr(index, 3));
                index += 3;
                continue;
            }

            if (index + 3 >= input.size())
            {
                AppendUtf8Replacement(output);
                break;
            }

            const unsigned char fourth = static_cast<unsigned char>(input[index + 3]);
            if (!IsUtf8Continuation(fourth))
            {
                AppendUtf8Replacement(output);
                index += 3;
                continue;
            }

            output.append(input.substr(index, 4));
            index += 4;
        }

        return output;
    }
#endif

#if defined(__APPLE__) || defined(__OpenBSD__) || defined(__sun) || defined(__linux__) \
    || (defined(__unix__) && !defined(__EMSCRIPTEN__) && !defined(__wasi__))
    [[nodiscard]] std::optional<std::string> RealPath(const char* path)
    {
        std::unique_ptr<char, decltype(&std::free)> resolved(realpath(path, nullptr), &std::free);
        if (!resolved)
        {
            return std::nullopt;
        }
        return std::string(resolved.get());
    }
#endif

    [[nodiscard]] std::optional<std::string> ReadProcessPath()
    {
#if defined(_WIN32)
        std::vector<wchar_t> buffer(260);
        for (;;)
        {
            const DWORD length = GetModuleFileNameW(nullptr, buffer.data(), static_cast<DWORD>(buffer.size()));
            if (length == 0)
            {
                throw std::runtime_error("Could not get the process path.");
            }
            if (length < buffer.size())
            {
                return Utf8FromWide(buffer.data(), static_cast<std::size_t>(length));
            }
            if (buffer.size() > static_cast<std::size_t>(std::numeric_limits<DWORD>::max()) / 2U)
            {
                throw std::length_error("Process path is too long.");
            }
            buffer.resize(buffer.size() * 2U);
        }
#elif defined(__APPLE__)
        char path[PATH_MAX];
        std::uint32_t length = PATH_MAX;
        if (_NSGetExecutablePath(path, &length) != 0)
        {
            return std::nullopt;
        }
        return RealPath(path);
#elif defined(__FreeBSD__)
        static const int name[] = {CTL_KERN, KERN_PROC, KERN_PROC_PATHNAME, -1};
        char path[PATH_MAX];
        std::size_t length = sizeof(path);
        if (sysctl(name, 4, path, &length, nullptr, 0) != 0)
        {
            return std::nullopt;
        }
        return std::string(path);
#elif defined(__OpenBSD__)
        const int name[] = {CTL_KERN, KERN_PROC_ARGS, getpid(), KERN_PROC_ARGV};
        std::size_t length = 0;
        if (sysctl(name, 4, nullptr, &length, nullptr, 0) != 0 || length == 0)
        {
            return std::nullopt;
        }
        std::vector<unsigned char> buffer(length);
        if (sysctl(name, 4, buffer.data(), &length, nullptr, 0) != 0)
        {
            return std::nullopt;
        }
        const char* executable = nullptr;
        std::memcpy(&executable, buffer.data(), sizeof(executable));
        if (executable == nullptr)
        {
            return std::nullopt;
        }
        if (std::string_view(executable).find('/') == std::string_view::npos)
        {
            const char* pathEnvironment = std::getenv("PATH");
            while (pathEnvironment != nullptr && *pathEnvironment != '\0')
            {
                const std::size_t segmentLength = std::strcspn(pathEnvironment, ":");
                char candidate[PATH_MAX];
                const int written = std::snprintf(
                    candidate, sizeof(candidate), "%.*s/%s",
                    static_cast<int>(segmentLength), pathEnvironment, executable);
                if (written >= 0 && static_cast<std::size_t>(written) < sizeof(candidate))
                {
                    struct stat info{};
                    if (stat(candidate, &info) == 0 && S_ISREG(info.st_mode))
                    {
                        return RealPath(candidate);
                    }
                }
                pathEnvironment += segmentLength;
                if (*pathEnvironment == ':')
                {
                    ++pathEnvironment;
                }
            }
        }
        return RealPath(executable);
#elif defined(__sun)
        const char* path = getexecname();
        return path == nullptr ? std::nullopt : RealPath(path);
#elif defined(__EMSCRIPTEN__)
        return std::string("/");
#elif defined(__wasi__)
        const char* coreRoot = std::getenv("CORE_ROOT");
        if (coreRoot == nullptr || *coreRoot == '\0')
        {
            return std::string("/");
        }
        return std::string(coreRoot) + "/corerun";
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

    [[nodiscard]] const std::optional<std::string>& ProcessPath()
    {
        static const std::optional<std::string> processPath = []
        {
            std::optional<std::string> path = ReadProcessPath();
#if !defined(_WIN32)
            if (path.has_value())
            {
                *path = DecodeUtf8LikeDotNet(*path);
            }
#endif
            if (path.has_value() && path->empty())
            {
                path.reset();
            }
            return path;
        }();
        return processPath;
    }

    [[nodiscard]] std::string GetFileNameWithoutExtension(std::string_view path)
    {
#if defined(_WIN32)
        const std::size_t root = GetWindowsRootLength(path);
        const std::size_t separator = path.find_last_of("/\\");
        const std::size_t start
            = separator == std::string_view::npos || separator < root
            ? root
            : separator + 1;
#else
        const std::size_t separator = path.find_last_of('/');
        const std::size_t start
            = separator == std::string_view::npos
            ? 0
            : separator + 1;
#endif

        std::string_view name = path.substr(start);
        const std::size_t period = name.find_last_of('.');
        if (period != std::string_view::npos)
        {
            name = name.substr(0, period);
        }
        return std::string(name);
    }
}

namespace MphRead
{
    namespace Mods
    {
        std::string Branding::Executable()
        {
            const std::optional<std::string>& path = ProcessPath();
            if (!path.has_value())
            {
                return std::string(FileName);
            }
            std::string name = GetFileNameWithoutExtension(*path);
            return name.empty() ? std::string(FileName) : name;
        }

        std::string Branding::NameAndVersion()
        {
            const bool isRelease = Update::BuildVersion::IsRelease();
            const std::string display(Update::BuildVersion::Display());
            if (isRelease)
            {
                return std::string(Name) + " " + display;
            }
            return std::string(Name) + " (" + display + ")";
        }
    }
}
