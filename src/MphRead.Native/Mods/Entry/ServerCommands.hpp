#pragma once

#include <string>
#include <vector>

namespace MphRead::Mods::Entry
{
    // The server half of the command line -- the last thing
    // ModEntry::TryHandleHeadless tries, before any game file is checked,
    // in this order:
    //
    // - a build defining MPHREAD_SERVER, started with no arguments, prints
    //   how to run it (no current C++ target defines it);
    // - -masterserver, -server and -dedicated first let the server updater
    //   act, with the command line as the operating system holds it rather
    //   than `args` -- it is what a restarted server is started with again;
    //   if that update takes over, nothing else runs;
    // - -masterserver runs a directory until Ctrl+C: its port is -port, else
    //   -masterport; -hostports FIRST-LAST or none; -public / -publicaddress;
    // - -servers and -hosts ask a directory (-master, -masterport) and print
    //   what it says;
    // - -server / -dedicated run the authoritative match server until Ctrl+C,
    //   and exit the process with 1 if it cannot run the match.
    //
    // True when one of them ran. False when the command line asks for none
    // of them, which hands it on to the setup check.
    [[nodiscard]] bool TryRunServerCommands(const std::vector<std::string>& args);
}
