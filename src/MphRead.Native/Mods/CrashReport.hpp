#pragma once

#include <exception>
#include <optional>
#include <string>
#include <string_view>

namespace MphRead::Mods
{
    // The last thing that runs when nothing else could: what a crash before
    // the window looks like from the outside, and where it is written down.
    //
    // Not the same thing as DebugLog, which is a session-long transcript kept
    // only when a player has asked for one. This is always installed and
    // writes exactly one file, because the failure it exists for is the one
    // nobody can report: the Windows build is a GUI binary (see
    // ConsoleWindow), so an exception on the way to the first window has
    // nowhere to print. The process disappears, the player double-clicks the
    // executable again and again, and every symptom there is is "nothing
    // happens".
    //
    // So: a file beside the executable, a console with the message in it on
    // Windows, and a pause when that console belongs to us -- otherwise the
    // window it was printed into closes with the process.
    class CrashReport final
    {
    public:
        CrashReport() = delete;
        ~CrashReport() = delete;
        CrashReport(const CrashReport&) = delete;
        CrashReport& operator=(const CrashReport&) = delete;
        CrashReport(CrashReport&&) = delete;
        CrashReport& operator=(CrashReport&&) = delete;

        // Where the last report went, if one was written.
        [[nodiscard]] static std::optional<std::string> Path();

        // Install the handler. Called first thing in Main, before anything
        // that can throw -- which on a fresh install is most of startup.
        static void Install();

        // Write one report and, on Windows, make sure it is on the screen.
        //
        // Only the first is written: an exception on the way out of another
        // one would otherwise overwrite the report naming the fault that
        // actually started it.
        static void Report(const std::exception_ptr& ex, std::string_view source);

    private:
        [[nodiscard]] static std::optional<std::string> TryWrite(
            const std::exception_ptr& ex, std::string_view source);

        static bool _installed;
        static bool _reported;
        static std::optional<std::string> _path;
    };
}
