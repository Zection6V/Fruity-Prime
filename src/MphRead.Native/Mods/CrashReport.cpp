#include "CrashReport.hpp"

#include "Branding.hpp"
#include "ConsoleWindow.hpp"
#include "LogShare.hpp"
#include "Launcher/Portable/LauncherPrefs.hpp"
#include "Update/BuildVersion.hpp"
#include "../Program.hpp"
#include "../NativeRuntime/System/AppDomain.hpp"
#include "../NativeRuntime/System/Console.hpp"
#include "../NativeRuntime/System/DateTime.hpp"
#include "../NativeRuntime/System/Encoding.hpp"
#include "../NativeRuntime/System/ExceptionText.hpp"
#include "../NativeRuntime/System/IO.hpp"
#include "../NativeRuntime/System/Runtime.hpp"

#include <array>
#include <exception>
#include <string>

namespace MphRead::Mods
{
    namespace
    {
        namespace Runtime = ::MphRead::NativeRuntime;

        // StringBuilder.AppendLine(value).
        void AppendLine(std::string& text, std::string_view value)
        {
            text.append(value);
            text.append(Runtime::EnvironmentNewLine());
        }

        void AppendLine(std::string& text)
        {
            text.append(Runtime::EnvironmentNewLine());
        }

        // ex.GetType().Name, for an exception held by pointer.
        [[nodiscard]] std::string TypeName(const std::exception_ptr& ex)
        {
            if (!ex)
            {
                return "System.Object";
            }
            try
            {
                std::rethrow_exception(ex);
            }
            catch (const std::exception& value)
            {
                return Runtime::ExceptionTypeName(value);
            }
            catch (...)
            {
                return "System.Object";
            }
        }

        // ex.Message.
        [[nodiscard]] std::string Message(const std::exception_ptr& ex)
        {
            if (!ex)
            {
                return std::string();
            }
            try
            {
                std::rethrow_exception(ex);
            }
            catch (const std::exception& value)
            {
                return value.what();
            }
            catch (...)
            {
                return std::string();
            }
        }
    }

    bool CrashReport::_installed = false;
    bool CrashReport::_reported = false;
    std::optional<std::string> CrashReport::_path{};

    std::optional<std::string> CrashReport::Path()
    {
        return _path;
    }

    void CrashReport::Install()
    {
        if (_installed)
        {
            return;
        }
        _installed = true;
        Runtime::AppDomainAddUnhandledExceptionHandler(
            [](std::exception_ptr ex) { Report(ex, "unhandled"); });
    }

    void CrashReport::Report(const std::exception_ptr& ex, std::string_view source)
    {
        if (_reported || !ex)
        {
            return;
        }
        _reported = true;
        const std::optional<std::string> path = TryWrite(ex, source);
        _path = path;
        // A GUI binary that got this far has no console at all; without one
        // the message below goes nowhere, which is the whole failure this
        // class exists for.
#if defined(_WIN32)
        ConsoleWindow::Show();
#endif
        Runtime::ConsoleErrorWriteLine("");
        Runtime::ConsoleErrorWriteLine(std::string(Branding::Name) + " could not start.");
        Runtime::ConsoleErrorWriteLine("");
        Runtime::ConsoleErrorWriteLine("  " + TypeName(ex) + ": " + Message(ex));
        if (path.has_value())
        {
            Runtime::ConsoleErrorWriteLine("");
            Runtime::ConsoleErrorWriteLine("The details are in " + *path + ".");
        }
        Runtime::ConsoleErrorWriteLine("");
        // Started by double-click: the console window is this process's own
        // and closes with it, so everything above would be a flash of black
        // without this.
#if defined(_WIN32)
        if (ConsoleWindow::OwnsItsConsole())
        {
            Runtime::ConsoleErrorWriteLine("Press any key to close...");
            try
            {
                Runtime::ConsoleReadKeyIntercept();
            }
            catch (const std::exception&)
            {
                // No key to read is no reason to fail on the way out.
            }
        }
#endif
    }

    std::optional<std::string> CrashReport::TryWrite(
        const std::exception_ptr& ex, std::string_view source)
    {
        std::string text;
        AppendLine(text, std::string(Branding::Name) + " " + Update::BuildVersion::Display()
            + ", data format " + Program::Version.ToString());
        AppendLine(text, Runtime::DateTimeToString(
            Runtime::DateTimeNow(), "yyyy-MM-dd HH:mm:ss zzz"));
        AppendLine(text, Runtime::EnvironmentOSVersion() + ", .NET "
            + Runtime::EnvironmentVersion() + ", 64-bit process="
            + (Runtime::EnvironmentIs64BitProcess() ? "True" : "False"));
        AppendLine(text, "base=" + Runtime::AppContextBaseDirectory());
        AppendLine(text, "command line=" + Runtime::EnvironmentCommandLine());
        AppendLine(text, "source=" + std::string(source));
        AppendLine(text);
        AppendLine(text, Runtime::ExceptionToString(ex));
        // Named .log because LogArchive gathers "*.log", and this has to
        // travel with the rest of them.
        std::string name(Branding::Name);
        for (std::size_t space = name.find(' '); space != std::string::npos;
            space = name.find(' '))
        {
            name.erase(space, 1);
        }
        name += "-crash-" + Runtime::DateTimeToString(
            Runtime::DateTimeNow(), "yyyyMMdd-HHmmss")
            + "-" + std::to_string(Runtime::EnvironmentProcessId()) + ".log";
        // The logs folder first, and not because it is tidy: that is the one
        // directory LogArchive gathers and the share sheet hands out, so a
        // crash written anywhere else is a crash a player on a phone has no
        // way to send. Then beside the executable, where somebody who has just
        // downloaded a desktop release will look, and the temporary directory
        // for an installation under Program Files.
        const std::array<std::string, 4> directories = {
            Runtime::Utf16ToUtf8(LogArchive::Directory()),
            Launcher::LauncherPrefs::Directory(),
            Runtime::AppContextBaseDirectory(),
            Runtime::PathGetTempPath()
        };
        for (const std::string& directory : directories)
        {
            try
            {
                Runtime::DirectoryCreateDirectory(directory);
                const std::string path = Runtime::PathCombine(directory, name);
                Runtime::FileWriteAllText(path, text);
                return path;
            }
            catch (const std::exception&)
            {
                // Try the next one; a report that cannot be written is still a
                // message that can be printed.
            }
        }
        return std::nullopt;
    }
}
