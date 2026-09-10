#include "Console.hpp"

#include <filesystem>
#include <locale>
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
#include <mach-o/dyld.h>
#include <stdlib.h>
#elif defined(__FreeBSD__)
#include <sys/param.h>
#include <sys/sysctl.h>
#include <sys/types.h>
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

    [[nodiscard]] std::string Utf8FromWide(std::wstring_view value)
    {
        static_assert(sizeof(wchar_t) == sizeof(std::uint16_t));

        std::string result;
        result.reserve(value.size());
        for (std::size_t index = 0; index < value.size(); ++index)
        {
            const std::uint32_t first = static_cast<std::uint16_t>(value[index]);
            if (first >= 0xD800U && first <= 0xDBFFU && index + 1 < value.size())
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
            AppendWtf8(result, first);
        }
        return result;
    }
#else
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

    [[nodiscard]] std::string CurrentDirectory()
    {
        const std::filesystem::path path = std::filesystem::current_path();
#if defined(_WIN32)
        return Utf8FromWide(path.native());
#else
        return DecodeUtf8LikeDotNet(path.native());
#endif
    }

    [[nodiscard]] std::optional<std::filesystem::path> ProcessPath()
    {
#if defined(_WIN32)
        std::vector<wchar_t> buffer(260);
        for (;;)
        {
            const DWORD length = ::GetModuleFileNameW(
                nullptr, buffer.data(), static_cast<DWORD>(buffer.size()));
            if (length == 0)
            {
                return std::nullopt;
            }
            if (length < buffer.size())
            {
                return std::filesystem::path(std::wstring(buffer.data(), length));
            }
            if (buffer.size() > static_cast<std::size_t>(std::numeric_limits<DWORD>::max()) / 2U)
            {
                return std::nullopt;
            }
            buffer.resize(buffer.size() * 2U);
        }
#elif defined(__APPLE__)
        std::uint32_t size = 1024;
        std::vector<char> buffer(size);
        while (_NSGetExecutablePath(buffer.data(), &size) != 0)
        {
            buffer.resize(size);
        }
        std::unique_ptr<char, decltype(&std::free)> resolved(
            realpath(buffer.data(), nullptr), &std::free);
        if (!resolved)
        {
            return std::nullopt;
        }
        return std::filesystem::path(DecodeUtf8LikeDotNet(resolved.get()));
#elif defined(__FreeBSD__)
        static const int name[] = {CTL_KERN, KERN_PROC, KERN_PROC_PATHNAME, -1};
        std::size_t size = 0;
        if (sysctl(name, 4, nullptr, &size, nullptr, 0) != 0 || size == 0)
        {
            return std::nullopt;
        }
        std::vector<char> buffer(size);
        if (sysctl(name, 4, buffer.data(), &size, nullptr, 0) != 0 || size == 0)
        {
            return std::nullopt;
        }
        const std::size_t length = buffer[size - 1] == '\0' ? size - 1 : size;
        return std::filesystem::path(DecodeUtf8LikeDotNet(std::string_view(buffer.data(), length)));
#elif defined(__linux__)
        std::unique_ptr<char, decltype(&std::free)> resolved(
            realpath("/proc/self/exe", nullptr), &std::free);
        if (resolved)
        {
            return std::filesystem::path(DecodeUtf8LikeDotNet(resolved.get()));
        }
#if defined(AT_EXECFN)
        const auto executable = reinterpret_cast<const char*>(getauxval(AT_EXECFN));
        if (executable != nullptr)
        {
            resolved.reset(realpath(executable, nullptr));
            if (resolved)
            {
                return std::filesystem::path(DecodeUtf8LikeDotNet(resolved.get()));
            }
        }
#endif
        return std::nullopt;
#elif defined(__unix__)
        std::unique_ptr<char, decltype(&std::free)> resolved(
            realpath("/proc/curproc/exe", nullptr), &std::free);
        if (!resolved)
        {
            return std::nullopt;
        }
        return std::filesystem::path(DecodeUtf8LikeDotNet(resolved.get()));
#else
        return std::nullopt;
#endif
    }

    [[nodiscard]] std::filesystem::path BaseDirectory()
    {
        const std::optional<std::filesystem::path> processPath = ProcessPath();
        if (!processPath.has_value())
        {
            throw std::runtime_error("Could not determine AppDomain.CurrentDomain.BaseDirectory.");
        }
        return processPath->parent_path();
    }

    struct ConsoleSetupState final
    {
        std::string LaunchDirectory = CurrentDirectory();
    };

    ConsoleSetupState& State()
    {
        static ConsoleSetupState state;
        return state;
    }
}

namespace MphRead
{
    std::string ConsoleSetup::LaunchDirectory()
    {
        return State().LaunchDirectory;
    }

    void ConsoleSetup::Run()
    {
        ConsoleSetupState& state = State();

        std::locale::global(std::locale::classic());
        state.LaunchDirectory = CurrentDirectory();
        std::filesystem::current_path(BaseDirectory());

#if defined(_WIN32)
        constexpr int StdOutputHandle = -11;
        constexpr DWORD EnableVirtualTerminalProcessing = 0x0004U;

        // DllImport(SetLastError = true) clears the native last-error slot before
        // the call on current .NET runtimes, then preserves the result for managed
        // retrieval. Preserve the native observable part of that behavior here.
        ::SetLastError(ERROR_SUCCESS);
        HANDLE stdOut = ::GetStdHandle(static_cast<DWORD>(StdOutputHandle));
        DWORD outConsoleMode = 0;
        ::GetConsoleMode(stdOut, &outConsoleMode);
        outConsoleMode |= EnableVirtualTerminalProcessing;
        ::SetConsoleMode(stdOut, outConsoleMode);
#endif
    }

    std::uint32_t ConsoleSetup::GetLastError()
    {
        (void)State();
#if defined(_WIN32)
        return static_cast<std::uint32_t>(::GetLastError());
#else
        throw std::runtime_error("kernel32.dll is unavailable on this platform.");
#endif
    }
}
