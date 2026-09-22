#include "NetConnectCommand.hpp"

#include "../../GameState.hpp"
#include "../../NativeRuntime/System/Console.hpp"
#include "../../Renderer.hpp"
#include "../../Scene.hpp"
#include "NetLaunch.hpp"
#include "NetLog.hpp"
#include "NetSession.hpp"

#include <optional>
#include <string>

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
        if (!NetLaunch::Join(host, port, playerName, hunter, 8000, recolor))
        {
            NativeRuntime::ConsoleWriteLine("[net] could not join; giving up");
            NetSession::Stop();
            return;
        }

        CSharpTryFinally(
            [&]()
            {
                const NetLaunchServerRoom room = NetLaunch::ServerRoom().value();
                // `using var renderer = new RenderWindow();` -- the object is
                // disposed when this scope ends, which is before the outer
                // finally runs, exactly as C# orders them.
                RenderWindow renderer;
                NetLaunch::BuildPlayers(renderer.Scene(), hunter, recolor,
                    GameState::IsTeamMode(room.Mode));
                renderer.AddRoom(room.RoomKey, room.Mode, NetLaunch::RoomPlayerCount);
                NativeRuntime::ConsoleWriteLine(
                    "[net] loading " + room.RoomKey
                    + " (" + ::MphRead::ToString(room.Mode) + ")");
                renderer.Run();
            },
            []()
            {
                // The session owns a worker thread and a bound socket; a crash
                // in the game must not leave either behind.
                NetSession::Stop();
                NetLog::Close();
            });
    }
}
