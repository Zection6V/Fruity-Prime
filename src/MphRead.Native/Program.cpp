#include "Program.hpp"
#include "Export/Images.hpp"
#include "Formats/Formats.hpp"
#include "Formats/Movie.hpp"
#include "Formats/Sound.hpp"
#include "Menu.hpp"
#include "Metadata/Metadata.hpp"
#include "Metadata/Rooms.hpp"
#include "Mods/Branding.hpp"
#include "Mods/ConsoleWindow.hpp"
#include "Mods/CrashReport.hpp"
#include "Mods/Launcher/Portable/RomWhitelist.hpp"
#include "Mods/ModEntry.hpp"
#include "Read.hpp"
#include "Renderer.hpp"
#include "NativeRuntime/System/Runtime.hpp"
#include "Utility/Console.hpp"
#include "Utility/Extract.hpp"
#include "NativeRuntime/System/Globalization.hpp"
#include "NativeRuntime/System/IO.hpp"
#include <array>
#include <cerrno>
#include <cstdint>
#include <cstdlib>
#include <deque>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <iterator>
#include <limits>
#include <memory>
#include <optional>
#include <stdexcept>
#include <string>
#include <string_view>
#include <utility>
#include <vector>
#if defined(_WIN32)
#include <conio.h>
#include <cstdio>
#include <io.h>
#include <windows.h>
#else
#include <poll.h>
#include <sys/stat.h>
#include <termios.h>
#include <unistd.h>
#endif

using ::MphRead::NativeRuntime::FileExists;
using ::MphRead::NativeRuntime::FileReadAllText;
using ::MphRead::NativeRuntime::Int32TryParseCurrentCulture;
using ::MphRead::NativeRuntime::PathFromUtf8;
using ::MphRead::NativeRuntime::PathToUtf8;
using ::MphRead::NativeRuntime::StringTrimView;

