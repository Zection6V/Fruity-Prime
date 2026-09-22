#include "Console.hpp"

#include "Exceptions.hpp"

#include <algorithm>
#include <cstdio>
#include <mutex>
#include <string>
#include <vector>

#if defined(_WIN32)
#ifndef NOMINMAX
#define NOMINMAX
#endif
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>
#else
#include <climits>
#include <unistd.h>
#endif

namespace MphRead::NativeRuntime
{
    namespace
    {
        // Console.Out and Console.Error are each a synchronized TextWriter, so
        // one write cannot be cut in half by another thread's.
        std::mutex& ConsoleLock()
        {
            static std::mutex lock;
            return lock;
        }

#if defined(_WIN32)
        // ConsolePal.Windows: a handle that is a console takes the UTF-16
        // path, and anything else (a pipe, a file) takes the encoded one.
        [[nodiscard]] bool IsConsoleHandle(HANDLE handle) noexcept
        {
            if (handle == nullptr || handle == INVALID_HANDLE_VALUE)
            {
                return false;
            }
            DWORD mode = 0;
            return GetConsoleMode(handle, &mode) != FALSE;
        }

        [[nodiscard]] std::wstring Utf8ToWide(std::string_view value)
        {
            if (value.empty())
            {
                return std::wstring();
            }
            const int length = MultiByteToWideChar(
                CP_UTF8, 0, value.data(), static_cast<int>(value.size()), nullptr, 0);
            if (length <= 0)
            {
                return std::wstring();
            }
            std::wstring result(static_cast<std::size_t>(length), L'\0');
            MultiByteToWideChar(
                CP_UTF8, 0, value.data(), static_cast<int>(value.size()),
                result.data(), length);
            return result;
        }

        void WriteToHandle(DWORD which, std::string_view value)
        {
            const HANDLE handle = GetStdHandle(which);
            if (handle == nullptr || handle == INVALID_HANDLE_VALUE)
            {
                // Console.Out with no handle behind it is Stream.Null: the
                // write is accepted and goes nowhere.
                return;
            }
            if (IsConsoleHandle(handle))
            {
                const std::wstring wide = Utf8ToWide(value);
                std::size_t written = 0;
                while (written < wide.size())
                {
                    DWORD chunk = 0;
                    const DWORD count = static_cast<DWORD>(
                        std::min<std::size_t>(wide.size() - written, 0x3FFF));
                    if (WriteConsoleW(handle, wide.data() + written, count, &chunk, nullptr)
                        == FALSE || chunk == 0)
                    {
                        return;
                    }
                    written += chunk;
                }
                return;
            }
            std::size_t written = 0;
            while (written < value.size())
            {
                DWORD chunk = 0;
                if (WriteFile(
                        handle, value.data() + written,
                        static_cast<DWORD>(value.size() - written), &chunk, nullptr) == FALSE
                    || chunk == 0)
                {
                    return;
                }
                written += chunk;
            }
        }
#else
        void WriteToHandle(int fd, std::string_view value)
        {
            std::size_t written = 0;
            while (written < value.size())
            {
                const ssize_t chunk = ::write(fd, value.data() + written, value.size() - written);
                if (chunk <= 0)
                {
                    return;
                }
                written += static_cast<std::size_t>(chunk);
            }
        }
#endif

        void WriteOut(std::string_view value)
        {
            const std::lock_guard<std::mutex> guard(ConsoleLock());
#if defined(_WIN32)
            WriteToHandle(STD_OUTPUT_HANDLE, value);
#else
            WriteToHandle(STDOUT_FILENO, value);
#endif
        }

        void WriteErr(std::string_view value)
        {
            const std::lock_guard<std::mutex> guard(ConsoleLock());
#if defined(_WIN32)
            WriteToHandle(STD_ERROR_HANDLE, value);
#else
            WriteToHandle(STDERR_FILENO, value);
#endif
        }
    }

    std::string_view EnvironmentNewLine() noexcept
    {
#if defined(_WIN32)
        return "\r\n";
#else
        return "\n";
#endif
    }

    void ConsoleWrite(std::string_view value)
    {
        if (value.empty())
        {
            return;
        }
        WriteOut(value);
    }

    void ConsoleWriteLine(std::string_view value)
    {
        // TextWriter.WriteLine(string) writes the value and the terminator as
        // one call, which is what keeps a line whole between threads.
        std::string text(value);
        text.append(EnvironmentNewLine());
        WriteOut(text);
    }

    void ConsoleWriteLineNullable(const std::optional<std::string>& value)
    {
        if (!value.has_value())
        {
            ConsoleWriteLine();
            return;
        }
        ConsoleWriteLine(std::string_view(*value));
    }

    void ConsoleWriteLine()
    {
        WriteOut(EnvironmentNewLine());
    }

    void ConsoleErrorWrite(std::string_view value)
    {
        if (value.empty())
        {
            return;
        }
        WriteErr(value);
    }

    void ConsoleErrorWriteLine(std::string_view value)
    {
        std::string text(value);
        text.append(EnvironmentNewLine());
        WriteErr(text);
    }

    void ConsoleFlush()
    {
        // The standard handles are written unbuffered above, so there is
        // nothing held back; the C streams may still hold output written
        // through them.
        std::fflush(stdout);
        std::fflush(stderr);
    }

    std::string EnvironmentMachineName()
    {
#if defined(_WIN32)
        wchar_t buffer[MAX_COMPUTERNAME_LENGTH + 1]{};
        DWORD length = MAX_COMPUTERNAME_LENGTH + 1;
        if (GetComputerNameW(buffer, &length) == FALSE)
        {
            throw System::InvalidOperationException();
        }
        const int size = WideCharToMultiByte(
            CP_UTF8, 0, buffer, static_cast<int>(length), nullptr, 0, nullptr, nullptr);
        std::string result(static_cast<std::size_t>(size), '\0');
        WideCharToMultiByte(
            CP_UTF8, 0, buffer, static_cast<int>(length),
            result.data(), size, nullptr, nullptr);
        return result;
#else
        char buffer[HOST_NAME_MAX + 1]{};
        if (::gethostname(buffer, sizeof(buffer) - 1) != 0)
        {
            throw System::InvalidOperationException();
        }
        std::string result(buffer);
        // Environment.MachineName is the host name up to the first dot.
        const std::size_t dot = result.find('.');
        if (dot != std::string::npos)
        {
            result.erase(dot);
        }
        return result;
#endif
    }
}
