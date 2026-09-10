#include "Branding.hpp"
#include "Update/BuildVersion.hpp"

#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <limits>
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
    [[nodiscard]] std::string Utf8FromWide(const wchar_t* value, std::size_t length)
    {
        if (length == 0)
        {
            return {};
        }
        if (length > static_cast<std::size_t>(std::numeric_limits<int>::max()))
        {
            throw std::length_error("Process path is too long.");
        }
        const int inputLength = static_cast<int>(length);
        const int required = WideCharToMultiByte(CP_UTF8, WC_ERR_INVALID_CHARS, value, inputLength, nullptr, 0, nullptr, nullptr);
        if (required == 0)
        {
            throw std::runtime_error("Could not convert the process path to UTF-8.");
        }
        std::string result(static_cast<std::size_t>(required), '\0');
        if (WideCharToMultiByte(CP_UTF8, WC_ERR_INVALID_CHARS, value, inputLength, result.data(), required, nullptr, nullptr) == 0)
        {
            throw std::runtime_error("Could not convert the process path to UTF-8.");
        }
        return result;
    }
#endif

#if !defined(_WIN32)
    [[nodiscard]] std::optional<std::string> RealPath(const char* path)
    {
        char* resolved = realpath(path, nullptr);
        if (resolved == nullptr)
        {
            return std::nullopt;
        }
        std::string result(resolved);
        std::free(resolved);
        return result;
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
        std::vector<char> buffer(length);
        if (sysctl(name, 4, buffer.data(), &length, nullptr, 0) != 0)
        {
            return std::nullopt;
        }
        const char* executable = *reinterpret_cast<char* const*>(buffer.data());
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
        const std::size_t separator = path.find_last_of("/\\");
#else
        const std::size_t separator = path.find_last_of('/');
#endif
        std::string_view name = separator == std::string_view::npos ? path : path.substr(separator + 1);
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