namespace
{
    [[nodiscard]] bool StartsWithDash(std::string_view text) noexcept
    {
        return !text.empty() && text.front() == '-';
    }
    [[nodiscard]] std::string ToLowerForProgramFallback(std::string_view text)
    {
        std::string result(text);
        for (char &ch : result)
            if (ch >= 'A' && ch <= 'Z')
                ch = static_cast<char>(ch - 'A' + 'a');
        return result;
    }
    [[nodiscard]] std::string GetFileNameWithoutExtension(std::string_view path)
    {
#if defined(_WIN32)
        const std::size_t separator = path.find_last_of("/\\");
#else
        const std::size_t separator = path.find_last_of('/');
#endif
        const std::string_view fileName = separator == std::string_view::npos ? path : path.substr(separator + 1);
        const std::size_t period = fileName.find_last_of('.');
        return period == std::string_view::npos ? std::string(fileName) : std::string(fileName.substr(0, period));
    }
    void WriteLine() { std::cout << '\n'; }
    void WriteLine(std::string_view value) { std::cout << value << '\n'; }
#if !defined(_WIN32)
    std::deque<unsigned char> PendingConsoleBytes;
    [[nodiscard]] std::optional<unsigned char> ReadConsoleByte(bool wait, int timeoutMilliseconds)
    {
        if (!PendingConsoleBytes.empty())
        {
            const unsigned char value = PendingConsoleBytes.front();
            PendingConsoleBytes.pop_front();
            return value;
        }
        if (!wait)
        {
            pollfd descriptor{STDIN_FILENO, POLLIN, 0};
            const int ready = ::poll(&descriptor, 1, timeoutMilliseconds);
            if (ready <= 0 || (descriptor.revents & POLLIN) == 0)
                return std::nullopt;
        }
        unsigned char value = 0;
        while (true)
        {
            const ssize_t count = ::read(STDIN_FILENO, &value, 1);
            if (count == 1)
                return value;
            if (count < 0 && errno == EINTR)
                continue;
            throw std::runtime_error("Could not read a key from the console.");
        }
    }
    void EchoConsoleBytes(const std::vector<unsigned char> &bytes)
    {
        if (bytes.empty())
            return;
        std::cout.write(reinterpret_cast<const char *>(bytes.data()), static_cast<std::streamsize>(bytes.size()));
        std::cout.flush();
    }
#endif
    void ReadKey()
    {
        std::cout.flush();
#if defined(_WIN32)
        if (_isatty(_fileno(stdin)) == 0)
            throw std::runtime_error("Console.ReadKey cannot be used when input is redirected.");
        const wint_t first = _getwche();
        if (first == 0 || first == 0xE0)
            (void)_getwch();
#else
        if (::isatty(STDIN_FILENO) == 0)
            throw std::runtime_error("Console.ReadKey cannot be used when input is redirected.");
        termios original{};
        if (::tcgetattr(STDIN_FILENO, &original) != 0)
            throw std::runtime_error("Could not read console mode for Console.ReadKey.");
        termios current = original;
        current.c_lflag &= static_cast<tcflag_t>(~(ICANON | ECHO));
        current.c_cc[VMIN] = 1;
        current.c_cc[VTIME] = 0;
        if (::tcsetattr(STDIN_FILENO, TCSANOW, &current) != 0)
            throw std::runtime_error("Could not configure the console for Console.ReadKey.");
        struct TerminalRestore final
        {
            termios Value;
            ~TerminalRestore() { (void)::tcsetattr(STDIN_FILENO, TCSANOW, &Value); }
        } restore{original};
        const unsigned char first = *ReadConsoleByte(true, 0);
        std::vector<unsigned char> keyBytes{first};
        bool echo = true;
        if (first == 0x1B)
        {
            const auto second = ReadConsoleByte(false, 30);
            if (second.has_value())
            {
                keyBytes.push_back(*second);
                if (*second == '[' || *second == 'O')
                {
                    echo = false;
                    while (true)
                    {
                        const auto next = ReadConsoleByte(false, 30);
                        if (!next.has_value())
                            break;
                        keyBytes.push_back(*next);
                        if (*next >= 0x40 && *next <= 0x7E)
                            break;
                    }
                }
                else
                    keyBytes.erase(keyBytes.begin());
            }
        }
        else if ((first & 0x80U) != 0)
        {
            std::size_t expected = 1;
            if ((first & 0xE0U) == 0xC0U)
                expected = 2;
            else if ((first & 0xF0U) == 0xE0U)
                expected = 3;
            else if ((first & 0xF8U) == 0xF0U)
                expected = 4;
            while (keyBytes.size() < expected)
            {
                const unsigned char next = *ReadConsoleByte(true, 0);
                if ((next & 0xC0U) != 0x80U)
                {
                    PendingConsoleBytes.push_front(next);
                    break;
                }
                keyBytes.push_back(next);
            }
        }
        if (echo)
            EchoConsoleBytes(keyBytes);
#endif
    }

    [[nodiscard]] std::optional<MphRead::Mods::Update::Version> TryParseVersionForProgram(
        std::string_view text)
    {
        std::array<std::int32_t, 4> components{};
        std::size_t componentCount = 0;
        std::size_t start = 0;
        while (true)
        {
            if (componentCount == components.size())
            {
                return std::nullopt;
            }
            const std::size_t separator = text.find('.', start);
            const std::string_view component = separator == std::string_view::npos
                ? text.substr(start)
                : text.substr(start, separator - start);
            std::int32_t value = 0;
            if (!Int32TryParseCurrentCulture(component, value) || value < 0)
            {
                return std::nullopt;
            }
            components[componentCount++] = value;
            if (separator == std::string_view::npos)
            {
                break;
            }
            start = separator + 1;
        }
        if (componentCount < 2 || componentCount > 4)
        {
            return std::nullopt;
        }
        try
        {
            if (componentCount == 2)
            {
                return MphRead::Mods::Update::Version(components[0], components[1]);
            }
            if (componentCount == 3)
            {
                return MphRead::Mods::Update::Version(
                    components[0], components[1], components[2]);
            }
            return MphRead::Mods::Update::Version(
                components[0], components[1], components[2], components[3]);
        }
        catch (...)
        {
            return std::nullopt;
        }
    }
}

