#include "Console.hpp"

#include "Exceptions.hpp"
#include "Encoding.hpp"

#include <algorithm>
#include <cstdio>
#include <cstdlib>
#include <iostream>
#include <optional>
#include <mutex>
#include <string>
#include <vector>

#if !defined(_WIN32)
#include <termios.h>
#include <unistd.h>
#endif

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

using ::MphRead::NativeRuntime::Utf8ToWide;

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

        // Anything still sitting in the C++ or C stream buffers was written
        // first; it has to reach the handle first.
        void FlushStreams() noexcept
        {
            std::cout.flush();
            std::cerr.flush();
            std::fflush(stdout);
            std::fflush(stderr);
        }

        void WriteOut(std::string_view value)
        {
            FlushStreams();
            const std::lock_guard<std::mutex> guard(ConsoleLock());
#if defined(_WIN32)
            WriteToHandle(STD_OUTPUT_HANDLE, value);
#else
            WriteToHandle(STDOUT_FILENO, value);
#endif
        }

        void WriteErr(std::string_view value)
        {
            FlushStreams();
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

    std::optional<std::string> EnvironmentGetVariable(const std::string& name)
    {
#if defined(_WIN32)
        // The wide API, as .NET reads it: getenv would hand back the value in
        // the ANSI code page.
        const std::wstring wideName = Wtf8ToWide(name);
        std::vector<wchar_t> buffer(128);
        for (;;)
        {
            ::SetLastError(ERROR_SUCCESS);
            const DWORD length = ::GetEnvironmentVariableW(
                wideName.c_str(), buffer.data(), static_cast<DWORD>(buffer.size()));
            if (length == 0)
            {
                if (::GetLastError() == ERROR_ENVVAR_NOT_FOUND)
                {
                    return std::nullopt;
                }
                return std::string();
            }
            if (length < buffer.size())
            {
                return WideToWtf8(std::wstring_view(buffer.data(), length));
            }
            buffer.resize(length);
        }
#else
        // .NET decodes the environment block as UTF-8.
        const char* const value = std::getenv(name.c_str());
        if (value == nullptr)
        {
            return std::nullopt;
        }
        return Utf8GetString(std::string_view(value));
#endif
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
        char buffer[256]{};
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

    namespace
    {
        void ReadKey(bool intercept)
        {
#if defined(_WIN32)
            const HANDLE input = ::GetStdHandle(STD_INPUT_HANDLE);
            DWORD mode = 0;
            if (input == nullptr || input == INVALID_HANDLE_VALUE
                || ::GetConsoleMode(input, &mode) == 0)
            {
                // Console.ReadKey with a redirected or absent input handle.
                throw System::InvalidOperationException();
            }
            const DWORD raw = intercept
                ? (mode & ~static_cast<DWORD>(ENABLE_LINE_INPUT | ENABLE_ECHO_INPUT))
                : (mode & ~static_cast<DWORD>(ENABLE_LINE_INPUT));
            ::SetConsoleMode(input, raw);
            INPUT_RECORD record{};
            DWORD read = 0;
            while (::ReadConsoleInputW(input, &record, 1, &read) != 0 && read == 1)
            {
                // A modifier on its own is not a key press to .NET either.
                if (record.EventType == KEY_EVENT && record.Event.KeyEvent.bKeyDown != 0
                    && record.Event.KeyEvent.wVirtualKeyCode != VK_SHIFT
                    && record.Event.KeyEvent.wVirtualKeyCode != VK_CONTROL
                    && record.Event.KeyEvent.wVirtualKeyCode != VK_MENU)
                {
                    break;
                }
            }
            ::SetConsoleMode(input, mode);
#else
            if (::isatty(STDIN_FILENO) == 0)
            {
                throw System::InvalidOperationException();
            }
            struct termios previous{};
            if (::tcgetattr(STDIN_FILENO, &previous) != 0)
            {
                throw System::InvalidOperationException();
            }
            struct termios raw = previous;
            raw.c_lflag &= static_cast<tcflag_t>(intercept ? ~(ICANON | ECHO) : ~ICANON);
            raw.c_cc[VMIN] = 1;
            raw.c_cc[VTIME] = 0;
            (void)::tcsetattr(STDIN_FILENO, TCSANOW, &raw);
            char value = 0;
            (void)::read(STDIN_FILENO, &value, 1);
            (void)::tcsetattr(STDIN_FILENO, TCSANOW, &previous);
#endif
        }
    }

    void ConsoleReadKey()
    {
        ReadKey(false);
    }

    void ConsoleReadKeyIntercept()
    {
        ReadKey(true);
    }

    bool ConsoleIsInputRedirected()
    {
#if defined(_WIN32)
        const HANDLE input = ::GetStdHandle(STD_INPUT_HANDLE);
        if (input == nullptr || input == INVALID_HANDLE_VALUE)
        {
            return true;
        }
        DWORD mode = 0;
        return ::GetConsoleMode(input, &mode) == 0;
#else
        return ::isatty(STDIN_FILENO) == 0;
#endif
    }

    std::optional<std::string> ConsoleReadLine()
    {
#if defined(_WIN32)
        const HANDLE input = ::GetStdHandle(STD_INPUT_HANDLE);
        DWORD mode = 0;
        if (input != nullptr && input != INVALID_HANDLE_VALUE && ::GetConsoleMode(input, &mode) != 0)
        {
            std::wstring line;
            wchar_t buffer[512];
            while (true)
            {
                DWORD read = 0;
                if (::ReadConsoleW(input, buffer, static_cast<DWORD>(std::size(buffer)), &read, nullptr) == 0)
                {
                    throw System::IO::IOException("The console could not be read.");
                }
                if (read == 0)
                {
                    if (line.empty())
                    {
                        return std::nullopt;
                    }
                    break;
                }
                line.append(buffer, read);
                if (line.back() == L'\n')
                {
                    break;
                }
            }
            while (!line.empty() && (line.back() == L'\n' || line.back() == L'\r'))
            {
                line.pop_back();
            }
            return WideToUtf8(line);
        }
#endif
        std::string line;
        if (!std::getline(std::cin, line))
        {
            if (std::cin.bad())
            {
                throw System::IO::IOException("The console could not be read.");
            }
            return std::nullopt;
        }
        if (!line.empty() && line.back() == '\r')
        {
            line.pop_back();
        }
        return Utf8GetString(line);
    }

    void ConsoleClear()
    {
#if defined(_WIN32)
        const HANDLE output = ::GetStdHandle(STD_OUTPUT_HANDLE);
        CONSOLE_SCREEN_BUFFER_INFO info{};
        if (output == nullptr || output == INVALID_HANDLE_VALUE
            || ::GetConsoleScreenBufferInfo(output, &info) == 0)
        {
            throw System::IO::IOException("The handle is invalid.");
        }
        const DWORD cells = static_cast<DWORD>(info.dwSize.X) * static_cast<DWORD>(info.dwSize.Y);
        const COORD home{ 0, 0 };
        DWORD written = 0;
        if (::FillConsoleOutputCharacterW(output, L' ', cells, home, &written) == 0
            || ::FillConsoleOutputAttribute(output, info.wAttributes, cells, home, &written) == 0
            || ::SetConsoleCursorPosition(output, home) == 0)
        {
            throw System::IO::IOException("The handle is invalid.");
        }
#else
        if (::isatty(STDOUT_FILENO) == 0)
        {
            throw System::IO::IOException("The handle is invalid.");
        }
        ConsoleWrite("\x1B[3J\x1B[H\x1B[2J");
#endif
    }
}
