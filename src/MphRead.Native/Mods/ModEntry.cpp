#include "ModEntry.hpp"

#include "../Entities/Players/PlayerEntity.hpp"
#include "../Features.hpp"
#include "../Formats/Enums.hpp"
#include "../Utility/Console.hpp"
#include "Branding.hpp"
#include "ConsoleWindow.hpp"
#include "Credits.hpp"
#include "DebugLog.hpp"
#include "Input/GamepadProbe.hpp"
#include "InputSettings.hpp"
#if defined(MPHREAD_AVALONIA)
#include "Launcher/Gui/GuiLauncher.hpp"
#include "Launcher/Gui/UiCapture.hpp"
#endif
#include "Launcher/Portable/LauncherPrefs.hpp"
#include "Launcher/Portable/TextLauncher.hpp"
#include "MapGen/CustomRooms.hpp"
#include "MapGen/MapBundle.hpp"
#include "MapGen/MapPacker.hpp"
#include "MapGen/MapReport.hpp"
#include "MapGen/MapTextureBake.hpp"
#include "MapGen/Q3Bsp.hpp"
#include "MapGen/Q3Convert.hpp"
#include "Network/DedicatedServer.hpp"
#include "Network/DemoInfo.hpp"
#include "Network/MapAudit.hpp"
#include "Network/MapRotation.hpp"
#include "Network/MechanicsDump.hpp"
#include "Network/NetCheckClient.hpp"
#include "Network/NetConnectCommand.hpp"
#include "Network/NetDiagnostics.hpp"
#include "Network/NetHitPrediction.hpp"
#include "Network/NetLag.hpp"
#include "Network/NetMaster.hpp"
#include "Network/NetUnlagged.hpp"
#include "Network/ServerSimCheck.hpp"
#include "Network/WeaponDps.hpp"
#include "Render/Crosshair.hpp"
#include "Render/FrameTiming.hpp"
#include "Render/FrameTimingCheck.hpp"
#include "RenderOptions.hpp"
#include "ShutdownSignals.hpp"
#include "ThumbnailBatch.hpp"
#include "ThumbnailCapture.hpp"
#include "ThumbnailGenerator.hpp"
#include "Update/DesktopUpdate.hpp"
#include "Update/ServerUpdate.hpp"
#include "Update/UpdateCheck.hpp"
#include "Update/UpdateInstall.hpp"
#include "Update/Updater.hpp"
#include "WindowMode.hpp"

#include <algorithm>
#include <array>
#include <charconv>
#include <cmath>
#include <cstdint>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <limits>
#include <locale>
#include <memory>
#include <optional>
#include <sstream>
#include <stop_token>
#include <string>
#include <string_view>
#include <system_error>
#include <utility>
#include <vector>

#if defined(_WIN32)
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <conio.h>
#include <windows.h>
#elif defined(__APPLE__)
#include <crt_externs.h>
#include <limits.h>
#include <mach-o/dyld.h>
#include <unistd.h>
#else
#include <limits.h>
#include <unistd.h>
#endif

// Environment.ExitCode is process state, not an immediate exit. The executable
// wrapper is the Native equivalent of the CLR host and owns the eventual return
// from main; this two-file port only assigns the state through that runtime
// boundary. A focused probe provides the boundary while peer Native entry files
// are still being ported.
namespace MphRead::NativeRuntime
{
    void SetEnvironmentExitCode(std::int32_t value) noexcept;
}

namespace
{
    using MphRead::BeamType;
    using MphRead::GameMode;
    using MphRead::Hunter;

    [[nodiscard]] bool IsAsciiWhitespace(unsigned char ch) noexcept
    {
        return ch == 0x20 || (ch >= 0x09 && ch <= 0x0D);
    }

    [[nodiscard]] std::string_view TrimNumberWhitespace(std::string_view value) noexcept
    {
        while (!value.empty() && IsAsciiWhitespace(static_cast<unsigned char>(value.front())))
        {
            value.remove_prefix(1);
        }
        while (!value.empty() && IsAsciiWhitespace(static_cast<unsigned char>(value.back())))
        {
            value.remove_suffix(1);
        }
        return value;
    }

    [[nodiscard]] char AsciiLower(char ch) noexcept
    {
        if (ch >= 'A' && ch <= 'Z')
        {
            return static_cast<char>(ch + ('a' - 'A'));
        }
        return ch;
    }

    [[nodiscard]] bool EqualsOrdinalIgnoreCase(std::string_view left, std::string_view right) noexcept
    {
        if (left.size() != right.size())
        {
            return false;
        }
        for (std::size_t i = 0; i < left.size(); ++i)
        {
            const unsigned char a = static_cast<unsigned char>(left[i]);
            const unsigned char b = static_cast<unsigned char>(right[i]);
            if (a < 0x80 && b < 0x80)
            {
                if (AsciiLower(static_cast<char>(a)) != AsciiLower(static_cast<char>(b)))
                {
                    return false;
                }
            }
            else if (a != b)
            {
                // All names owned by ModEntry are ASCII. Non-ASCII code units
                // therefore cannot match one of them unless they are identical;
                // this branch also avoids locale-dependent case conversion.
                return false;
            }
        }
        return true;
    }

    [[nodiscard]] std::string ToLowerAscii(std::string_view value)
    {
        std::string result(value);
        for (char& ch : result)
        {
            ch = AsciiLower(ch);
        }
        return result;
    }

    [[nodiscard]] std::string_view TrimAscii(std::string_view value) noexcept
    {
        return TrimNumberWhitespace(value);
    }

    [[nodiscard]] std::string_view TrimStartHyphen(std::string_view value) noexcept
    {
        while (!value.empty() && value.front() == '-')
        {
            value.remove_prefix(1);
        }
        return value;
    }

    [[nodiscard]] bool IsFlag(std::string_view argument, std::string_view name) noexcept
    {
        return EqualsOrdinalIgnoreCase(TrimStartHyphen(argument), name);
    }

    [[nodiscard]] bool HasFlag(const std::vector<std::string>& args, std::string_view name) noexcept
    {
        for (const std::string& argument : args)
        {
            if (IsFlag(argument, name))
            {
                return true;
            }
        }
        return false;
    }

    [[nodiscard]] int IndexOfFlag(const std::vector<std::string>& args, std::string_view name) noexcept
    {
        for (std::size_t i = 0; i < args.size(); ++i)
        {
            if (IsFlag(args[i], name))
            {
                return static_cast<int>(i);
            }
        }
        return -1;
    }

    [[nodiscard]] std::optional<std::string> ValueAfter(
        const std::vector<std::string>& args, std::string_view name)
    {
        if (args.size() < 2)
        {
            return std::nullopt;
        }
        for (std::size_t i = 0; i + 1 < args.size(); ++i)
        {
            if (IsFlag(args[i], name))
            {
                return args[i + 1];
            }
        }
        return std::nullopt;
    }

    [[nodiscard]] std::vector<std::string> ValuesAfter(
        const std::vector<std::string>& args, std::string_view name)
    {
        std::vector<std::string> result;
        if (args.size() < 2)
        {
            return result;
        }
        for (std::size_t i = 0; i + 1 < args.size(); ++i)
        {
            if (IsFlag(args[i], name))
            {
                result.push_back(args[i + 1]);
            }
        }
        return result;
    }

    [[nodiscard]] bool StartsWithHyphen(const std::optional<std::string>& value) noexcept
    {
        return value.has_value() && !value->empty() && value->front() == '-';
    }