namespace MphRead
{
    Program::Argument::Argument(
        std::string name,
        std::optional<std::string> valueOne,
        std::optional<std::string> valueTwo)
        : Name(std::move(name)),
          ValueOne(std::move(valueOne)),
          ValueTwo(std::move(valueTwo))
    {
    }

    Program::Argument& Program::Argument::operator=(const Argument& other)
    {
        if (this != std::addressof(other))
        {
            this->~Argument();
            std::construct_at(this, other);
        }
        return *this;
    }

    Program::Argument& Program::Argument::operator=(Argument&& other)
    {
        if (this != std::addressof(other))
        {
            this->~Argument();
            std::construct_at(this, std::move(other));
        }
        return *this;
    }

    class Program::PairRange final
    {
    public:
        class Iterator final
        {
        public:
            using value_type = std::pair<std::string, std::int32_t>;
            using difference_type = std::ptrdiff_t;
            using iterator_category = std::input_iterator_tag;

            Iterator(
                const std::vector<Argument>* arguments,
                std::string_view fullName,
                std::string_view shortName,
                std::size_t index)
                : _arguments(arguments),
                  _fullName(fullName),
                  _shortName(shortName),
                  _index(index)
            {
                AdvanceToMatch();
            }

            [[nodiscard]] value_type operator*() const
            {
                return *_current;
            }

            Iterator& operator++()
            {
                ++_index;
                AdvanceToMatch();
                return *this;
            }

            void operator++(int)
            {
                ++(*this);
            }

            friend bool operator==(const Iterator& left, const Iterator& right) noexcept
            {
                return left._arguments == right._arguments
                    && left._index == right._index;
            }

            friend bool operator!=(const Iterator& left, const Iterator& right) noexcept
            {
                return !(left == right);
            }

        private:
            void AdvanceToMatch()
            {
                _current.reset();
                while (_arguments != nullptr && _index < _arguments->size())
                {
                    const Argument& argument = (*_arguments)[_index];
                    const bool nameMatches = argument.Name.has_value()
                        && (*argument.Name == _fullName || *argument.Name == _shortName);
                    if (nameMatches && argument.ValueOne.has_value())
                    {
                        std::int32_t valueTwo = 0;
                        if (argument.ValueTwo.has_value())
                        {
                            (void)Int32TryParseCurrentCulture(*argument.ValueTwo, valueTwo);
                        }
                        _current = std::make_pair(*argument.ValueOne, valueTwo);
                        return;
                    }
                    ++_index;
                }
            }

            const std::vector<Argument>* _arguments;
            std::string_view _fullName;
            std::string_view _shortName;
            std::size_t _index;
            std::optional<value_type> _current;
        };

        PairRange(
            const std::vector<Argument>& arguments,
            std::string_view fullName,
            std::string_view shortName)
            : _arguments(arguments),
              _fullName(fullName),
              _shortName(shortName)
        {
        }

        [[nodiscard]] Iterator begin() const
        {
            return Iterator(&_arguments, _fullName, _shortName, 0);
        }

        [[nodiscard]] Iterator end() const
        {
            return Iterator(&_arguments, _fullName, _shortName, _arguments.size());
        }

    private:
        const std::vector<Argument>& _arguments;
        std::string_view _fullName;
        std::string_view _shortName;
    };

    const Mods::Update::Version Program::Version(0, 35, 1, 0);
    const Mods::Update::Version Program::_minExtractVersion(0, 19, 0, 0);

    ProgramException::ProgramException(const std::string& message)
        : std::runtime_error(message)
    {
    }

    Program::PairRange Program::GetPairs(
        const std::vector<Argument>& arguments,
        std::string_view fullName,
        std::string_view shortName)
    {
        return PairRange(arguments, fullName, shortName);
    }

