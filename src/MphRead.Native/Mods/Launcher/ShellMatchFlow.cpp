#include "ShellMatchFlow.hpp"

#include "Portable/MatchStart.hpp"
#include "../DebugLog.hpp"
#include "../Network/Session/NetSession.hpp"
#include "../../NativeRuntime/System/ExceptionText.hpp"

#include <exception>
#include <iostream>
#include <optional>
#include <type_traits>
#include <utility>
#include <variant>

namespace MphRead::Mods::Launcher
{
    namespace
    {
        using Request = std::variant<LaunchPlan, ShellMatchFlow::StartCallback>;

        std::optional<Request> g_pending;
        std::optional<LaunchPlan> g_played;

        [[nodiscard]] LaunchPlan WithRoomKey(const LaunchPlan& source, std::string roomKey)
        {
            LaunchPlan::Init init;
            init.Kind = source.Kind();
            init.Lobby = source.Lobby();
            init.Hunter = source.Hunter();
            init.RoomKey = std::move(roomKey);
            init.Mode = source.Mode();
            init.Bots = source.Bots();
            init.BotLevel = source.BotLevel();
            init.Instagib = source.Instagib();
            init.Port = source.Port();
            init.PlayerName = source.PlayerName();
            init.SaveSlot = source.SaveSlot();
            init.NewGame = source.NewGame();
            init.DemoPath = source.DemoPath();
            return LaunchPlan(init);
        }
    }

    void ShellMatchFlow::Queue(LaunchPlan plan)
    {
        g_pending = std::move(plan);
    }

    void ShellMatchFlow::Queue(StartCallback start)
    {
        if (!start)
        {
            g_pending.reset();
            return;
        }
        g_pending = std::move(start);
    }

    bool ShellMatchFlow::HasPending() noexcept
    {
        return g_pending.has_value();
    }

    bool ShellMatchFlow::StartPending(
        MphRead::RenderWindow& window,
        std::shared_ptr<MphRead::MenuSettings> settings)
    {
        if (!g_pending.has_value())
        {
            return false;
        }

        Request request = std::move(*g_pending);
        g_pending.reset();
        try
        {
            const bool started = std::visit([&](auto&& start)
            {
                using Start = std::remove_cvref_t<decltype(start)>;
                if constexpr (std::is_same_v<Start, LaunchPlan>)
                {
                    g_played = start;
                    return MatchStart::Begin(window, settings, start);
                }
                else
                {
                    g_played.reset();
                    return start(window);
                }
            }, request);
            if (!started)
            {
                MphRead::Mods::Network::NetSession::ReportMatchLoadFailed(
                    "The match could not be started.");
            }
            return started;
        }
        catch (const std::exception&)
        {
            const std::exception_ptr exception = std::current_exception();
            std::cout << "\nThe game could not start: "
                      << MphRead::NativeRuntime::ExceptionMessage(exception) << '\n';
            MphRead::Mods::DebugLog::Line("crash", "the match could not start");
            MphRead::Mods::DebugLog::Exception("crash", exception);
            MphRead::Mods::Network::NetSession::ReportMatchLoadFailed(
                MphRead::NativeRuntime::ExceptionMessage(exception));
            return false;
        }
    }

    bool ShellMatchFlow::CanPlayAnother() noexcept
    {
        return !g_pending.has_value() && g_played.has_value()
            && g_played->Kind() == LaunchKind::Offline;
    }

    bool ShellMatchFlow::QueueAnother(std::string roomKey)
    {
        if (!CanPlayAnother() || roomKey.empty())
        {
            return false;
        }
        Queue(WithRoomKey(*g_played, std::move(roomKey)));
        return true;
    }

    void ShellMatchFlow::ClearPending() noexcept
    {
        g_pending.reset();
    }
}
