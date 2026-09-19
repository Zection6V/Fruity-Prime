#include "Updater.hpp"
#include "BuildVersion.hpp"

#include <atomic>
#include <cerrno>
#include <chrono>
#include <cstdlib>
#include <limits>
#include <mutex>
#include <new>
#include <optional>
#include <stdexcept>
#include <string>
#include <thread>
#include <utility>

#ifdef _WIN32
#define NOMINMAX
#include <windows.h>
#include <shellapi.h>
#else
#include <spawn.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>
extern char** environ;
#endif

namespace MphRead::Mods::Update
{
    namespace
    {
        constexpr std::int64_t UnixEpochTicks = 621'355'968'000'000'000LL;
        constexpr std::int64_t MaxDateTimeTicks = 3'155'378'975'999'999'999LL;

        struct State final
        {
            std::atomic_bool Disabled{false};
            std::mutex AvailableGate;
            std::optional<UpdateInfo> Available;
            std::atomic_bool Checked{false};
        };

        [[nodiscard]] State& GetState()
        {
            alignas(State) static unsigned char storage[sizeof(State)];
            static State* state = ::new (static_cast<void*>(storage)) State();
            return *state;
        }

        [[nodiscard]] const std::string& RequireString(
            const Detail::InitOnlyProperty<std::optional<std::string>>& value)
        {
            const std::optional<std::string>& string = value.Get();
            if (!string.has_value())
            {
                throw NullReferenceException();
            }
            return *string;
        }

        [[nodiscard]] std::string InterpolateString(
            const Detail::InitOnlyProperty<std::optional<std::string>>& value)
        {
            const std::optional<std::string>& string = value.Get();
            return string.has_value() ? *string : std::string{};
        }

        [[nodiscard]] std::int64_t UtcNowTicks()
        {
            using TimeSpan = Updater::TimeSpan;
            const auto sinceUnix = std::chrono::system_clock::now().time_since_epoch();
            const std::int64_t unixTicks = std::chrono::duration_cast<TimeSpan>(sinceUnix).count();
            return UnixEpochTicks + unixTicks;
        }

        [[nodiscard]] std::int64_t AddDateTimeTicks(
            std::int64_t ticks, Updater::TimeSpan value)
        {
            const std::int64_t delta = value.count();
            if (delta > 0)
            {
                if (ticks > MaxDateTimeTicks - delta)
                {
                    throw std::out_of_range("value");
                }
            }
            else if (delta < 0)
            {
                if (delta == std::numeric_limits<std::int64_t>::min()
                    || ticks < -delta)
                {
                    throw std::out_of_range("value");
                }
            }
            return ticks + delta;
        }

#ifdef _WIN32
        using ShellExecuteExWFunction = BOOL (WINAPI*)(SHELLEXECUTEINFOW*);
        using CoGetApartmentTypeFunction = HRESULT (WINAPI*)(int*, int*);
        using CoInitializeExFunction = HRESULT (WINAPI*)(void*, DWORD);
        using CoUninitializeFunction = void (WINAPI*)();

        [[nodiscard]] FARPROC LoadProcedure(const wchar_t* moduleName, const char* procedureName)
        {
            HMODULE module = ::GetModuleHandleW(moduleName);
            if (module == nullptr)
            {
                module = ::LoadLibraryW(moduleName);
            }
            return module == nullptr ? nullptr : ::GetProcAddress(module, procedureName);
        }

        [[nodiscard]] ShellExecuteExWFunction ShellExecuteExWApi()
        {
            static auto function = reinterpret_cast<ShellExecuteExWFunction>(
                LoadProcedure(L"shell32.dll", "ShellExecuteExW"));
            return function;
        }

        [[nodiscard]] CoGetApartmentTypeFunction CoGetApartmentTypeApi()
        {
            static auto function = reinterpret_cast<CoGetApartmentTypeFunction>(
                LoadProcedure(L"ole32.dll", "CoGetApartmentType"));
            return function;
        }

        [[nodiscard]] CoInitializeExFunction CoInitializeExApi()
        {
            static auto function = reinterpret_cast<CoInitializeExFunction>(
                LoadProcedure(L"ole32.dll", "CoInitializeEx"));
            return function;
        }

