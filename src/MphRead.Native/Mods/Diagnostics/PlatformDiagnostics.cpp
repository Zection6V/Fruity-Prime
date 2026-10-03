#include "PlatformDiagnostics.hpp"

#include "../Branding.hpp"
#include "../DebugLog.hpp"
#include "../Platform/AppPaths.hpp"
#include "../Update/BuildVersion.hpp"
#include "../../NativeRuntime/System/AppDomain.hpp"
#include "../../NativeRuntime/System/Console.hpp"
#include "../../NativeRuntime/System/ExceptionText.hpp"
#include "../../NativeRuntime/System/Exceptions.hpp"
#include "../../NativeRuntime/System/IO.hpp"
#include "../../NativeRuntime/System/Process.hpp"
#include "../../NativeRuntime/System/Runtime.hpp"
#include "../../Utility/Console.hpp"

#include <chrono>
#include <exception>
#include <optional>
#include <stdexcept>
#include <string>
#include <string_view>
#include <utility>

namespace MphRead::Mods::Diagnostics
{
    namespace Runtime = ::MphRead::NativeRuntime;

    namespace
    {
        [[nodiscard]] std::string TrimFileDescription(std::string value)
        {
            constexpr std::string_view whitespace = " \t\r\n\f\v";
            const std::size_t first = value.find_first_not_of(whitespace);
            if (first == std::string::npos)
            {
                return std::string();
            }
            const std::size_t last = value.find_last_not_of(whitespace);
            return value.substr(first, last - first + 1);
        }
    }

    std::string PlatformDiagnostics::LogPath()
    {
        return Runtime::PathCombine(Platform::AppPaths::UserDataDirectory(), "logs", "platform-startup.log");
    }

    void PlatformDiagnostics::Start()
    {
        std::string message;
        const auto appendLine = [&message](std::string_view line)
        {
            message.append(line);
            message.append(Runtime::EnvironmentNewLine());
        };
        appendLine(std::string(Branding::Name) + " " + Update::BuildVersion::Display());
        appendLine("OS: " + Runtime::RuntimeInformationOSDescription());
        appendLine("OS architecture: " + Runtime::RuntimeInformationOSArchitecture());
        appendLine("Process architecture: " + Runtime::RuntimeInformationProcessArchitecture());
        appendLine("RID: " + Runtime::RuntimeInformationRuntimeIdentifier());
        appendLine("Runtime: " + Runtime::RuntimeInformationFrameworkDescription());
        appendLine("Base directory: " + Platform::AppPaths::ExecutableDirectory());
        appendLine("Launch directory: " + ::MphRead::ConsoleSetup::LaunchDirectory());
        appendLine("Current directory: " + Runtime::EnvironmentCurrentDirectory());
        appendLine("User data: " + Platform::AppPaths::UserDataDirectory());

        if (Runtime::IsMacOS())
        {
            for (const std::string_view library : {
                    "libopenal.1.dylib", "libglfw.3.dylib", "libminiaudio.dylib"})
            {
                appendLine("Native: " + Runtime::PathCombine(
                    Platform::AppPaths::ExecutableDirectory(), library));
            }
            Persist(message, false);
            Runtime::AppDomainAddUnhandledExceptionHandler([](std::exception_ptr exception)
            {
                PlatformDiagnostics::Report("unhandled startup/runtime failure", exception);
            });
        }

        Runtime::ConsoleWrite(message);
    }

    void PlatformDiagnostics::Report(std::string library, const std::exception& exception)
    {
        if (!Runtime::IsMacOS())
        {
            if (library == "libopenal.1.dylib")
            {
                library = Runtime::IsWindows() ? "openal32.dll" : "libopenal.so.1";
            }
            else if (library == "libglfw.3.dylib")
            {
                library = Runtime::IsWindows() ? "glfw3.dll" : "libglfw.so.3";
            }
            else if (library == "libminiaudio.dylib")
            {
                library = Runtime::IsWindows() ? "miniaudio.dll" : "libminiaudio.so";
            }
        }

        const std::string path = Runtime::PathCombine(
            Platform::AppPaths::ExecutableDirectory(), library);
        const std::string message = "[native] requested=" + library + "; location=" + path
            + "; process=" + Runtime::RuntimeInformationProcessArchitecture()
            + "; binary=" + Describe(path) + "\n"
            + Runtime::ExceptionToString(exception) + "\n";
        Runtime::ConsoleErrorWrite(message);
        DebugLog::Exception("native", exception);
        if (Runtime::IsMacOS())
        {
            Persist(message, true);
        }
    }

    void PlatformDiagnostics::Report(std::string library, std::exception_ptr exception)
    {
        if (!Runtime::IsMacOS())
        {
            if (library == "libopenal.1.dylib")
            {
                library = Runtime::IsWindows() ? "openal32.dll" : "libopenal.so.1";
            }
            else if (library == "libglfw.3.dylib")
            {
                library = Runtime::IsWindows() ? "glfw3.dll" : "libglfw.so.3";
            }
            else if (library == "libminiaudio.dylib")
            {
                library = Runtime::IsWindows() ? "miniaudio.dll" : "libminiaudio.so";
            }
        }

        const std::string path = Runtime::PathCombine(
            Platform::AppPaths::ExecutableDirectory(), library);
        const std::string exceptionText = exception
            ? Runtime::ExceptionToString(exception)
            : std::string("System.Object");
        const std::string message = "[native] requested=" + library + "; location=" + path
            + "; process=" + Runtime::RuntimeInformationProcessArchitecture()
            + "; binary=" + Describe(path) + "\n"
            + exceptionText + "\n";
        Runtime::ConsoleErrorWrite(message);
        DebugLog::Exception("native", exception);
        if (Runtime::IsMacOS())
        {
            Persist(message, true);
        }
    }

    std::string PlatformDiagnostics::Describe(const std::string& path)
    {
        if (!Runtime::FileExists(path))
        {
            return "not present at the installation path";
        }
        if (!Runtime::IsMacOS())
        {
            return "see loader exception";
        }
#if defined(__APPLE__)
        try
        {
            const std::optional<std::string> output = Runtime::ProcessRunCaptureOutputTimeout(
                "/usr/bin/file", {"-b", path}, std::chrono::milliseconds(2000));
            if (output.has_value())
            {
                return TrimFileDescription(*output);
            }
        }
        catch (const std::exception&)
        {
            // Diagnostics must preserve the original error.
        }
#endif
        return "architecture unavailable";
    }

    void PlatformDiagnostics::Persist(const std::string& message, bool append)
    {
        try
        {
            const std::string path = LogPath();
            if (const std::optional<std::string> directory = Runtime::PathGetDirectoryName(path);
                directory.has_value())
            {
                Runtime::DirectoryCreateDirectory(*directory);
            }
            if (append)
            {
                Runtime::FileAppendAllText(path, message);
            }
            else
            {
                Runtime::FileWriteAllText(path, message);
            }
        }
        catch (const System::IO::IOException& exception)
        {
            Runtime::ConsoleErrorWriteLine(std::string("Could not write platform log: ")
                + exception.what());
        }
        catch (const System::UnauthorizedAccessException& exception)
        {
            Runtime::ConsoleErrorWriteLine(std::string("Could not write platform log: ")
                + exception.what());
        }
    }
}
