#include "NetConnectCommand.hpp"

#include <optional>
#include <string>
#include <string_view>

namespace MphRead
{
    enum class GameMode;
    class RenderWindow;
    class Scene;
}

namespace MphRead::Mods::Network::Detail
{
    // Narrow integration boundary for the C# systems this isolated port touches.
    // Each CreateRenderWindow call must construct a fresh RenderWindow, keep it
    // alive until DisposeRenderWindow, and propagate every exception.
    struct NetConnectCommandServerRoom
    {
        std::string RoomKey;
        GameMode Mode;
    };

    bool NetConnectCommandNetLaunchJoin(const std::string& host, std::int32_t port,
        const std::string& playerName, Hunter hunter, std::int32_t color);
    std::optional<NetConnectCommandServerRoom> NetConnectCommandNetLaunchServerRoom();

    RenderWindow& NetConnectCommandCreateRenderWindow();
    Scene& NetConnectCommandRenderWindowScene(RenderWindow& renderer);
    void NetConnectCommandDisposeRenderWindow(RenderWindow& renderer);

    bool NetConnectCommandGameStateIsTeamMode(GameMode mode);
    void NetConnectCommandNetLaunchBuildPlayers(Scene& scene, Hunter hunter,
        std::int32_t recolor, bool teams);
    void NetConnectCommandRenderWindowAddRoom(RenderWindow& renderer,
        const std::string& roomKey, GameMode mode, std::int32_t playerCount);
    void NetConnectCommandRenderWindowRun(RenderWindow& renderer);

    std::string NetConnectCommandGameModeToString(GameMode mode);
    void NetConnectCommandConsoleWriteLine(std::string_view value);

    void NetConnectCommandNetSessionStop();
    void NetConnectCommandNetLogClose();

    // NetLaunch.RoomPlayerCount is a C# const, so reading it cannot execute code.
    inline constexpr std::int32_t NetConnectCommandNetLaunchRoomPlayerCount = 2;
}

namespace
{
    // C++ destructors cannot reproduce a throwing C# Dispose/finally while an
    // earlier exception is active. Calling the finalizer explicitly preserves
    // C# exception replacement as well as the exact ordering of nested finally
    // blocks.
    template <typename TBody, typename TFinally>
    void CSharpTryFinally(TBody&& body, TFinally&& finalizer)
    {
        try
        {
            body();
        }
        catch (...)
        {
            finalizer();
            throw;
        }
        finalizer();
    }
}

namespace MphRead::Mods::Network
{
    void NetConnectCommand::Run(const std::string& host, std::int32_t port,
        const std::string& playerName, Hunter hunter, std::int32_t recolor)
    {
        if (!Detail::NetConnectCommandNetLaunchJoin(
            host, port, playerName, hunter, recolor))
        {
            Detail::NetConnectCommandConsoleWriteLine("[net] could not join; giving up");
            Detail::NetConnectCommandNetSessionStop();
            return;
        }

        CSharpTryFinally(
            [&]()
            {
                Detail::NetConnectCommandServerRoom room
                    = Detail::NetConnectCommandNetLaunchServerRoom().value();
                RenderWindow& renderer = Detail::NetConnectCommandCreateRenderWindow();

                CSharpTryFinally(
                    [&]()
                    {
                        Scene& scene = Detail::NetConnectCommandRenderWindowScene(renderer);
                        const bool teams = Detail::NetConnectCommandGameStateIsTeamMode(room.Mode);
                        Detail::NetConnectCommandNetLaunchBuildPlayers(
                            scene, hunter, recolor, teams);
                        Detail::NetConnectCommandRenderWindowAddRoom(renderer, room.RoomKey,
                            room.Mode, Detail::NetConnectCommandNetLaunchRoomPlayerCount);

                        std::string message = "[net] loading ";
                        message += room.RoomKey;
                        message += " (";
                        message += Detail::NetConnectCommandGameModeToString(room.Mode);
                        message += ')';
                        Detail::NetConnectCommandConsoleWriteLine(message);

                        Detail::NetConnectCommandRenderWindowRun(renderer);
                    },
                    [&]()
                    {
                        Detail::NetConnectCommandDisposeRenderWindow(renderer);
                    });
            },
            []()
            {
                Detail::NetConnectCommandNetSessionStop();
                Detail::NetConnectCommandNetLogClose();
            });
    }
}
