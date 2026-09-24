#include "Console.hpp"

#include "../Mods/Platform/AppPaths.hpp"
#include "NativeRuntime/System/AtomicSharedPtr.hpp"
#include "NativeRuntime/System/Console.hpp"
#include "NativeRuntime/System/Exceptions.hpp"
#include "../NativeRuntime/System/IO.hpp"
#include "../NativeRuntime/System/Encoding.hpp"

#include <atomic>
#include <clocale>
#include <cstdint>
#include <cstdlib>
#include <exception>
#include <filesystem>
#include <limits>
#include <locale>
#include <memory>
#include <optional>
#include <stdexcept>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

#if defined(_WIN32)
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <locale.h>
#include <windows.h>
#elif defined(__APPLE__)
#include <locale.h>
#include <mach-o/dyld.h>
#include <stdlib.h>
#elif defined(__FreeBSD__)
#include <locale.h>
#include <sys/param.h>
#include <sys/sysctl.h>
#include <sys/types.h>
#elif defined(__linux__)
#include <locale.h>
#include <stdlib.h>
#include <sys/auxv.h>
#elif defined(__unix__)
#include <locale.h>
#include <stdlib.h>
#endif

using ::MphRead::NativeRuntime::Utf8GetString;
using ::MphRead::NativeRuntime::WideToWtf8;

namespace
{
#if defined(_WIN32)
#else
    [[nodiscard]] bool IsUtf8Continuation(unsigned char value) noexcept
    {
        return (value & 0xC0U) == 0x80U;
    }

#endif

    [[nodiscard]] std::string CurrentDirectory()
    {
        const std::filesystem::path path = std::filesystem::current_path();
#if defined(_WIN32)
        return WideToWtf8(path.native());
#else
        return Utf8GetString(path.native());
#endif
    }

    void SetInvariantCultureForCurrentThread()
    {
#if defined(_WIN32)
        // CultureInfo.CurrentCulture is per-thread. Configure the CRT locale
        // per-thread when this CRT supports it; never fall back to changing
        // the process-global locale.
        if (::_configthreadlocale(_ENABLE_PER_THREAD_LOCALE) != -1)
        {
            (void)::setlocale(LC_ALL, "C");
        }
#elif defined(LC_ALL_MASK)
        // POSIX locale_t/uselocale is the thread-scoped analogue of managed
        // CurrentCulture. The "C" locale is the native invariant locale.
        static locale_t invariantLocale = ::newlocale(LC_ALL_MASK, "C", nullptr);
        if (invariantLocale != static_cast<locale_t>(0))
        {
            (void)::uselocale(invariantLocale);
        }
#else
        // No portable process-independent locale hook exists here. Retain a
        // thread-local invariant object rather than introducing global state.
        static const std::locale invariantCulture = std::locale::classic();
        (void)invariantCulture;
#endif
    }

    struct ConsoleSetupState final
    {
        ::MphRead::NativeRuntime::AtomicSharedPtr<const std::string> LaunchDirectory;

        explicit ConsoleSetupState(std::string launchDirectory)
            : LaunchDirectory(std::make_shared<const std::string>(std::move(launchDirectory)))
        {
        }
    };

    struct ConsoleSetupStateHolder final
    {
        std::optional<ConsoleSetupState> Value;
        std::exception_ptr InitializationException;

        ConsoleSetupStateHolder() noexcept
        {
            try
            {
                Value.emplace(CurrentDirectory());
            }
            catch (...)
            {
                InitializationException = std::current_exception();
            }
        }
    };

    ConsoleSetupState& State()
    {
        static ConsoleSetupStateHolder holder;
        if (holder.InitializationException)
        {
            std::rethrow_exception(holder.InitializationException);
        }
        return *holder.Value;
    }
}

namespace MphRead
{
    std::string ConsoleSetup::LaunchDirectory()
    {
        const std::shared_ptr<const std::string> value
            = State().LaunchDirectory.load(std::memory_order_relaxed);
        return *value;
    }

    void ConsoleSetup::PauseIfInteractive()
    {
        if (::MphRead::NativeRuntime::ConsoleIsInputRedirected())
        {
            return;
        }
        try
        {
            ::MphRead::NativeRuntime::ConsoleReadKey();
        }
        catch (const System::InvalidOperationException&)
        {
            // No console to read from. The message above it was still
            // printed, which is the part that mattered.
        }
        catch (const System::IO::IOException&)
        {
        }
    }

    void ConsoleSetup::Run()
    {
        ConsoleSetupState& state = State();

        SetInvariantCultureForCurrentThread();
        state.LaunchDirectory.store(
            std::make_shared<const std::string>(CurrentDirectory()),
            std::memory_order_relaxed);
        // Upstream reads/writes settings, saves and extraction output relative
        // to cwd. On macOS this must be writable and outside the signed app.
        ::MphRead::Mods::Platform::AppPaths::PrepareUserData();
        const std::string userData = ::MphRead::Mods::Platform::AppPaths::UserDataDirectory();
        std::filesystem::current_path(
            std::filesystem::path(std::u8string(userData.begin(), userData.end())));

#if defined(_WIN32)
        constexpr int StdOutputHandle = -11;
        constexpr DWORD EnableVirtualTerminalProcessing = 0x0004U;

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
#if defined(_WIN32)
        return static_cast<std::uint32_t>(::GetLastError());
#else
        throw std::runtime_error("kernel32.dll is unavailable on this platform.");
#endif
    }
}