    [[nodiscard]] bool TryParseInt32(std::string_view text, std::int32_t& value) noexcept
    {
        text = TrimNumberWhitespace(text);
        if (text.empty())
        {
            value = 0;
            return false;
        }
        bool negative = false;
        if (text.front() == '+' || text.front() == '-')
        {
            negative = text.front() == '-';
            text.remove_prefix(1);
            if (text.empty())
            {
                value = 0;
                return false;
            }
        }
        std::uint64_t magnitude = 0;
        const char* first = text.data();
        const char* last = first + text.size();
        const auto parsed = std::from_chars(first, last, magnitude, 10);
        if (parsed.ec != std::errc{} || parsed.ptr != last)
        {
            value = 0;
            return false;
        }
        constexpr std::uint64_t maxPositive = static_cast<std::uint64_t>(std::numeric_limits<std::int32_t>::max());
        constexpr std::uint64_t maxNegative = maxPositive + 1;
        if ((!negative && magnitude > maxPositive) || (negative && magnitude > maxNegative))
        {
            value = 0;
            return false;
        }
        if (negative)
        {
            value = magnitude == maxNegative
                ? std::numeric_limits<std::int32_t>::min()
                : -static_cast<std::int32_t>(magnitude);
        }
        else
        {
            value = static_cast<std::int32_t>(magnitude);
        }
        return true;
    }

    [[nodiscard]] bool EqualsIgnoreCaseAscii(std::string_view left, std::string_view right) noexcept
    {
        return EqualsOrdinalIgnoreCase(left, right);
    }

    template <typename Float>
    [[nodiscard]] bool TryParseFloatingClassic(std::string_view text, Float& value, bool allowThousands)
    {
        text = TrimNumberWhitespace(text);
        if (text.empty())
        {
            value = 0;
            return false;
        }
        std::string work(text);
        if (allowThousands)
        {
            work.erase(std::remove(work.begin(), work.end(), ','), work.end());
        }
        const std::string lower = ToLowerAscii(work);
        if (lower == "nan" || lower == "+nan" || lower == "-nan")
        {
            value = std::numeric_limits<Float>::quiet_NaN();
            return true;
        }
        if (lower == "infinity" || lower == "+infinity")
        {
            value = std::numeric_limits<Float>::infinity();
            return true;
        }
        if (lower == "-infinity")
        {
            value = -std::numeric_limits<Float>::infinity();
            return true;
        }
        std::istringstream stream(work);
        stream.imbue(std::locale::classic());
        Float parsed = 0;
        stream >> parsed;
        if (stream.fail())
        {
            value = 0;
            return false;
        }
        stream >> std::ws;
        if (!stream.eof())
        {
            value = 0;
            return false;
        }
        value = parsed;
        return true;
    }

    [[nodiscard]] bool TryParseDoubleInvariant(
        const std::optional<std::string>& text, double& value, bool allowThousands = true)
    {
        if (!text.has_value())
        {
            value = 0;
            return false;
        }
        return TryParseFloatingClassic(*text, value, allowThousands);
    }

    [[nodiscard]] bool TryParseFloatInvariant(
        const std::optional<std::string>& text, float& value, bool allowThousands = true)
    {
        if (!text.has_value())
        {
            value = 0;
            return false;
        }
        return TryParseFloatingClassic(*text, value, allowThousands);
    }

    [[nodiscard]] bool TryParseDoubleCurrent(std::string_view text, double& value)
    {
        text = TrimNumberWhitespace(text);
        if (text.empty())
        {
            value = 0;
            return false;
        }
        const std::string lower = ToLowerAscii(text);
        if (lower == "nan" || lower == "+nan" || lower == "-nan")
        {
            value = std::numeric_limits<double>::quiet_NaN();
            return true;
        }
        if (lower == "infinity" || lower == "+infinity")
        {
            value = std::numeric_limits<double>::infinity();
            return true;
        }
        if (lower == "-infinity")
        {
            value = -std::numeric_limits<double>::infinity();
            return true;
        }
        std::istringstream stream{std::string(text)};
        try
        {
            stream.imbue(std::locale(""));
        }
        catch (const std::runtime_error&)
        {
            stream.imbue(std::locale::classic());
        }
        double parsed = 0;
        stream >> parsed;
        if (stream.fail())
        {
            value = 0;
            return false;
        }
        stream >> std::ws;
        if (!stream.eof())
        {
            value = 0;
            return false;
        }
        value = parsed;
        return true;
    }

    template <typename Enum>
    struct EnumName final
    {
        std::string_view Name;
        std::int32_t Value;
    };

    constexpr std::array<EnumName<GameMode>, 15> GameModeNames{{
        {"None", 0},
        {"SinglePlayer", 2},
        {"Battle", 3},
        {"BattleTeams", 4},
        {"Survival", 5},
        {"SurvivalTeams", 6},
        {"Capture", 7},
        {"Bounty", 8},
        {"BountyTeams", 9},
        {"Nodes", 10},
        {"NodesTeams", 11},
        {"Defender", 12},
        {"DefenderTeams", 13},
        {"PrimeHunter", 14},
        {"Unknown15", 15}
    }};

    constexpr std::array<EnumName<Hunter>, 9> HunterNames{{
        {"Samus", 0}, {"Kanden", 1}, {"Trace", 2}, {"Sylux", 3},
        {"Noxus", 4}, {"Spire", 5}, {"Weavel", 6}, {"Guardian", 7},
        {"Random", 8}
    }};

    constexpr std::array<EnumName<BeamType>, 12> BeamNames{{
        {"None", -1}, {"PowerBeam", 0}, {"VoltDriver", 1}, {"Missile", 2},
        {"Battlehammer", 3}, {"Imperialist", 4}, {"Judicator", 5},
        {"Magmaul", 6}, {"ShockCoil", 7}, {"OmegaCannon", 8},
        {"Platform", 9}, {"Enemy", 10}
    }};

    template <typename Enum, std::size_t N>
    [[nodiscard]] bool TryParseEnum(std::string_view text,
        const std::array<EnumName<Enum>, N>& names, std::int32_t minValue,
        std::int32_t maxValue, Enum& value)
    {
        text = TrimAscii(text);
        if (text.empty())
        {
            return false;
        }

        const char first = text.front();
        if ((first >= '0' && first <= '9') || first == '+' || first == '-')
        {
            std::int32_t number = 0;
            if (!TryParseInt32(text, number) || number < minValue || number > maxValue)
            {
                return false;
            }
            value = static_cast<Enum>(number);
            return true;
        }

        std::int32_t combined = 0;
        std::size_t start = 0;
        bool foundAny = false;
        while (true)
        {
            const std::size_t comma = text.find(',', start);
            std::string_view part = comma == std::string_view::npos
                ? text.substr(start)
                : text.substr(start, comma - start);
            part = TrimAscii(part);
            bool found = false;
            for (const auto& item : names)
            {
                if (EqualsIgnoreCaseAscii(part, item.Name))
                {
                    combined |= item.Value;
                    found = true;
                    foundAny = true;
                    break;
                }
            }
            if (!found)
            {
                return false;
            }
            if (comma == std::string_view::npos)
            {
                break;
            }
            start = comma + 1;
        }
        value = static_cast<Enum>(combined);
        return foundAny;
    }

    [[nodiscard]] bool TryParseGameMode(const std::optional<std::string>& text, GameMode& value)
    {
        if (!text.has_value())
        {
            return false;
        }
        return TryParseEnum(*text, GameModeNames, 0, 255, value);
    }

    [[nodiscard]] bool TryParseHunter(const std::optional<std::string>& text, Hunter& value)
    {
        if (!text.has_value())
        {
            return false;
        }
        return TryParseEnum(*text, HunterNames, 0, 255, value);
    }

