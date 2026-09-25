#pragma once

// System.Console as .NET implements it: UTF-8 text out of the standard
// streams, Environment.NewLine as the line terminator, and a null string
// written as nothing at all.

#include <cstdint>
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

    // Console.ReadKey(intercept) waits for one key, echoing it unless
    // intercept is true. Throws System::InvalidOperationException where there
    // is no console to read from, as .NET does.
    void ConsoleReadKey();
    void ConsoleReadKeyIntercept();
    // ConsoleKey: the values .NET gives the keys this program asks about,
    // which are the Windows virtual-key codes.
    enum class ConsoleKey : std::int32_t
    {
        None = 0,
        Backspace = 8,
        Enter = 13,
        Escape = 27
    };
    // ConsoleKeyInfo: the key and the character it typed.
    struct ConsoleKeyInfo final
    {
        ConsoleKey Key = ConsoleKey::None;
        char32_t KeyChar = 0;
    };
    // Console.ReadKey(intercept), returning what was pressed.
    [[nodiscard]] ConsoleKeyInfo ConsoleReadKeyInfo(bool intercept);
    // Console.KeyAvailable: a key press is waiting to be read.
    [[nodiscard]] bool ConsoleKeyAvailable();
    // Console.IsInputRedirected.
    [[nodiscard]] bool ConsoleIsInputRedirected();
    // Console.ReadLine(): one line without its terminator, or null at the end
    // of the input. A console on Windows is read as UTF-16, so typed text
    // keeps every character; redirected input is UTF-8.
    [[nodiscard]] std::optional<std::string> ConsoleReadLine();
    // Console.Clear(): IOException when there is no console to clear.
    void ConsoleClear();

    // Environment.GetEnvironmentVariable(name): null when it is not set.
    [[nodiscard]] std::optional<std::string> EnvironmentGetVariable(const std::string& name);
    // Environment.MachineName.
    [[nodiscard]] std::string EnvironmentMachineName();
}
