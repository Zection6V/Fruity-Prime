#include "ConsoleWindow.hpp"

#include <cstddef>
#include <cstdint>
#include <exception>
#include <ios>
#include <iostream>
#include <string_view>

#if defined(_WIN32)
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <cstdio>
#include <io.h>
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
                // The other operand is ASCII. .NET's OrdinalIgnoreCase scalar
                // path does not consider a non-ASCII UTF-16 code unit equal to
                // an ASCII one.
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
    constexpr DWORD EnableVirtualTerminalProcessing = 0x0004U;

    [[nodiscard]] HANDLE GetStdHandleWithLastError(int handle) noexcept
    {
        // DllImport(SetLastError = true) clears the native last-error value
        // before the call on the .NET runtime targeted by the C# project.
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

    [[nodiscard]] bool IsHandleRedirected(int stdHandle) noexcept
    {
        const HANDLE handle = ::GetStdHandle(static_cast<DWORD>(stdHandle));
        const DWORD fileType = ::GetFileType(handle);
        if ((fileType & FILE_TYPE_CHAR) != FILE_TYPE_CHAR)
        {
            return true;
        }

        DWORD mode = 0;
        return ::GetConsoleMode(handle, &mode) == FALSE;
    }

    [[nodiscard]] bool IsOutputRedirected() noexcept
    {
        return IsHandleRedirected(StdOutputHandle);
    }

    [[nodiscard]] bool IsInputRedirected() noexcept
    {
        return IsHandleRedirected(StdInputHandle);
    }

    void RebindFile(std::FILE* destination, const char* device, const char* mode)
    {
        std::FILE* source = std::fopen(device, mode);
        if (source == nullptr)
        {
            throw std::ios_base::failure("Unable to open the console stream.");
        }

        const int sourceDescriptor = ::_fileno(source);
        const int destinationDescriptor = ::_fileno(destination);
        if (sourceDescriptor == -1 || destinationDescriptor == -1
            || ::_dup2(sourceDescriptor, destinationDescriptor) != 0)
        {
            std::fclose(source);
            throw std::ios_base::failure("Unable to bind the console stream.");
        }

        std::fclose(source);
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
                RebindFile(stdout, "CONOUT$", "w");
                std::cout.clear();
                std::cout << std::unitbuf;

                RebindFile(stderr, "CONOUT$", "w");
                std::cerr.clear();
                std::cerr << std::unitbuf;

                RebindFile(stdin, "CONIN$", "r");
                std::cin.clear();

                // The escape-sequence mode ConsoleSetup asks for, re-applied:
                // it ran before this console existed.
                const HANDLE handle = GetStdHandleWithLastError(StdOutputHandle);
                DWORD mode = 0;
                if (::GetConsoleMode(handle, &mode) != FALSE)
                {
                    (void)::SetConsoleMode(
                        handle, mode | EnableVirtualTerminalProcessing);
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