    [[nodiscard]] bool TryParseBeam(const std::optional<std::string>& text, BeamType& value)
    {
        if (!text.has_value())
        {
            return false;
        }
        return TryParseEnum(*text, BeamNames, -128, 127, value);
    }

#if defined(_WIN32)
    [[nodiscard]] std::string Utf8FromWide(std::wstring_view text)
    {
        if (text.empty())
        {
            return {};
        }
        const int count = WideCharToMultiByte(CP_UTF8, WC_ERR_INVALID_CHARS,
            text.data(), static_cast<int>(text.size()), nullptr, 0, nullptr, nullptr);
        if (count <= 0)
        {
            throw std::runtime_error("WideCharToMultiByte failed");
        }
        std::string result(static_cast<std::size_t>(count), '\0');
        if (WideCharToMultiByte(CP_UTF8, WC_ERR_INVALID_CHARS,
            text.data(), static_cast<int>(text.size()), result.data(), count,
            nullptr, nullptr) != count)
        {
            throw std::runtime_error("WideCharToMultiByte failed");
        }
        return result;
    }
#endif

    [[nodiscard]] std::vector<std::string> GetCommandLineArguments()
    {
#if defined(_WIN32)
        using CommandLineToArgvWFunction = LPWSTR* (WINAPI*)(LPCWSTR, int*);
        HMODULE shell = LoadLibraryW(L"shell32.dll");
        if (shell == nullptr)
        {
            throw std::runtime_error("LoadLibraryW(shell32.dll) failed");
        }
        auto commandLineToArgvW = reinterpret_cast<CommandLineToArgvWFunction>(
            GetProcAddress(shell, "CommandLineToArgvW"));
        if (commandLineToArgvW == nullptr)
        {
            FreeLibrary(shell);
            throw std::runtime_error("CommandLineToArgvW is unavailable");
        }
        int argc = 0;
        LPWSTR* argv = commandLineToArgvW(GetCommandLineW(), &argc);
        if (argv == nullptr)
        {
            FreeLibrary(shell);
            throw std::runtime_error("CommandLineToArgvW failed");
        }
        std::vector<std::string> result;
        result.reserve(static_cast<std::size_t>(argc));
        try
        {
            for (int i = 0; i < argc; ++i)
            {
                result.push_back(Utf8FromWide(argv[i]));
            }
        }
        catch (...)
        {
            LocalFree(argv);
            FreeLibrary(shell);
            throw;
        }
        LocalFree(argv);
        FreeLibrary(shell);
        return result;
#elif defined(__APPLE__)
        const int argc = *_NSGetArgc();
        char** argv = *_NSGetArgv();
        std::vector<std::string> result;
        result.reserve(static_cast<std::size_t>(std::max(argc, 0)));
        for (int i = 0; i < argc; ++i)
        {
            result.emplace_back(argv[i] == nullptr ? "" : argv[i]);
        }
        return result;
#else
        std::ifstream input("/proc/self/cmdline", std::ios::binary);
        if (!input)
        {
            throw std::runtime_error("could not read /proc/self/cmdline");
        }
        const std::string data((std::istreambuf_iterator<char>(input)),
            std::istreambuf_iterator<char>());
        std::vector<std::string> result;
        std::size_t start = 0;
        while (start < data.size())
        {
            const std::size_t end = data.find('\0', start);
            if (end == std::string::npos)
            {
                result.emplace_back(data.substr(start));
                break;
            }
            result.emplace_back(data.substr(start, end - start));
            start = end + 1;
        }
        return result;
#endif
    }

    [[nodiscard]] std::optional<std::string> ProcessPath()
    {
#if defined(_WIN32)
        std::vector<wchar_t> buffer(512);
        for (;;)
        {
            const DWORD length = GetModuleFileNameW(nullptr, buffer.data(),
                static_cast<DWORD>(buffer.size()));
            if (length == 0)
            {
                return std::nullopt;
            }
            if (length < buffer.size() - 1)
            {
                return Utf8FromWide(std::wstring_view(buffer.data(), length));
            }
            buffer.resize(buffer.size() * 2);
        }
#elif defined(__APPLE__)
        std::uint32_t size = 0;
        (void)_NSGetExecutablePath(nullptr, &size);
        if (size == 0)
        {
            return std::nullopt;
        }
        std::vector<char> buffer(size);
        if (_NSGetExecutablePath(buffer.data(), &size) != 0)
        {
            return std::nullopt;
        }
        std::array<char, PATH_MAX> resolved{};
        if (realpath(buffer.data(), resolved.data()) != nullptr)
        {
            return std::string(resolved.data());
        }
        return std::string(buffer.data());
#else
        std::vector<char> buffer(512);
        for (;;)
        {
            const ssize_t length = readlink("/proc/self/exe", buffer.data(), buffer.size());
            if (length < 0)
            {
                return std::nullopt;
            }
            if (static_cast<std::size_t>(length) < buffer.size())
            {
                return std::string(buffer.data(), static_cast<std::size_t>(length));
            }
            buffer.resize(buffer.size() * 2);
        }
#endif
    }

    [[nodiscard]] std::filesystem::path PathFromUtf8(std::string_view value)
    {
#if defined(__cpp_char8_t)
        std::u8string converted;
        converted.reserve(value.size());
        for (unsigned char ch : value)
        {
            converted.push_back(static_cast<char8_t>(ch));
        }
        return std::filesystem::path(converted);
#else
        return std::filesystem::u8path(value.begin(), value.end());
#endif
    }

    [[nodiscard]] std::string PathToUtf8(const std::filesystem::path& path)
    {
#if defined(__cpp_char8_t)
        const std::u8string value = path.u8string();
        std::string result;
        result.reserve(value.size());
        for (char8_t ch : value)
        {
            result.push_back(static_cast<char>(ch));
        }
        return result;
#else
        return path.u8string();
#endif
    }

    [[nodiscard]] std::string AppBaseDirectory()
    {
        const std::optional<std::string> path = ProcessPath();
        if (!path.has_value())
        {
            throw std::runtime_error("process path is unavailable");
        }
        std::filesystem::path directory = PathFromUtf8(*path).parent_path();
        directory = std::filesystem::absolute(directory);
        std::string result = PathToUtf8(directory);
        if (!result.empty() && result.back() != std::filesystem::path::preferred_separator)
        {
            result.push_back(std::filesystem::path::preferred_separator);
        }
        return result;
    }

    [[nodiscard]] std::string MachineName()
    {
#if defined(_WIN32)
        DWORD size = 256;
        std::vector<wchar_t> buffer(size);
        for (;;)
        {
            DWORD actual = size;
            if (GetComputerNameW(buffer.data(), &actual) != FALSE)
            {
                return Utf8FromWide(std::wstring_view(buffer.data(), actual));
            }
            if (GetLastError() != ERROR_BUFFER_OVERFLOW)
            {
                throw std::runtime_error("GetComputerNameW failed");
            }
            size *= 2;
            buffer.resize(size);
        }
#else
        std::array<char, 256> buffer{};
        if (gethostname(buffer.data(), buffer.size()) != 0)
        {
            throw std::runtime_error("gethostname failed");
        }
        buffer.back() = '\0';
        return std::string(buffer.data());
#endif
    }

    [[nodiscard]] std::string FullPathCombine(
        std::string_view directory, std::string_view value)
    {
        const std::filesystem::path combined = PathFromUtf8(directory) / PathFromUtf8(value);
        return PathToUtf8(std::filesystem::absolute(combined).lexically_normal());
    }

    [[nodiscard]] std::string CombinePath(std::string_view left, std::string_view right)
    {
        return PathToUtf8(PathFromUtf8(left) / PathFromUtf8(right));
    }

    [[maybe_unused, nodiscard]] std::string FileNameWithoutExtension(const std::string& path)
    {
        return PathToUtf8(PathFromUtf8(path).stem());
    }

    void WriteLine(std::string_view value)
    {
        std::cout << value << '\n';
    }

    void SetExitCode(std::int32_t value) noexcept
    {
        MphRead::NativeRuntime::SetEnvironmentExitCode(value);
    }

