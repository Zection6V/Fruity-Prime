#include "Updater.hpp"
#include "BuildVersion.hpp"

#include <atomic>
#include <cerrno>
#include <chrono>
#include <cstdlib>
#include <limits>
#include <mutex>
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
            static State state;
            return state;
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

#ifndef _WIN32
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
                // Task.Run retains exceptions raised by the completion callback
                // in its unobserved task. The native fire-and-forget equivalent
                // must likewise keep them from escaping the worker thread.
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
            const HINSTANCE result = ::ShellExecuteA(
                nullptr, "open", url.c_str(), nullptr, nullptr, SW_SHOWNORMAL);
            return reinterpret_cast<std::intptr_t>(result) > 32;
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