        [[nodiscard]] CoUninitializeFunction CoUninitializeApi()
        {
            static auto function = reinterpret_cast<CoUninitializeFunction>(
                LoadProcedure(L"ole32.dll", "CoUninitialize"));
            return function;
        }

        [[nodiscard]] bool CurrentThreadIsSta() noexcept
        {
            CoGetApartmentTypeFunction getApartmentType = CoGetApartmentTypeApi();
            if (getApartmentType == nullptr)
            {
                return false;
            }
            int apartmentType = -1;
            int qualifier = 0;
            const HRESULT result = getApartmentType(&apartmentType, &qualifier);
            return SUCCEEDED(result) && (apartmentType == 0 || apartmentType == 3);
        }

        [[nodiscard]] std::optional<std::wstring> Utf8ToWide(const std::string& text)
        {
            if (text.size() > static_cast<std::size_t>(std::numeric_limits<int>::max()))
            {
                return std::nullopt;
            }
            const int byteCount = static_cast<int>(text.size());
            const int length = ::MultiByteToWideChar(
                CP_UTF8, MB_ERR_INVALID_CHARS, text.data(), byteCount, nullptr, 0);
            if (length <= 0)
            {
                return std::nullopt;
            }
            std::wstring result(static_cast<std::size_t>(length), L'\0');
            if (::MultiByteToWideChar(CP_UTF8, MB_ERR_INVALID_CHARS,
                    text.data(), byteCount, result.data(), length) != length)
            {
                return std::nullopt;
            }
            return result;
        }

        [[nodiscard]] bool InvokeShellExecute(SHELLEXECUTEINFOW& info)
        {
            ShellExecuteExWFunction shellExecute = ShellExecuteExWApi();
            return shellExecute != nullptr && shellExecute(&info) != FALSE;
        }

        [[nodiscard]] bool StartWindowsUrl(const std::string& url)
        {
            std::optional<std::wstring> wide = Utf8ToWide(url);
            if (!wide.has_value())
            {
                return false;
            }

            SHELLEXECUTEINFOW info{};
            info.cbSize = sizeof(info);
            info.fMask = SEE_MASK_NOCLOSEPROCESS
                | SEE_MASK_FLAG_DDEWAIT
                | SEE_MASK_FLAG_NO_UI;
            info.lpVerb = nullptr;
            info.lpFile = wide->c_str();
            info.lpParameters = nullptr;
            info.lpDirectory = nullptr;
            info.nShow = SW_SHOWNORMAL;

            bool succeeded = false;
            if (CurrentThreadIsSta())
            {
                succeeded = InvokeShellExecute(info);
            }
            else
            {
                std::thread executionThread([&]
                {
                    CoInitializeExFunction initialize = CoInitializeExApi();
                    CoUninitializeFunction uninitialize = CoUninitializeApi();
                    if (initialize == nullptr || uninitialize == nullptr)
                    {
                        return;
                    }
                    constexpr DWORD CoinitApartmentThreaded = 0x2;
                    const HRESULT initialized = initialize(nullptr, CoinitApartmentThreaded);
                    if (FAILED(initialized))
                    {
                        return;
                    }
                    succeeded = InvokeShellExecute(info);
                    uninitialize();
                });
                executionThread.join();
            }

            if (info.hProcess != nullptr)
            {
                (void)::CloseHandle(info.hProcess);
            }
            return succeeded;
        }
#else
        void ReapChild(pid_t child) noexcept
        {
            int status = 0;
            while (::waitpid(child, &status, 0) < 0 && errno == EINTR)
            {
            }
        }

        [[nodiscard]] bool StartDesktopCommand(const char* program, const std::string& url)
        {
            pid_t child = -1;
            char* const argv[] = {
                const_cast<char*>(program),
                const_cast<char*>(url.c_str()),
                nullptr
            };
            const int error = ::posix_spawnp(&child, program, nullptr, nullptr, argv, environ);
            if (error != 0)
            {
                return false;
            }
            try
            {
                std::thread(ReapChild, child).detach();
            }
            catch (...)
            {
                // Process.Start has already succeeded. Failure to create a
                // native reaper must not turn that successful launch into a
                // failed OpenUrl result.
            }
            return true;
        }
#endif
    }