    [[nodiscard]] std::size_t DotNetUtf16Length(std::string_view value) noexcept
    {
        // Native strings are UTF-8; C# alignment pads by System.String.Length,
        // i.e. UTF-16 code units rather than UTF-8 bytes or Unicode scalars.
        std::size_t units = 0;
        for (std::size_t i = 0; i < value.size();)
        {
            const unsigned char lead = static_cast<unsigned char>(value[i]);
            std::uint32_t codePoint = lead;
            std::size_t length = 1;
            if ((lead & 0xE0) == 0xC0 && i + 1 < value.size())
            {
                codePoint = lead & 0x1F;
                length = 2;
            }
            else if ((lead & 0xF0) == 0xE0 && i + 2 < value.size())
            {
                codePoint = lead & 0x0F;
                length = 3;
            }
            else if ((lead & 0xF8) == 0xF0 && i + 3 < value.size())
            {
                codePoint = lead & 0x07;
                length = 4;
            }
            for (std::size_t j = 1; j < length; ++j)
            {
                const unsigned char next = static_cast<unsigned char>(value[i + j]);
                if ((next & 0xC0) != 0x80)
                {
                    length = 1;
                    codePoint = lead;
                    break;
                }
                codePoint = (codePoint << 6) | (next & 0x3F);
            }
            units += codePoint > 0xFFFF ? 2 : 1;
            i += length;
        }
        return units;
    }

    [[nodiscard]] std::string LeftAlign(std::string_view value, std::size_t width)
    {
        const std::size_t length = DotNetUtf16Length(value);
        if (length >= width)
        {
            return std::string(value);
        }
        std::string result(value);
        result.append(width - length, ' ');
        return result;
    }

    [[nodiscard]] std::pair<int, int> ParseSize(const std::vector<std::string>& args)
    {
        std::optional<std::string> value = ValueAfter(args, "size");
        if (value.has_value())
        {
            std::vector<std::string_view> parts;
            std::size_t start = 0;
            for (std::size_t i = 0; i <= value->size(); ++i)
            {
                if (i == value->size() || (*value)[i] == 'x' || (*value)[i] == 'X')
                {
                    parts.emplace_back(value->data() + start, i - start);
                    start = i + 1;
                }
            }
            std::int32_t width = 0;
            std::int32_t height = 0;
            if (parts.size() == 2 && TryParseInt32(parts[0], width)
                && TryParseInt32(parts[1], height) && width > 0 && height > 0)
            {
                return {width, height};
            }
            WriteLine("[thumbnails] ignoring -size " + *value
                + " (expected e.g. 1920x1440)");
        }
        return {MphRead::Mods::ThumbnailGenerator::ThumbnailWidth,
            MphRead::Mods::ThumbnailGenerator::ThumbnailHeight};
    }

    [[nodiscard]] int ParsePort(const std::vector<std::string>& args)
    {
        const std::optional<std::string> value = ValueAfter(args, "port");
        std::int32_t port = 0;
        if (value.has_value() && TryParseInt32(*value, port))
        {
            return port;
        }
        return MphRead::Mods::Network::NetConfig::DefaultPort;
    }

    [[nodiscard]] std::string ParseName(const std::vector<std::string>& args)
    {
        const std::optional<std::string> value = ValueAfter(args, "name");
        return value.has_value() ? *value : MachineName();
    }

    [[nodiscard]] Hunter ParseHunter(const std::vector<std::string>& args)
    {
        Hunter hunter = Hunter::Samus;
        (void)TryParseHunter(ValueAfter(args, "hunter"), hunter);
        return hunter;
    }

    [[nodiscard]] int ParseRecolor(const std::vector<std::string>& args)
    {
        const std::optional<std::string> value = ValueAfter(args, "recolor");
        std::int32_t recolor = 0;
        if (value.has_value() && TryParseInt32(*value, recolor))
        {
            return recolor;
        }
        return 0;
    }

    void ApplyRenderOverrides(const std::vector<std::string>& args)
    {
        using MphRead::Features;
        using MphRead::Mods::RenderOptions;
        using MphRead::Mods::Render::Crosshair;
        using MphRead::Mods::Render::FrameTiming;

        const std::optional<std::string> cel = ValueAfter(args, "cel");
        if (cel.has_value() && !StartsWithHyphen(cel))
        {
            RenderOptions::CelShading(
                RenderOptions::ParseOnOff(*cel, RenderOptions::CelShading()));
        }
        else if (HasFlag(args, "cel"))
        {
            RenderOptions::CelShading(true);
        }

        const std::optional<std::string> fog = ValueAfter(args, "fog");
        if (fog.has_value() && !StartsWithHyphen(fog))
        {
            RenderOptions::Fog(RenderOptions::ParseOnOff(*fog, RenderOptions::Fog()));
        }

        const std::optional<std::string> fps = ValueAfter(args, "fps");
        if (fps.has_value() && !StartsWithHyphen(fps))
        {
            RenderOptions::ShowFps(RenderOptions::ParseOnOff(*fps, RenderOptions::ShowFps()));
        }
        else if (HasFlag(args, "fps"))
        {
            RenderOptions::ShowFps(true);
        }

        const std::optional<std::string> fpsCap = ValueAfter(args, "fpscap");
        if (fpsCap.has_value() && !StartsWithHyphen(fpsCap))
        {
            FrameTiming::FrameRateCap(
                FrameTiming::ParseCap(*fpsCap, FrameTiming::FrameRateCap()));
        }

        const std::optional<std::string> bands = ValueAfter(args, "celbands");
        std::int32_t parsedBands = 0;
        if (bands.has_value() && TryParseInt32(*bands, parsedBands))
        {
            RenderOptions::CelBands(parsedBands);
        }

        const std::optional<std::string> edge = ValueAfter(args, "celedge");
        if (edge.has_value())
        {
            std::string trimmed = *edge;
            while (!trimmed.empty() && trimmed.back() == '%')
            {
                trimmed.pop_back();
            }
            std::int32_t edgePercent = 0;
            if (TryParseInt32(trimmed, edgePercent))
            {
                RenderOptions::CelEdge(edgePercent / 100.0f);
            }
        }

        const std::optional<std::string> proHud = ValueAfter(args, "prohud");
        if (proHud.has_value() && !StartsWithHyphen(proHud))
        {
            Features::ProHud(RenderOptions::ParseOnOff(*proHud, Features::ProHud()));
        }
        else if (HasFlag(args, "prohud"))
        {
            Features::ProHud(true);
        }

        const std::optional<std::string> weaponStyle = ValueAfter(args, "weaponstyle");
        if (weaponStyle.has_value() && !StartsWithHyphen(weaponStyle))
        {
            if (EqualsOrdinalIgnoreCase(*weaponStyle, "static")
                || EqualsOrdinalIgnoreCase(*weaponStyle, "quake"))
            {
                Features::ProHudFixedWeapon(true);
                Features::FixedWeapon(true);
            }
            else if (EqualsOrdinalIgnoreCase(*weaponStyle, "dynamic")
                || EqualsOrdinalIgnoreCase(*weaponStyle, "metroid"))
            {
                Features::ProHudFixedWeapon(false);
                Features::FixedWeapon(false);
            }
        }

        const std::optional<std::string> crosshair = ValueAfter(args, "crosshair");
        if (crosshair.has_value() && !StartsWithHyphen(crosshair))
        {
            Crosshair::Style(Crosshair::ParseStyle(*crosshair, Crosshair::Style()));
        }

        const std::optional<std::string> crosshairSize = ValueAfter(args, "crosshairsize");
        if (crosshairSize.has_value() && !StartsWithHyphen(crosshairSize))
        {
            Crosshair::Size(Crosshair::ParseSize(*crosshairSize, Crosshair::Size()));
        }
    }

#if defined(MPHREAD_SERVER)
    void ServerUsage()
    {
        using MphRead::Mods::Branding;
        using MphRead::Mods::ConsoleWindow;
        using MphRead::Mods::Network::NetConfig;
        using MphRead::Mods::Network::NetMasterConfig;

        const std::optional<std::string> processPath = ProcessPath();
        std::string executable = processPath.has_value()
            ? FileNameWithoutExtension(*processPath)
            : std::string("MphReadServer");

        WriteLine("");
        WriteLine(std::string(Branding::Name) + " dedicated server. It needs no game files.");
        WriteLine("");
        WriteLine("  " + executable + " -server -port " + std::to_string(NetConfig::DefaultPort)
            + " -players 8 -servername \"My server\"");
        WriteLine("      run a server. Maps come from maprotation.txt, written");
        WriteLine("      beside this program on first run.");
        WriteLine("");
        WriteLine("  " + executable + " -masterserver -port "
            + std::to_string(NetMasterConfig::DefaultPort));
        WriteLine("      run a server directory of your own.");
        WriteLine("");
        WriteLine("  " + executable + " -servers");
        WriteLine("      list the servers that are up right now.");
        WriteLine("");
        WriteLine("A server lists itself on " + std::string(NetMasterConfig::DefaultHost)
            + " so players can find it;");
        WriteLine("-nomaster keeps it off every list. See SERVER.txt.");
        WriteLine("");
#if defined(_WIN32)
        if (ConsoleWindow::OwnsItsConsole())
        {
            WriteLine("Press any key to close this window...");
            (void)_getche();
        }
#endif
    }
#endif

