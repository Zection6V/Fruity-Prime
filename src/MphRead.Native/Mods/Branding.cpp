#include "Branding.hpp"
#include "Update/BuildVersion.hpp"
#include "../NativeRuntime/System/Encoding.hpp"
#include "../NativeRuntime/System/IO.hpp"
#include "../NativeRuntime/System/Runtime.hpp"

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
#include <system_error>
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

using ::MphRead::NativeRuntime::EnvironmentProcessPath;
using ::MphRead::NativeRuntime::PathGetFileNameWithoutExtension;
using ::MphRead::NativeRuntime::Utf8GetString;
using ::MphRead::NativeRuntime::WideToWtf8;

namespace
{
#if defined(_WIN32)
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
    [[nodiscard]] bool IsUtf8Continuation(unsigned char value) noexcept
    {
        return (value & 0xC0U) == 0x80U;
    }

#endif

#if defined(__APPLE__) || defined(__OpenBSD__) || defined(__sun) || defined(__linux__) \
    || (defined(__unix__) && !defined(__EMSCRIPTEN__) && !defined(__wasi__))
#endif

}

namespace MphRead
{
    namespace Mods
    {
        std::string Branding::Executable()
        {
            const std::optional<std::string> path = EnvironmentProcessPath();
            if (!path.has_value())
            {
                return std::string(FileName);
            }
            std::string name = PathGetFileNameWithoutExtension(*path);
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