    bool Updater::Disabled() noexcept
    {
        return GetState().Disabled.load(std::memory_order_relaxed);
    }

    void Updater::Disabled(bool value) noexcept
    {
        GetState().Disabled.store(value, std::memory_order_relaxed);
    }

    std::optional<UpdateInfo> Updater::Available()
    {
        State& state = GetState();
        std::lock_guard lock(state.AvailableGate);
        return state.Available;
    }

    bool Updater::Checked() noexcept
    {
        return GetState().Checked.load(std::memory_order_relaxed);
    }

    std::optional<UpdateInfo> Updater::Check(CancellationToken cancel)
    {
        if (Disabled())
        {
            return std::nullopt;
        }

        std::optional<UpdateInfo> update = UpdateCheck::Latest(cancel);
        State& state = GetState();
        {
            std::lock_guard lock(state.AvailableGate);
            state.Available = std::move(update);
        }
        state.Checked.store(true, std::memory_order_relaxed);
        return Available();
    }

    void Updater::CheckInBackground(
        std::function<void(UpdateInfo)> found,
        std::function<void()> done)
    {
        if (Disabled())
        {
            return;
        }

        std::thread([found = std::move(found), done = std::move(done)]() mutable
        {
            try
            {
                try
                {
                    std::optional<UpdateInfo> update = Check();
                    if (update.has_value())
                    {
                        found(*update);
                    }
                }
                catch (...)
                {
                    // A background check never exposes its failure to callers.
                }

                if (done)
                {
                    done();
                }
            }
            catch (...)
            {
                // Task.Run retains exceptions thrown by the completion callback
                // in its unobserved task. The native fire-and-forget equivalent
                // keeps them from escaping the worker thread.
            }
        }).detach();
    }

    void Updater::WaitForCheck(TimeSpan limit)
    {
        if (Disabled())
        {
            return;
        }

        const std::int64_t until = AddDateTimeTicks(UtcNowTicks(), limit);
        while (!Checked() && UtcNowTicks() < until)
        {
            std::this_thread::sleep_for(std::chrono::milliseconds(50));
        }
    }

    bool Updater::OpenPage(UpdateInfo update)
    {
        const std::string& page = RequireString(update.PageUrl);
        return OpenUrl(page.length() > 0 ? page : std::string(UpdateCheck::ReleasesPage));
    }

    bool Updater::OpenLink(const std::string& url)
    {
        return OpenUrl(url);
    }

    bool Updater::OpenLink(std::nullptr_t url)
    {
        return OpenUrl(url);
    }

    bool Updater::OpenUrl(const std::string& url)
    {
        if (!url.starts_with("https://"))
        {
            return false;
        }

        try
        {
#ifdef _WIN32
            return StartWindowsUrl(url);
#elif defined(__APPLE__)
            return StartDesktopCommand("open", url);
#else
            const char* display = std::getenv("DISPLAY");
            const char* wayland = std::getenv("WAYLAND_DISPLAY");
            if ((display == nullptr || *display == '\0')
                && (wayland == nullptr || *wayland == '\0'))
            {
                return false;
            }
            return StartDesktopCommand("xdg-open", url);
#endif
        }
        catch (...)
        {
            return false;
        }
    }

    bool Updater::OpenUrl(std::nullptr_t)
    {
        throw NullReferenceException();
    }

    std::string Updater::Describe(UpdateInfo update)
    {
        const std::string& assetName = RequireString(update.AssetName);
        std::string which;
        if (!assetName.empty())
        {
            which = " -- you want " + assetName;
        }

        const std::string tag = InterpolateString(update.Tag);
        const std::string display = BuildVersion::Display();
        return tag + " is available (this is " + display + ")" + which;
    }
}

namespace MphRead::Mods::Update::Detail
{
    bool ServerUpdateUpdaterDisabled()
    {
        return Updater::Disabled();
    }

    std::optional<UpdateInfo> ServerUpdateUpdaterCheck()
    {
        return Updater::Check();
    }

    std::string ServerUpdateUpdaterDescribe(UpdateInfo update)
    {
        return Updater::Describe(std::move(update));
    }
}