    void ListServers(const std::string& masterHost,
        const std::optional<std::string>& portValue)
    {
        using namespace MphRead::Mods::Network;

        int port = NetMasterConfig::DefaultPort;
        std::int32_t parsedPort = 0;
        if (portValue.has_value() && TryParseInt32(*portValue, parsedPort))
        {
            port = parsedPort;
        }
        WriteLine("[servers] asking " + masterHost + ":" + std::to_string(port));
        const auto result = NetMasterClient::Query(masterHost, port);
        if (!result.Answered)
        {
            WriteLine("[servers] no answer from " + masterHost + ":" + std::to_string(port)
                + " -- it may be down, or UDP may not reach it");
            return;
        }
        if (result.Servers.empty())
        {
            WriteLine("[servers] the directory is up and has nobody listed");
            return;
        }
        WriteLine("[servers] " + std::to_string(result.Servers.size())
            + " listed; asking each one");
        for (const auto& listing : result.Servers)
        {
            const auto status = NetStatus::Query(listing.Address, listing.Port,
                false /* allowJoinProbe */);
            std::string name;
            if (!status.ServerName.empty())
            {
                name = status.ServerName;
            }
            else if (!listing.ServerName.empty())
            {
                name = listing.ServerName;
            }
            else
            {
                name = listing.Endpoint;
            }
            if (!status.Online)
            {
                WriteLine("  " + LeftAlign(name, 24) + " "
                    + LeftAlign(listing.Endpoint, 26) + " did not answer");
                continue;
            }
            const std::string players = status.MaxPlayers > 0
                ? std::to_string(status.Players) + "/" + std::to_string(status.MaxPlayers)
                : std::to_string(status.Players);
            const std::string ping = status.Latency >= 0
                ? std::to_string(status.Latency) + " ms"
                : std::string("-- ms");
            WriteLine("  " + LeftAlign(name, 24) + " "
                + LeftAlign(listing.Endpoint, 26) + " "
                + LeftAlign(status.RoomKey, 20) + " "
                + LeftAlign(NetStatus::ModeName(status.Mode), 14) + " "
                + LeftAlign(players, 6) + " " + ping);
        }
    }

#if defined(_MSC_VER)
    __declspec(noinline)
#elif defined(__GNUC__) || defined(__clang__)
    __attribute__((noinline))
#endif
    int RunUiCapture(const std::string& directory)
    {
#if defined(MPHREAD_AVALONIA)
        try
        {
            return MphRead::Mods::Launcher::Gui::UiCapture::Run(directory);
        }
        catch (const std::exception& ex)
        {
            WriteLine(std::string("[uishot] no launcher toolkit here: ") + ex.what());
            return 1;
        }
#else
        (void)directory;
        WriteLine("[uishot] this build has no Avalonia launcher");
        return 1;
#endif
    }

    void GenerateThumbnails(const std::vector<std::string>& args, int width, int height)
    {
        using MphRead::Mods::ThumbnailBatch;
        using MphRead::Mods::ThumbnailGenerator;

        const bool force = HasFlag(args, "force");
        std::vector<std::string> rooms = force
            ? ThumbnailGenerator::MultiplayerRooms()
            : ThumbnailGenerator::MissingThumbnails();
        if (rooms.empty())
        {
            WriteLine("[thumbnails] all previews already present in "
                + ThumbnailGenerator::CacheDirectory());
            WriteLine("[thumbnails] pass -force to re-render them");
            return;
        }
        int jobs = ThumbnailBatch::DefaultParallelism;
        const std::optional<std::string> jobsValue = ValueAfter(args, "jobs");
        std::int32_t parsedJobs = 0;
        if (jobsValue.has_value() && TryParseInt32(*jobsValue, parsedJobs))
        {
            jobs = parsedJobs;
        }
        WriteLine("[thumbnails] rendering " + std::to_string(rooms.size())
            + " preview(s) at " + std::to_string(width) + "x" + std::to_string(height)
            + ", " + std::to_string(jobs) + " at a time");
        WriteLine("[thumbnails] output: " + ThumbnailGenerator::CacheDirectory());
        const int written = ThumbnailBatch::Run(rooms, jobs, width, height);
        WriteLine("[thumbnails] done -- " + std::to_string(written) + "/"
            + std::to_string(rooms.size()) + " written");
    }
}

