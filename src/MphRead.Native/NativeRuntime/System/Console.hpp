#pragma once

// System.Console as .NET implements it: UTF-8 text out of the standard
// streams, Environment.NewLine as the line terminator, and a null string
// written as nothing at all.

#include <optional>
#include <string>
#include <string_view>

namespace MphRead::NativeRuntime
{
    // Environment.NewLine.
    [[nodiscard]] std::string_view EnvironmentNewLine() noexcept;

    // Console.Write(value).
    void ConsoleWrite(std::string_view value);
    // Console.WriteLine(value).
    void ConsoleWriteLine(std::string_view value);
    // Console.WriteLine(value) where a null string writes the line terminator
    // alone.
    void ConsoleWriteLineNullable(const std::optional<std::string>& value);
    // Console.WriteLine().
    void ConsoleWriteLine();

    // Console.Error.Write(value) / Console.Error.WriteLine(value).
    void ConsoleErrorWrite(std::string_view value);
    void ConsoleErrorWriteLine(std::string_view value);

    // Console.Out.Flush().
    void ConsoleFlush();

    // Environment.MachineName.
    [[nodiscard]] std::string EnvironmentMachineName();
}