    bool Program::TryGetArgument(
        const std::vector<Argument>& arguments,
        std::string_view fullName,
        std::string_view shortName,
        std::optional<Argument>& argument)
    {
        bool any = false;
        for (const Argument& item : arguments)
        {
            if (item.Name.has_value()
                && (*item.Name == fullName || *item.Name == shortName))
            {
                any = true;
                break;
            }
        }

        if (any)
        {
            for (const Argument& item : arguments)
            {
                if (item.Name.has_value()
                    && (*item.Name == fullName || *item.Name == shortName))
                {
                    argument = item;
                    return true;
                }
            }
        }

        argument.reset();
        return false;
    }

    bool Program::TryGetString(
        const std::vector<Argument>& arguments,
        std::string_view fullName,
        std::string_view shortName,
        std::optional<std::string>& value)
    {
        std::optional<Argument> argument;
        if (TryGetArgument(arguments, fullName, shortName, argument)
            && argument->ValueOne.has_value())
        {
            value = argument->ValueOne;
            return true;
        }
        value.reset();
        return false;
    }

    bool Program::TryGetInt(
        const std::vector<Argument>& arguments,
        std::string_view fullName,
        std::string_view shortName,
        std::int32_t& value)
    {
        std::optional<std::string> stringValue;
        if (TryGetString(arguments, fullName, shortName, stringValue))
        {
            std::int32_t intValue = 0;
            if (Int32TryParseCurrentCulture(*stringValue, intValue))
            {
                value = intValue;
                return true;
            }
        }
        value = 0;
        return false;
    }

    std::vector<Program::Argument> Program::ParseArguments(
        const std::vector<std::string>& args)
    {
        std::vector<Argument> arguments;
        for (std::size_t i = 0; i < args.size(); ++i)
        {
            std::string arg = args[i];
            if (StartsWithDash(arg) && arg.size() > 1)
            {
                arg = arg.substr(1);
                if (i == args.size() - 1)
                {
                    arguments.emplace_back(std::move(arg), std::nullopt);
                }
                else
                {
                    const std::string& valueOne = args[i + 1];
                    if (StartsWithDash(valueOne))
                    {
                        arguments.emplace_back(std::move(arg), std::nullopt);
                    }
                    else
                    {
                        std::optional<std::string> valueTwo;
                        if (i < args.size() - 2 && !StartsWithDash(args[i + 2]))
                        {
                            valueTwo = args[i + 2];
                            ++i;
                        }
                        arguments.emplace_back(
                            std::move(arg), valueOne, std::move(valueTwo));
                        ++i;
                    }
                }
            }
        }
        return arguments;
    }

    bool Program::CheckVersion()
    {
        std::string text = FileReadAllText("paths.txt");
        const std::size_t newline = text.find('\n');
        if (newline != std::string::npos)
        {
            text.resize(newline);
        }
        const std::string_view trimmed = StringTrimView(text);
        const std::optional<Mods::Update::Version> extractVersion
            = TryParseVersionForProgram(trimmed);
        return extractVersion.has_value()
            && *extractVersion >= _minExtractVersion;
    }

    bool Program::CheckSetup(const std::vector<std::string>& args)
    {
        if (FileExists("paths.txt") && !CheckVersion())
        {
            WriteLine(
                "Your paths.txt file is not compatible with this version of "
                + std::string(Mods::Branding::Name)
                + " and needs to be recreated.");
            WriteLine(
                "It is recommended that you delete the file as well as any extracted game files, "
                "then perform setup again.");
            WriteLine();
            WriteLine("Press any key to exit...");
            ConsoleSetup::PauseIfInteractive();
            return true;
        }
        if (args.size() == 1 && !StartsWithDash(args[0]) && FileExists(args[0]))
        {
            // Same MD5 whitelist the launcher's file picker checks, so
            // dragging a ROM onto the executable can't skip it.
            std::optional<std::string> label;
            if (!Mods::Launcher::RomWhitelist::TryIdentify(args[0], label))
            {
                WriteLine("This .nds file doesn't match a known Metroid Prime Hunters "
                    "dump (checked by MD5).");
                WriteLine("Nothing was extracted.");
                WriteLine();
                WriteLine("Press any key to exit...");
                ConsoleSetup::PauseIfInteractive();
                return true;
            }
            WriteLine("Recognised: Metroid Prime Hunters, " + label.value_or(std::string()));
            Extract::Setup(args[0]);
            return true;
        }
        if (!FileExists("paths.txt"))
        {
            WriteLine("Could not find the paths.txt file.");
            WriteLine(
                "You may need to perform first-time setup by dragging a ROM onto the "
                + Mods::Branding::Executable()
                + " executable.");
            WriteLine();
            WriteLine("Press any key to exit...");
            ConsoleSetup::PauseIfInteractive();
            return true;
        }
        Paths::UpdatePaths();
        Paths::ChooseMphPath();
        Paths::ChooseFhPath();
        return false;
    }