namespace MphRead::Mods
{
    bool ModEntry::TryHandleHeadless(const std::vector<std::string>& args)
    {
        InputSettings::Load();
        Launcher::LauncherPrefs::Load();
        if (HasFlag(args, "debuglog"))
        {
            DebugLog::Force();
        }
        DebugLog::Attach();

        Update::Updater::Disabled(HasFlag(args, "noupdate"));
        ApplyRenderOverrides(args);

        const int applyAt = IndexOfFlag(args, Update::DesktopUpdate::ApplyFlag);
        if (applyAt >= 0 && static_cast<std::size_t>(applyAt + 2) < args.size())
        {
            std::vector<std::string> relaunch;
            int separator = -1;
            for (std::size_t i = static_cast<std::size_t>(applyAt + 3); i < args.size(); ++i)
            {
                if (args[i] == Update::DesktopUpdate::RelaunchSeparator)
                {
                    separator = static_cast<int>(i);
                    break;
                }
            }
            for (int i = separator + 1; separator >= 0 && static_cast<std::size_t>(i) < args.size(); ++i)
            {
                relaunch.push_back(args[static_cast<std::size_t>(i)]);
            }
            std::int32_t waitFor = -1;
            if (!TryParseInt32(args[static_cast<std::size_t>(applyAt + 2)], waitFor))
            {
                waitFor = -1;
            }
            SetExitCode(Update::DesktopUpdate::Apply(
                args[static_cast<std::size_t>(applyAt + 1)], waitFor, relaunch));
            return true;
        }

        Update::DesktopUpdate::Clean();
        Update::UpdateInstall::UseDesktopIfPossible();

        const std::optional<std::string> netLag = ValueAfter(args, "netlag");
        if (netLag.has_value() && !Network::NetLag::Configure(*netLag))
        {
            WriteLine("[net] -netlag " + *netLag
                + " is not a number of milliseconds (try -netlag 200 or -netlag 200:40)");
            return true;
        }
        const std::optional<std::string> netLoss = ValueAfter(args, "netloss");
        if (netLoss.has_value() && !Network::NetLag::ConfigureLoss(*netLoss))
        {
            WriteLine("[net] -netloss " + *netLoss + " is not a percentage");
            return true;
        }
        if (Network::NetLag::Active())
        {
            const std::optional<std::string> description = Network::NetLag::Describe();
            WriteLine("[net] simulating a bad line: " + description.value_or(""));
        }
        if (HasFlag(args, "nounlagged"))
        {
            Network::NetUnlagged::Enabled(false);
            WriteLine("[net] lag compensation off: shots resolve against the present");
        }
        if (HasFlag(args, "nohitprediction"))
        {
            Network::NetHitPrediction::Enabled(false);
            WriteLine("[net] hit prediction off: a client's hits land when the authority says so");
        }
        if (HasFlag(args, "nohitmarker"))
        {
            Network::NetHitPrediction::MarkerEnabled(false);
            WriteLine("[hud] hit marker off");
        }
        if (HasFlag(args, "deathprediction"))
        {
            Network::NetHitPrediction::DeathEnabled(true);
            WriteLine("[net] death prediction on: a client's kills land the frame it lands them");
        }
        if (HasFlag(args, "nodeathprediction"))
        {
            Network::NetHitPrediction::DeathEnabled(false);
            WriteLine("[net] death prediction off: a client's kills land when the authority says so");
        }
        if (HasFlag(args, "credits"))
        {
            Credits::Print();
            return true;
        }

        const std::optional<std::string> mapDir = ValueAfter(args, "mapdir");
        if (mapDir.has_value())
        {
            MapGen::CustomRooms::MapDirectory(
                FullPathCombine(ConsoleSetup::LaunchDirectory(), *mapDir));
        }

        if (HasFlag(args, "mapbundle"))
        {
            const std::optional<std::string> which = ValueAfter(args, "mapbundle");
            const std::optional<std::string> outPath = ValueAfter(args, "out");
            int cooked = 0;
            int failed = 0;
            for (const auto& definition : MapGen::CustomRooms::Definitions())
            {
                if (which.has_value() && !EqualsOrdinalIgnoreCase(*which, definition.Name)
                    && !EqualsOrdinalIgnoreCase(*which, "all"))
                {
                    continue;
                }
                if (!definition.SourcePath.has_value() || definition.BundlePath.has_value()
                    || !definition.Import.has_value())
                {
                    continue;
                }
                try
                {
                    MapGen::MapBundle::Cook(definition, *definition.SourcePath, outPath);
                    ++cooked;
                }
                catch (const std::exception& ex)
                {
                    WriteLine(definition.Name + ": " + ex.what());
                    ++failed;
                }
            }
            if (cooked == 0 && failed == 0)
            {
                WriteLine("No map to bundle. A bundle is cooked from a recipe and the level it converts; put both in "
                    + MapGen::CustomRooms::MapDirectory() + ".");
            }
            SetExitCode(failed == 0 ? 0 : 1);
            return true;
        }

        if (HasFlag(args, "update"))
        {
            Update::Updater::Disabled(false);
            const std::optional<Update::UpdateInfo> update = Update::Updater::Check();
            if (!update.has_value())
            {
                WriteLine("[update] " + Update::UpdateCheck::LastReason());
                return true;
            }
            WriteLine("[update] " + Update::Updater::Describe(*update));
            WriteLine("[update] " + update->PageUrl);
            (void)Update::Updater::OpenPage(*update);
            return true;
        }

        bool doubleClicked = args.empty();
#if !defined(_WIN32) && !defined(__APPLE__)
        doubleClicked = false;
#endif
#if defined(MPHREAD_SERVER)
        doubleClicked = false;
#endif
        if ((HasFlag(args, "launcher") || doubleClicked) && !HasFlag(args, "menu"))
        {
#if defined(MPHREAD_AVALONIA)
            if (!HasFlag(args, "text") && Launcher::Gui::GuiLauncher::TryRun())
            {
                return true;
            }
#if defined(_WIN32)
            ConsoleWindow::Show();
#endif
#endif
            Launcher::TextLauncher::Run();
            return true;
        }

#if defined(MPHREAD_SERVER)
        if (args.empty())
        {
            ServerUsage();
            return true;
        }
#endif

        if (HasFlag(args, "masterserver") || HasFlag(args, "server")
            || HasFlag(args, "dedicated"))
        {
            Update::ServerUpdate::Enabled(!HasFlag(args, "noautoupdate"));
            std::vector<std::string> commandLine = GetCommandLineArguments();
            std::vector<std::string> typed;
            if (commandLine.size() > 1)
            {
                typed.assign(commandLine.begin() + 1, commandLine.end());
            }
            if (Update::ServerUpdate::AtStartup(typed))
            {
                return true;
            }
        }

        if (HasFlag(args, "masterserver"))
        {
            int masterPort = Network::NetMasterConfig::DefaultPort;
            std::optional<std::string> masterPortValue = ValueAfter(args, "port");
            if (!masterPortValue.has_value())
            {
                masterPortValue = ValueAfter(args, "masterport");
            }
            std::int32_t parsedMasterPort = 0;
            if (masterPortValue.has_value() && TryParseInt32(*masterPortValue, parsedMasterPort))
            {
                masterPort = parsedMasterPort;
            }
            Network::MasterServer master(masterPort);
            ShutdownSignals signals;

            const std::string hostPorts = ValueAfter(args, "hostports").value_or("27900-27919");
            if (!EqualsOrdinalIgnoreCase(hostPorts, "none"))
            {
                const std::size_t dash = hostPorts.find('-');
                std::string firstText;
                std::string lastText;
                if (dash == std::string::npos)
                {
                    firstText = hostPorts;
                }
                else
                {
                    firstText = hostPorts.substr(0, dash);
                    lastText = hostPorts.substr(dash + 1);
                }
                std::int32_t first = 0;
                std::int32_t last = 0;
                if (dash != std::string::npos && TryParseInt32(firstText, first)
                    && TryParseInt32(lastText, last) && first > 0 && last >= first)
                {
                    master.SetHostPorts(first, last);
                }
                else
                {
                    WriteLine("[master] ignoring -hostports " + hostPorts
                        + " (expected e.g. 27900-27919, or none)");
                }
            }

            std::optional<std::string> publicHost = ValueAfter(args, "public");
            if (!publicHost.has_value())
            {
                publicHost = ValueAfter(args, "publicaddress");
            }
            if (publicHost.has_value())
            {
                master.SetPublicAddress(*publicHost);
            }

            std::stop_source cancel;
            signals.OnShutdown([&cancel, &master]()
            {
                cancel.request_stop();
                master.Stop();
            });
            master.Run(cancel.get_token());
            return true;
        }

        if (HasFlag(args, "servers"))
        {
            ListServers(ValueAfter(args, "master").value_or(
                std::string(Network::NetMasterConfig::DefaultHost)),
                ValueAfter(args, "masterport"));
            return true;
        }

        if (!HasFlag(args, "server") && !HasFlag(args, "dedicated"))
        {
            return false;
        }

        int port = Network::NetConfig::DefaultPort;
        const std::optional<std::string> portValue = ValueAfter(args, "port");
        std::int32_t parsedPort = 0;
        if (portValue.has_value() && TryParseInt32(*portValue, parsedPort))
        {
            port = parsedPort;
        }
        int maxPlayers = 4;
        const std::optional<std::string> playersValue = ValueAfter(args, "players");
        std::int32_t parsedPlayers = 0;
        if (playersValue.has_value() && TryParseInt32(*playersValue, parsedPlayers))
        {
            maxPlayers = parsedPlayers;
        }
        std::string rotationPath;
        if (const std::optional<std::string> value = ValueAfter(args, "rotation"); value.has_value())
        {
            rotationPath = *value;
        }
        else
        {
            rotationPath = CombinePath(AppBaseDirectory(), "maprotation.txt");
        }
        Network::MapRotation rotation = Network::MapRotation::LoadOrCreate(rotationPath);
        Network::DedicatedServer server(port, maxPlayers, rotation);
        std::string serverName;
        if (const std::optional<std::string> value = ValueAfter(args, "servername"); value.has_value())
        {
            serverName = *value;
        }
        else if (const std::optional<std::string> value = ValueAfter(args, "name"); value.has_value())
        {
            serverName = *value;
        }
        else
        {
            serverName = MachineName();
        }
        server.ServerName(serverName);
        server.FriendlyFire(HasFlag(args, "friendlyfire"));
        server.ShadowFreeze(!HasFlag(args, "noshadowfreeze"));
        server.AllowMapVotes(!HasFlag(args, "novote"));
        server.AutoUpdate(true);
        server.Simulate(HasFlag(args, "simulate") || HasFlag(args, "authority"));

        if (!HasFlag(args, "nomaster") && !HasFlag(args, "unlisted"))
        {
            const std::string masterHost = ValueAfter(args, "master").value_or(
                std::string(Network::NetMasterConfig::DefaultHost));
            int reportPort = Network::NetMasterConfig::DefaultPort;
            const std::optional<std::string> reportPortValue = ValueAfter(args, "masterport");
            std::int32_t parsedReportPort = 0;
            if (reportPortValue.has_value() && TryParseInt32(*reportPortValue, parsedReportPort))
            {
                reportPort = parsedReportPort;
            }
            server.Reporter(std::make_unique<Network::MasterReporter>(masterHost, reportPort));
            WriteLine("[server] listing on " + masterHost + ":" + std::to_string(reportPort)
                + " as \"" + server.ServerName() + "\" (-nomaster to stay private)");
        }

        std::stop_source cancel;
        ShutdownSignals signals;
        signals.OnShutdown([&cancel, &server]()
        {
            cancel.request_stop();
            server.Stop();
        });
        server.Run(cancel.get_token());
        return true;
    }

