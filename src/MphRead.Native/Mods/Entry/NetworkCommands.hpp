#pragma once

#include <string>
#include <vector>

namespace MphRead::Mods::Entry
{
    // The client side of the network, once the game files are set up, in
    // ModEntry's order: -installserver (fetch and unpack the server
    // package), -hostlocal (start a server here, report, stop it), -hostgame
    // (ask a directory to run a match, then join it), -connect, -netcheck (a
    // scripted client; -size's width and height are its window's), and
    // -demoinfo. True when one ran; false, having touched nothing, when none
    // was asked for.
    //
    // None of these is -server: a dedicated server is ServerCommands', and
    // runs before setup.
    [[nodiscard]] bool TryRunNetworkCommands(const std::vector<std::string>& args, int width, int height);
}