    void Program::Nop()
    {
    }

    [[noreturn]] void Program::Exit()
    {
        Nop();
        WriteLine(Mods::Branding::Executable() + " usage:");
        WriteLine("    -room <room_name -or- room_id>");
        WriteLine("    -model <model_name> [recolor_index]");
        WriteLine("At most one room may be specified. Any number of models may be specified.");
        WriteLine("To load First Hunt models, include -fh in the argument list.");
        WriteLine("Available room options: -mode, -players, -boss, -node, -entity");
        WriteLine("- or -");
        WriteLine("    -extract <archive_path>");
        WriteLine("If the target archive is LZ10-compressed, it will be decompressed.");
        WriteLine("- or -");
        WriteLine("    -export <target_name>");
        WriteLine("The export target may be a model or room name.");
        std::exit(1);
    }

    void Program::Main(const std::vector<std::string>& args)
    {
        // First, before anything that can throw. A Windows game build is a
        // GUI binary with no console, so without this a fault anywhere in
        // startup is a process that exits with no window, no message and
        // no file: "I double-click it and nothing happens".
        Mods::CrashReport::Install();
        try
        {
            Run(args);
        }
        catch (const std::exception&)
        {
            // The main thread's own. UnhandledException is raised for it
            // too, but only after the runtime has already printed to a
            // stderr that a GUI build does not have.
            Mods::CrashReport::Report(std::current_exception(), "startup");
            ::MphRead::NativeRuntime::SetEnvironmentExitCode(1);
        }
    }