    bool ModEntry::TryHandle(const std::vector<std::string>& args)
    {
        const auto [width, height] = ParseSize(args);

        if (!HasFlag(args, "mapgen"))
        {
            MapGen::CustomRooms::GenerateMissing();
        }

        if (HasFlag(args, "fullscreen") || HasFlag(args, "borderless"))
        {
            WindowMode::Startup(WindowStartMode::BorderlessFullscreen);
        }
        else if (HasFlag(args, "windowed"))
        {
            WindowMode::Startup(WindowStartMode::Windowed);
        }

        if (HasFlag(args, "nohelmet"))
        {
            Features::HelmetOpacity(0);
            Features::VisorOpacity(0);
        }
        if (HasFlag(args, "netdebug"))
        {
            Network::NetDiagnostics::Enabled(true);
            Network::MapAudit::Diagnostic(true);
        }

        if (HasFlag(args, "mechanics"))
        {
            Network::MechanicsDump::Run();
            return true;
        }

        if (HasFlag(args, "gamepad"))
        {
            double seconds = 15;
            const std::optional<std::string> given = ValueAfter(args, "seconds");
            double parsed = 0;
            if (given.has_value() && TryParseDoubleCurrent(*given, parsed) && parsed > 0)
            {
                seconds = parsed;
            }
            SetExitCode(Input::GamepadProbe::Run(seconds));
            return true;
        }

        if (HasFlag(args, "rooms"))
        {
            for (const std::string& room : ThumbnailGenerator::MultiplayerRooms())
            {
                WriteLine(room);
            }
            return true;
        }

        const std::optional<std::string> dpsTest = ValueAfter(args, "dpstest");
        if (dpsTest.has_value())
        {
            Hunter dpsHunter = Hunter::Sylux;
            (void)TryParseHunter(ValueAfter(args, "hunter"), dpsHunter);
            BeamType dpsBeam = BeamType::ShockCoil;
            (void)TryParseBeam(ValueAfter(args, "weapon"), dpsBeam);
            double dpsSeconds = 10;
            double parsedSeconds = 0;
            if (TryParseDoubleInvariant(ValueAfter(args, "seconds"), parsedSeconds))
            {
                dpsSeconds = parsedSeconds;
            }
            float dpsDistance = 2.2f;
            float parsedDistance = 0;
            if (TryParseFloatInvariant(ValueAfter(args, "distance"), parsedDistance))
            {
                dpsDistance = parsedDistance;
            }
            SetExitCode(Network::WeaponDps::Run(*dpsTest, dpsHunter, dpsBeam,
                dpsSeconds, dpsDistance, HasFlag(args, "bombs")));
            return true;
        }

        if (HasFlag(args, "mapgen"))
        {
            const std::optional<std::string> only = ValueAfter(args, "mapgen");
            const bool force = HasFlag(args, "force");
            (void)force;
            int count = 0;
            int failed = 0;
            for (const auto& definition : MapGen::CustomRooms::Definitions())
            {
                if (only.has_value() && !EqualsOrdinalIgnoreCase(*only, definition.Name)
                    && !EqualsOrdinalIgnoreCase(*only, "all"))
                {
                    continue;
                }
                try
                {
                    MapGen::MapPacker::Generate(definition,
                        MapGen::CustomRooms::ArchiveDirectory(definition),
                        MapGen::CustomRooms::EntityDirectory(),
                        MapGen::CustomRooms::NodeDirectory(), true /* verbose */);
                    ++count;
                }
                catch (const std::exception& ex)
                {
                    WriteLine(definition.Name + ": " + ex.what());
                    ++failed;
                }
            }
            if (count == 0 && failed == 0)
            {
                WriteLine("No maps to generate. Put a map JSON in "
                    + MapGen::CustomRooms::MapDirectory() + ".");
            }
            SetExitCode(failed == 0 ? 0 : 1);
            return true;
        }

        const std::optional<std::string> q3Maps = ValueAfter(args, "q3maps");
        if (q3Maps.has_value())
        {
            try
            {
                for (const std::string& map : MapGen::Q3Bsp::ListMaps(*q3Maps))
                {
                    WriteLine(map);
                }
                SetExitCode(0);
            }
            catch (const std::exception& ex)
            {
                WriteLine("Could not read " + *q3Maps + ": " + ex.what());
                SetExitCode(1);
            }
            return true;
        }

        const std::optional<std::string> q3Convert = ValueAfter(args, "q3convert");
        if (q3Convert.has_value())
        {
            std::optional<float> scale;
            float parsedScale = 0;
            if (TryParseFloatInvariant(ValueAfter(args, "scale"), parsedScale,
                false /* NumberStyles.Float has no thousands */) && parsedScale > 0)
            {
                scale = parsedScale;
            }
            int textureSize = MapGen::MapTextureBake::DefaultSize;
            const std::optional<std::string> texSizeValue = ValueAfter(args, "texsize");
            std::int32_t parsedTextureSize = 0;
            if (texSizeValue.has_value() && TryParseInt32(*texSizeValue, parsedTextureSize)
                && parsedTextureSize >= 8 && parsedTextureSize <= 256)
            {
                textureSize = parsedTextureSize;
            }
            try
            {
                SetExitCode(MapGen::Q3Convert::Run(*q3Convert,
                    ValueAfter(args, "map"), ValueAfter(args, "name"),
                    ValueAfter(args, "out"), HasFlag(args, "noclip"), scale,
                    textureSize));
            }
            catch (const std::exception& ex)
            {
                WriteLine("Could not convert " + *q3Convert + ": " + ex.what());
                SetExitCode(1);
            }
            return true;
        }

        const std::optional<std::string> q3Shaders = ValueAfter(args, "q3shaders");
        if (q3Shaders.has_value())
        {
            SetExitCode(MapGen::MapReport::ListShaders(*q3Shaders, ValueAfter(args, "map")));
            return true;
        }

        const std::optional<std::string> mapMaterials = ValueAfter(args, "mapmaterials");
        if (mapMaterials.has_value())
        {
            SetExitCode(MapGen::MapReport::ListMaterials(*mapMaterials));
            return true;
        }

        const std::optional<std::string> uiShot = ValueAfter(args, "uishot");
        if (uiShot.has_value())
        {
            SetExitCode(RunUiCapture(*uiShot));
            return true;
        }

        if (HasFlag(args, "frametimingcheck"))
        {
            SetExitCode(Render::FrameTimingCheck::Run());
            return true;
        }

        const std::optional<std::string> simCheck = ValueAfter(args, "simcheck");
        if (simCheck.has_value())
        {
            int players = 8;
            const std::optional<std::string> playersValue = ValueAfter(args, "players");
            std::int32_t parsedPlayers = 0;
            if (playersValue.has_value() && TryParseInt32(*playersValue, parsedPlayers))
            {
                players = parsedPlayers;
            }
            double seconds = 10;
            double parsedSeconds = 0;
            if (TryParseDoubleInvariant(ValueAfter(args, "seconds"), parsedSeconds))
            {
                seconds = parsedSeconds;
            }
            GameMode mode = GameMode::Battle;
            (void)TryParseGameMode(ValueAfter(args, "mode"), mode);
            SetExitCode(Network::ServerSimCheck::Run(*simCheck, players, seconds, mode));
            return true;
        }

        const std::optional<std::string> mapTest = ValueAfter(args, "maptest");
        if (mapTest.has_value())
        {
            int players = 8;
            const std::optional<std::string> playersValue = ValueAfter(args, "players");
            std::int32_t parsedPlayers = 0;
            if (playersValue.has_value() && TryParseInt32(*playersValue, parsedPlayers))
            {
                players = parsedPlayers;
            }
            double seconds = 10;
            double parsedSeconds = 0;
            if (TryParseDoubleInvariant(ValueAfter(args, "seconds"), parsedSeconds))
            {
                seconds = parsedSeconds;
            }
            GameMode mode = GameMode::Battle;
            (void)TryParseGameMode(ValueAfter(args, "mode"), mode);
            Network::MapAudit::ShowWindow(HasFlag(args, "hudshots"));
            if (ValueAfter(args, "hunter").has_value())
            {
                Network::MapAudit::MainHunter(ParseHunter(args));
            }
            else
            {
                Network::MapAudit::MainHunter(std::nullopt);
            }
            const std::optional<std::string> drawRateValue = ValueAfter(args, "drawrate");
            std::int32_t drawRate = 0;
            if (drawRateValue.has_value() && TryParseInt32(*drawRateValue, drawRate)
                && drawRate > 0)
            {
                Network::MapAudit::DrawRate(drawRate);
            }
            const std::optional<std::string> sizeValue = ValueAfter(args, "size");
            if (sizeValue.has_value())
            {
                const std::string lowered = ToLowerAscii(*sizeValue);
                std::vector<std::string_view> split;
                std::size_t start = 0;
                for (std::size_t i = 0; i <= lowered.size(); ++i)
                {
                    if (i == lowered.size() || lowered[i] == 'x')
                    {
                        split.emplace_back(lowered.data() + start, i - start);
                        start = i + 1;
                    }
                }
                std::int32_t auditWidth = 0;
                std::int32_t auditHeight = 0;
                if (split.size() == 2 && TryParseInt32(split[0], auditWidth)
                    && TryParseInt32(split[1], auditHeight)
                    && auditWidth > 0 && auditHeight > 0)
                {
                    Network::MapAudit::WindowSize(auditWidth, auditHeight);
                }
            }
            SetExitCode(Network::MapAudit::Run(*mapTest, players, seconds, mode,
                HasFlag(args, "bots"), ValueAfter(args, "shots"),
                HasFlag(args, "renderprobe"), HasFlag(args, "allnodes"),
                HasFlag(args, "itemshots")));
            return true;
        }

        const std::optional<std::string> hostGame = ValueAfter(args, "hostgame");
        if (hostGame.has_value())
        {
            const std::string masterHost = ValueAfter(args, "master").value_or(
                std::string(Network::NetMasterConfig::DefaultHost));
            int masterPort = Network::NetMasterConfig::DefaultPort;
            const std::optional<std::string> masterPortValue = ValueAfter(args, "masterport");
            std::int32_t parsedMasterPort = 0;
            if (masterPortValue.has_value() && TryParseInt32(*masterPortValue, parsedMasterPort))
            {
                masterPort = parsedMasterPort;
            }
            GameMode hostMode = GameMode::Battle;
            (void)TryParseGameMode(ValueAfter(args, "mode"), hostMode);
            const std::string hostName = ParseName(args);
            WriteLine("[net] asking " + masterHost + ":" + std::to_string(masterPort)
                + " to run " + *hostGame);
            const auto game = Network::NetMasterClient::RequestGame(masterHost,
                masterPort, *hostGame, hostMode, 420, 7,
                Entities::PlayerEntity::SlotCapacity, hostName + "'s game");
            if (!game.Started)
            {
                WriteLine("[net] it would not: " + game.Reason);
                SetExitCode(1);
                return true;
            }
            WriteLine("[net] running on " + game.Host + ":" + std::to_string(game.Port)
                + "; joining it");
            Network::NetConnectCommand::Run(game.Host, game.Port, hostName,
                ParseHunter(args), ParseRecolor(args));
            return true;
        }

        const std::optional<std::string> connect = ValueAfter(args, "connect");
        if (connect.has_value())
        {
            Network::NetConnectCommand::Run(*connect, ParsePort(args), ParseName(args),
                ParseHunter(args), ParseRecolor(args));
            return true;
        }

        const std::optional<std::string> check = ValueAfter(args, "netcheck");
        if (check.has_value())
        {
            const std::optional<std::string> shots = ValueAfter(args, "shots");
            double seconds = 30;
            double parsedSeconds = 0;
            if (TryParseDoubleInvariant(ValueAfter(args, "seconds"), parsedSeconds))
            {
                seconds = parsedSeconds;
            }
            double spectateAt = -1;
            double rejoinAt = -1;
            if (HasFlag(args, "spectate"))
            {
                spectateAt = 0;
                double parsedSpectate = 0;
                if (TryParseDoubleInvariant(ValueAfter(args, "spectate"), parsedSpectate))
                {
                    spectateAt = parsedSpectate;
                }
            }
            double parsedRejoin = 0;
            if (TryParseDoubleInvariant(ValueAfter(args, "rejoin"), parsedRejoin))
            {
                rejoinAt = parsedRejoin;
            }
            const int color = ValueAfter(args, "recolor").has_value()
                ? ParseRecolor(args)
                : -1;
            SetExitCode(Network::NetCheckClient::Run(*check, ParsePort(args),
                ParseName(args), ParseHunter(args), seconds, shots, width, height,
                HasFlag(args, "recorddemo"), spectateAt, rejoinAt, color));
            return true;
        }

        const std::optional<std::string> demoInfo = ValueAfter(args, "demoinfo");
        if (demoInfo.has_value())
        {
            SetExitCode(Network::DemoInfo::Print(*demoInfo, HasFlag(args, "replay")));
            return true;
        }

        const std::vector<std::string> share = ValuesAfter(args, "thumbnail");
        if (!share.empty())
        {
            const int captured = ThumbnailCapture::CaptureRooms(share, width, height);
            WriteLine("[thumbnails] captured " + std::to_string(captured) + "/"
                + std::to_string(share.size()));
            return true;
        }

        if (HasFlag(args, "thumbnails"))
        {
            GenerateThumbnails(args, width, height);
            return true;
        }

        return false;
    }
}