    void Program::Run(const std::vector<std::string>& args)
    {
        ConsoleSetup::Run();
#if defined(_WIN32)
        Mods::ConsoleWindow::Prepare(args);
#endif
        if (Mods::ModEntry::TryHandleHeadless(args))
        {
            return;
        }
        if (CheckSetup(args))
        {
            return;
        }

        const std::vector<Argument> arguments = ParseArguments(args);
        if (Mods::ModEntry::TryHandle(args))
        {
            return;
        }

        const auto hasName = [](const std::vector<Argument>& values, std::string_view name)
        {
            for (const Argument& argument : values)
            {
                if (argument.Name.has_value() && *argument.Name == name)
                {
                    return true;
                }
            }
            return false;
        };

        if (arguments.empty())
        {
            Menu::ShowMenuPrompts();
        }
        else if (hasName(arguments, "setup"))
        {
            const std::string archivesPath
                = Paths::Combine(Paths::FileSystem(), "archives");
            for (const std::filesystem::directory_entry& entry
                : std::filesystem::directory_iterator(PathFromUtf8(archivesPath)))
            {
                if (entry.is_regular_file())
                {
                    const std::string path = PathToUtf8(entry.path());
                    Read::ExtractArchive(GetFileNameWithoutExtension(path));
                }
            }
        }
        else
        {
            std::optional<std::string> exportValue;
            if (TryGetString(arguments, "export", "e", exportValue))
            {
                if (ToLowerForProgramFallback(*exportValue) == "layer2d")
                {
                    Export::Images::ExportHudLayers();
                }
                else if (ToLowerForProgramFallback(*exportValue) == "object2d")
                {
                    Export::Images::ExportHudObjects();
                }
                else if (ToLowerForProgramFallback(*exportValue) == "sfx")
                {
                    Formats::Sound::SoundRead::ExportSamples();
                }
                else if (ToLowerForProgramFallback(*exportValue) == "wfs")
                {
                    Formats::Sound::SoundRead::ExportWfsSamples();
                }
                else if (ToLowerForProgramFallback(*exportValue) == "strm")
                {
                    Formats::Sound::SoundRead::ExportStreams();
                }
                else if (ToLowerForProgramFallback(*exportValue) == "fhsfx")
                {
                    Formats::Sound::SoundRead::ExportAllFh();
                }
                else if (ToLowerForProgramFallback(*exportValue) == "movie")
                {
                    std::optional<Argument> exportArgument;
                    (void)TryGetArgument(arguments, "export", "e", exportArgument);
                    if (exportArgument->ValueTwo.has_value())
                    {
                        Formats::VxDecoder::Instance1()
                            .Export(*exportArgument->ValueTwo)
                            .GetAwaiter()
                            .GetResult();
                    }
                    else
                    {
                        Formats::VxDecoder::Instance1()
                            .ExportAll()
                            .GetAwaiter()
                            .GetResult();
                    }
                }
                else
                {
                    const bool firstHunt = hasName(arguments, "fh");
                    Read::ReadAndExport(*exportValue, firstHunt);
                }
            }
            else
            {
                std::optional<std::string> extractValue;
                if (TryGetString(arguments, "extract", "x", extractValue))
                {
                    Read::ExtractArchive(*extractValue);
                }
                else
                {
                    std::vector<std::string> rooms;
                    std::vector<std::pair<std::string, std::int32_t>> models;
                    GameMode mode = GameMode::None;
                    std::int32_t playerCount = 0;
                    BossFlags bossFlags = BossFlags::None;
                    std::int32_t nodeLayerMask = 0;
                    std::int32_t entityLayerId = -1;

                    std::int32_t roomId = 0;
                    if (TryGetInt(arguments, "room", "r", roomId))
                    {
                        const RoomMetadata* meta = Metadata::GetRoomById(roomId);
                        if (meta == nullptr)
                        {
                            Exit();
                        }
                        rooms.push_back(meta->Name);
                    }
                    else
                    {
                        std::optional<std::string> roomName;
                        if (TryGetString(arguments, "room", "r", roomName))
                        {
                            rooms.push_back(*roomName);
                        }
                    }

                    std::int32_t modeValue = 0;
                    if (TryGetInt(arguments, "mode", "g", modeValue))
                    {
                        mode = static_cast<GameMode>(modeValue);
                    }
                    std::int32_t playerValue = 0;
                    if (TryGetInt(arguments, "players", "p", playerValue))
                    {
                        playerCount = playerValue;
                    }
                    std::int32_t bossValue = 0;
                    if (TryGetInt(arguments, "boss", "b", bossValue))
                    {
                        bossFlags = static_cast<BossFlags>(bossValue);
                    }
                    std::int32_t nodeValue = 0;
                    if (TryGetInt(arguments, "node", "n", nodeValue))
                    {
                        nodeLayerMask = nodeValue;
                    }
                    std::int32_t entityValue = 0;
                    if (TryGetInt(arguments, "entity", "l", entityValue))
                    {
                        entityLayerId = entityValue;
                    }

                    for (const auto& pair : GetPairs(arguments, "model", "m"))
                    {
                        models.push_back(pair);
                    }

                    if (rooms.size() > 1
                        || (rooms.empty() && models.empty()))
                    {
                        Exit();
                    }

                    RenderWindow renderer;
                    for (const std::string& room : rooms)
                    {
                        renderer.AddRoom(
                            room, mode, playerCount, bossFlags,
                            nodeLayerMask, entityLayerId);
                    }

                    const bool firstHunt = hasName(arguments, "fh");
                    for (const auto& [model, recolor] : models)
                    {
                        renderer.AddModel(model, recolor, firstHunt);
                    }
                    renderer.Run();
                }
            }
        }
    }
}
