#include "Program.hpp"

// Future one-file migration dependencies.
//
// These are deliberately the counterparts of the C# files Program.cs actually
// calls. They are dependencies, not substitute implementations and must not be
// filled with stubs merely to make Program.cpp link.
//
// src/MphRead/Export/Images.cs
#include "Export/Images.hpp"

// src/MphRead/Formats/Formats.cs
// Contains Paths.
#include "Formats/Formats.hpp"

// src/MphRead/Formats/Movie.cs
#include "Formats/Movie.hpp"

// src/MphRead/Formats/Sound.cs
#include "Formats/Sound.hpp"

// src/MphRead/Menu.cs
#include "Menu.hpp"

// src/MphRead/Metadata/Metadata.cs
#include "Metadata/Metadata.hpp"

// src/MphRead/Metadata/Rooms.cs
// Contains RoomMetadata.
#include "Metadata/Rooms.hpp"

// src/MphRead/Mods/Branding.cs
#include "Mods/Branding.hpp"

// src/MphRead/Mods/ConsoleWindow.cs
#include "Mods/ConsoleWindow.hpp"

// src/MphRead/Mods/ModEntry.cs
#include "Mods/ModEntry.hpp"

// src/MphRead/Read.cs
#include "Read.hpp"

// src/MphRead/Renderer.cs
#include "Renderer.hpp"

// src/MphRead/Utility/Console.cs
// Contains ConsoleSetup.
#include "Utility/Console.hpp"

// src/MphRead/Utility/Extract.cs
#include "Utility/Extract.hpp"

#include <array>
#include <cerrno>
#include <climits>
#include <cstdint>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <iterator>
#include <limits>
#include <optional>
#include <stdexcept>
#include <string>
#include <string_view>
#include <tuple>
#include <utility>
#include <vector>

#if defined(_WIN32)
#include <conio.h>
#else
#include <termios.h>
#include <unistd.h>
#endif

namespace
{
    using MphRead::ProgramException;

    // C#:
    // private static readonly Version _minExtractVersion
    //     = new Version(0, 19, 0, 0);
    const System::Version MinExtractVersion(0, 19, 0, 0);

    struct Argument final
    {
        std::string Name;
        std::optional<std::string> ValueOne;
        std::optional<std::string> ValueTwo;

        Argument(
            std::string name,
            std::optional<std::string> valueOne,
            std::optional<std::string> valueTwo = std::nullopt)
            : Name(std::move(name)),
              ValueOne(std::move(valueOne)),
              ValueTwo(std::move(valueTwo))
        {
        }
    };

    [[nodiscard]] bool IsDotNetWhitespace(std::uint32_t codePoint) noexcept
    {
        // Char.IsWhiteSpace set relevant to string.Trim() on current .NET.
        if (codePoint >= 0x0009 && codePoint <= 0x000D)
        {
            return true;
        }

        switch (codePoint)
        {
        case 0x0020:
        case 0x0085:
        case 0x00A0:
        case 0x1680:
        case 0x2000:
        case 0x2001:
        case 0x2002:
        case 0x2003:
        case 0x2004:
        case 0x2005:
        case 0x2006:
        case 0x2007:
        case 0x2008:
        case 0x2009:
        case 0x200A:
        case 0x2028:
        case 0x2029:
        case 0x202F:
        case 0x205F:
        case 0x3000:
            return true;

        default:
            return false;
        }
    }

    struct Utf8CodePoint final
    {
        std::uint32_t Value;
        std::size_t Length;
    };

    [[nodiscard]] std::optional<Utf8CodePoint> DecodeUtf8Forward(
        std::string_view text,
        std::size_t position) noexcept
    {
        if (position >= text.size())
        {
            return std::nullopt;
        }

        const auto first = static_cast<unsigned char>(text[position]);

        if (first <= 0x7F)
        {
            return Utf8CodePoint{first, 1};
        }

        if ((first & 0xE0U) == 0xC0U)
        {
            if (position + 1 >= text.size())
            {
                return std::nullopt;
            }

            const auto second = static_cast<unsigned char>(text[position + 1]);
            if ((second & 0xC0U) != 0x80U)
            {
                return std::nullopt;
            }

            const std::uint32_t value
                = (static_cast<std::uint32_t>(first & 0x1FU) << 6)
                | static_cast<std::uint32_t>(second & 0x3FU);

            if (value < 0x80U)
            {
                return std::nullopt;
            }

            return Utf8CodePoint{value, 2};
        }

        if ((first & 0xF0U) == 0xE0U)
        {
            if (position + 2 >= text.size())
            {
                return std::nullopt;
            }

            const auto second = static_cast<unsigned char>(text[position + 1]);
            const auto third = static_cast<unsigned char>(text[position + 2]);

            if ((second & 0xC0U) != 0x80U || (third & 0xC0U) != 0x80U)
            {
                return std::nullopt;
            }

            const std::uint32_t value
                = (static_cast<std::uint32_t>(first & 0x0FU) << 12)
                | (static_cast<std::uint32_t>(second & 0x3FU) << 6)
                | static_cast<std::uint32_t>(third & 0x3FU);

            if (value < 0x800U || (value >= 0xD800U && value <= 0xDFFFU))
            {
                return std::nullopt;
            }

            return Utf8CodePoint{value, 3};
        }

        if ((first & 0xF8U) == 0xF0U)
        {
            if (position + 3 >= text.size())
            {
                return std::nullopt;
            }

            const auto second = static_cast<unsigned char>(text[position + 1]);
            const auto third = static_cast<unsigned char>(text[position + 2]);
            const auto fourth = static_cast<unsigned char>(text[position + 3]);

            if ((second & 0xC0U) != 0x80U
                || (third & 0xC0U) != 0x80U
                || (fourth & 0xC0U) != 0x80U)
            {
                return std::nullopt;
            }

            const std::uint32_t value
                = (static_cast<std::uint32_t>(first & 0x07U) << 18)
                | (static_cast<std::uint32_t>(second & 0x3FU) << 12)
                | (static_cast<std::uint32_t>(third & 0x3FU) << 6)
                | static_cast<std::uint32_t>(fourth & 0x3FU);

            if (value < 0x10000U || value > 0x10FFFFU)
            {
                return std::nullopt;
            }

            return Utf8CodePoint{value, 4};
        }

        return std::nullopt;
    }

    [[nodiscard]] std::optional<std::pair<Utf8CodePoint, std::size_t>>
    DecodeUtf8Backward(
        std::string_view text,
        std::size_t end) noexcept
    {
        if (end == 0 || end > text.size())
        {
            return std::nullopt;
        }

        std::size_t start = end - 1;

        while (start > 0)
        {
            const auto byte = static_cast<unsigned char>(text[start]);
            if ((byte & 0xC0U) != 0x80U)
            {
                break;
            }
            --start;
        }

        std::optional<Utf8CodePoint> decoded = DecodeUtf8Forward(text, start);
        if (!decoded.has_value() || start + decoded->Length != end)
        {
            return std::nullopt;
        }

        return std::make_pair(*decoded, start);
    }

    [[nodiscard]] std::string_view TrimDotNetWhitespace(
        std::string_view text) noexcept
    {
        std::size_t first = 0;
        std::size_t last = text.size();

        while (first < last)
        {
            std::optional<Utf8CodePoint> decoded
                = DecodeUtf8Forward(text.substr(0, last), first);

            if (!decoded.has_value() || !IsDotNetWhitespace(decoded->Value))
            {
                break;
            }

            first += decoded->Length;
        }

        while (last > first)
        {
            std::optional<std::pair<Utf8CodePoint, std::size_t>> decoded
                = DecodeUtf8Backward(text, last);

            if (!decoded.has_value()
                || !IsDotNetWhitespace(decoded->first.Value))
            {
                break;
            }

            last = decoded->second;
        }

        return text.substr(first, last - first);
    }

    [[nodiscard]] bool TryParseInt32(
        std::string_view input,
        std::int32_t& result) noexcept
    {
        result = 0;

        input = TrimDotNetWhitespace(input);
        if (input.empty())
        {
            return false;
        }

        bool negative = false;
        std::size_t index = 0;

        if (input[index] == '+' || input[index] == '-')
        {
            negative = input[index] == '-';
            ++index;

            if (index == input.size())
            {
                return false;
            }
        }

        constexpr std::uint64_t PositiveLimit
            = static_cast<std::uint64_t>(
                std::numeric_limits<std::int32_t>::max());

        constexpr std::uint64_t NegativeLimit
            = PositiveLimit + 1ULL;

        const std::uint64_t limit = negative
            ? NegativeLimit
            : PositiveLimit;

        std::uint64_t value = 0;

        for (; index < input.size(); ++index)
        {
            const unsigned char ch
                = static_cast<unsigned char>(input[index]);

            if (ch < static_cast<unsigned char>('0')
                || ch > static_cast<unsigned char>('9'))
            {
                result = 0;
                return false;
            }

            const std::uint64_t digit
                = static_cast<std::uint64_t>(ch - '0');

            if (value > (limit - digit) / 10ULL)
            {
                result = 0;
                return false;
            }

            value = value * 10ULL + digit;
        }

        if (!negative)
        {
            result = static_cast<std::int32_t>(value);
            return true;
        }

        if (value == NegativeLimit)
        {
            result = std::numeric_limits<std::int32_t>::min();
        }
        else
        {
            result = -static_cast<std::int32_t>(value);
        }

        return true;
    }

    [[nodiscard]] bool StartsWithDash(std::string_view text) noexcept
    {
        return !text.empty() && text.front() == '-';
    }

    [[nodiscard]] std::string ToLowerInvariantForProgram(
        std::string_view text)
    {
        // Program.cs calls ToLower() only before comparing against ASCII
        // literals. ConsoleSetup.Run() has already installed InvariantCulture.
        // ASCII invariant casing therefore produces exactly the branch
        // decisions required here without locale-dependent std::tolower().
        std::string result(text);

        for (char& ch : result)
        {
            if (ch >= 'A' && ch <= 'Z')
            {
                ch = static_cast<char>(ch - 'A' + 'a');
            }
        }

        return result;
    }

    [[nodiscard]] std::filesystem::path PathFromUtf8(
        std::string_view value)
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

    [[nodiscard]] std::string PathToUtf8(
        const std::filesystem::path& path)
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

    [[nodiscard]] bool FileExists(std::string_view path) noexcept
    {
        // System.IO.File.Exists:
        // * false for missing paths,
        // * false for directories,
        // * false rather than propagation for ordinary status errors.
        std::error_code error;

        const std::filesystem::file_status status
            = std::filesystem::status(PathFromUtf8(path), error);

        if (error)
        {
            return false;
        }

        return std::filesystem::is_regular_file(status);
    }

    [[nodiscard]] std::string ReadAllText(std::string_view path)
    {
        std::ifstream stream(
            PathFromUtf8(path),
            std::ios::in | std::ios::binary);

        if (!stream.is_open())
        {
            throw std::ios_base::failure(
                "Could not open file for reading: "
                + std::string(path));
        }

        stream.exceptions(std::ios::badbit);

        std::string text(
            std::istreambuf_iterator<char>(stream),
            std::istreambuf_iterator<char>());

        // File.ReadAllText uses BOM-aware UTF-8 reading. Remove a UTF-8 BOM
        // so paths.txt's first logical character matches the managed string.
        if (text.size() >= 3
            && static_cast<unsigned char>(text[0]) == 0xEF
            && static_cast<unsigned char>(text[1]) == 0xBB
            && static_cast<unsigned char>(text[2]) == 0xBF)
        {
            text.erase(0, 3);
        }

        return text;
    }

    [[nodiscard]] std::string GetFileNameWithoutExtension(
        std::string_view path)
    {
#if defined(_WIN32)
        const std::size_t separator = path.find_last_of("/\\");
#else
        const std::size_t separator = path.find_last_of('/');
#endif

        std::string_view fileName;

        if (separator == std::string_view::npos)
        {
            fileName = path;
        }
        else
        {
            fileName = path.substr(separator + 1);
        }

        const std::size_t period = fileName.find_last_of('.');

        if (period == std::string_view::npos)
        {
            return std::string(fileName);
        }

        return std::string(fileName.substr(0, period));
    }

    void WriteLine()
    {
        std::cout << '\n';
    }

    void WriteLine(std::string_view value)
    {
        std::cout << value << '\n';
    }

    void ReadKey()
    {
        std::cout.flush();

#if defined(_WIN32)
        const wint_t first = _getwche();

        // Extended Windows console keys are represented as a prefix plus a
        // scan code. Console.ReadKey() consumes the complete key event before
        // returning, so consume the second code as well.
        if (first == 0 || first == 0xE0)
        {
            (void)_getwch();
        }
#else
        termios original{};

        // Console.ReadKey() requires an actual console. Failing rather than
        // silently falling back to line-oriented stdin preserves that
        // distinction for redirected/headless input.
        if (tcgetattr(STDIN_FILENO, &original) != 0)
        {
            throw std::runtime_error(
                "Console.ReadKey requires an interactive console.");
        }

        termios current = original;
        current.c_lflag &= static_cast<tcflag_t>(~ICANON);
        current.c_cc[VMIN] = 1;
        current.c_cc[VTIME] = 0;

        if (tcsetattr(STDIN_FILENO, TCSANOW, &current) != 0)
        {
            throw std::runtime_error(
                "Could not configure the console for Console.ReadKey.");
        }

        struct TerminalRestore final
        {
            termios Value;

            ~TerminalRestore()
            {
                (void)tcsetattr(STDIN_FILENO, TCSANOW, &Value);
            }
        } restore{original};

        char ch = 0;

        while (true)
        {
            const ssize_t count = ::read(STDIN_FILENO, &ch, 1);

            if (count == 1)
            {
                break;
            }

            if (count < 0 && errno == EINTR)
            {
                continue;
            }

            throw std::runtime_error(
                "Could not read a key from the console.");
        }
#endif
    }

    [[nodiscard]] bool AnyName(
        const std::vector<Argument>& arguments,
        std::string_view name) noexcept
    {
        for (const Argument& argument : arguments)
        {
            if (argument.Name == name)
            {
                return true;
            }
        }

        return false;
    }

    [[nodiscard]] bool TryGetArgument(
        const std::vector<Argument>& arguments,
        std::string_view fullName,
        std::string_view shortName,
        const Argument*& argument) noexcept
    {
        // C# ultimately returns the first result from:
        // arguments.Where(...).First()
        //
        // The source enumerates twice via Any() and First(), but arguments is
        // a List-backed sequence with no mutation or enumeration side effects.
        for (const Argument& item : arguments)
        {
            if (item.Name == fullName || item.Name == shortName)
            {
                argument = &item;
                return true;
            }
        }

        argument = nullptr;
        return false;
    }

    [[nodiscard]] bool TryGetString(
        const std::vector<Argument>& arguments,
        std::string_view fullName,
        std::string_view shortName,
        std::optional<std::string>& value)
    {
        const Argument* argument = nullptr;

        if (TryGetArgument(
                arguments,
                fullName,
                shortName,
                argument)
            && argument->ValueOne.has_value())
        {
            value = argument->ValueOne;
            return true;
        }

        value = std::nullopt;
        return false;
    }

    [[nodiscard]] bool TryGetInt(
        const std::vector<Argument>& arguments,
        std::string_view fullName,
        std::string_view shortName,
        std::int32_t& value)
    {
        std::optional<std::string> stringValue;

        if (TryGetString(
                arguments,
                fullName,
                shortName,
                stringValue))
        {
            std::int32_t intValue = 0;

            if (TryParseInt32(*stringValue, intValue))
            {
                value = intValue;
                return true;
            }
        }

        value = 0;
        return false;
    }

    class PairRange final
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
                const Argument& argument = (*_arguments)[_index];

                std::int32_t valueTwo = 0;

                if (argument.ValueTwo.has_value())
                {
                    (void)TryParseInt32(
                        *argument.ValueTwo,
                        valueTwo);
                }

                return std::make_pair(
                    *argument.ValueOne,
                    valueTwo);
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

            friend bool operator==(
                const Iterator& left,
                const Iterator& right) noexcept
            {
                return left._arguments == right._arguments
                    && left._index == right._index;
            }

            friend bool operator!=(
                const Iterator& left,
                const Iterator& right) noexcept
            {
                return !(left == right);
            }

        private:
            void AdvanceToMatch()
            {
                if (_arguments == nullptr)
                {
                    return;
                }

                while (_index < _arguments->size())
                {
                    const Argument& argument
                        = (*_arguments)[_index];

                    if ((argument.Name == _fullName
                            || argument.Name == _shortName)
                        && argument.ValueOne.has_value())
                    {
                        return;
                    }

                    ++_index;
                }
            }

            const std::vector<Argument>* _arguments;
            std::string_view _fullName;
            std::string_view _shortName;
            std::size_t _index;
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
            return Iterator(
                &_arguments,
                _fullName,
                _shortName,
                0);
        }

        [[nodiscard]] Iterator end() const
        {
            return Iterator(
                &_arguments,
                _fullName,
                _shortName,
                _arguments.size());
        }

    private:
        const std::vector<Argument>& _arguments;
        std::string_view _fullName;
        std::string_view _shortName;
    };

    [[nodiscard]] PairRange GetPairs(
        const std::vector<Argument>& arguments,
        std::string_view fullName,
        std::string_view shortName)
    {
        return PairRange(arguments, fullName, shortName);
    }

    [[nodiscard]] std::vector<Argument> ParseArguments(
        const std::vector<std::string>& args)
    {
        std::vector<Argument> arguments;

        for (std::size_t i = 0; i < args.size(); ++i)
        {
            std::string arg = args[i];

            if (StartsWithDash(arg) && arg.size() > 1)
            {
                // C# arg[1..] removes exactly one leading '-'.
                arg.erase(0, 1);

                if (i == args.size() - 1)
                {
                    arguments.emplace_back(
                        std::move(arg),
                        std::nullopt);
                }
                else
                {
                    const std::string& valueOne
                        = args[i + 1];

                    if (StartsWithDash(valueOne))
                    {
                        arguments.emplace_back(
                            std::move(arg),
                            std::nullopt);
                    }
                    else
                    {
                        std::optional<std::string> valueTwo;

                        if (i < args.size() - 2
                            && !StartsWithDash(args[i + 2]))
                        {
                            valueTwo = args[i + 2];
                            ++i;
                        }

                        arguments.emplace_back(
                            std::move(arg),
                            valueOne,
                            std::move(valueTwo));

                        ++i;
                    }
                }
            }
        }

        return arguments;
    }

    [[nodiscard]] bool CheckVersion()
    {
        std::string text = ReadAllText("paths.txt");

        const std::size_t newline = text.find('\n');
        if (newline != std::string::npos)
        {
            text.resize(newline);
        }

        const std::string_view trimmed
            = TrimDotNetWhitespace(text);

        const std::optional<System::Version> extractVersion
            = System::Version::TryParse(trimmed);

        if (extractVersion.has_value())
        {
            return *extractVersion >= MinExtractVersion;
        }

        return false;
    }

    [[nodiscard]] bool CheckSetup(
        const std::vector<std::string>& args)
    {
        if (FileExists("paths.txt") && !CheckVersion())
        {
            WriteLine(
                "Your paths.txt file is not compatible with this version of "
                + std::string(MphRead::Mods::Branding::Name)
                + " and needs to be recreated.");

            WriteLine(
                "It is recommended that you delete the file as well as any "
                "extracted game files, then perform setup again.");

            WriteLine();
            WriteLine("Press any key to exit...");
            ReadKey();

            return true;
        }

        if (args.size() == 1
            && !StartsWithDash(args[0])
            && FileExists(args[0]))
        {
            MphRead::Extract::Setup(args[0]);
            return true;
        }

        if (!FileExists("paths.txt"))
        {
            WriteLine("Could not find the paths.txt file.");

            WriteLine(
                "You may need to perform first-time setup by dragging a ROM "
                "onto the "
                + MphRead::Mods::Branding::Executable()
                + " executable.");

            WriteLine();
            WriteLine("Press any key to exit...");
            ReadKey();

            return true;
        }

        MphRead::Paths::UpdatePaths();
        MphRead::Paths::ChooseMphPath();
        MphRead::Paths::ChooseFhPath();

        return false;
    }

    void Nop()
    {
    }

    [[noreturn]] void Exit()
    {
        Nop();

        WriteLine(
            MphRead::Mods::Branding::Executable()
            + " usage:");

        WriteLine("    -room <room_name -or- room_id>");
        WriteLine("    -model <model_name> [recolor_index]");
        WriteLine(
            "At most one room may be specified. Any number of models may be "
            "specified.");
        WriteLine(
            "To load First Hunt models, include -fh in the argument list.");
        WriteLine(
            "Available room options: -mode, -players, -boss, -node, -entity");
        WriteLine("- or -");
        WriteLine("    -extract <archive_path>");
        WriteLine(
            "If the target archive is LZ10-compressed, it will be "
            "decompressed.");
        WriteLine("- or -");
        WriteLine("    -export <target_name>");
        WriteLine("The export target may be a model or room name.");

        std::cout.flush();

        // C# Environment.Exit(1):
        // immediate process termination; normal automatic-scope unwinding is
        // intentionally not performed.
        std::exit(1);
    }
}

namespace System
{
    Version::Version(int major, int minor)
        : _major(major),
          _minor(minor),
          _build(-1),
          _revision(-1)
    {
        if (major < 0)
        {
            throw std::out_of_range("major");
        }

        if (minor < 0)
        {
            throw std::out_of_range("minor");
        }
    }

    Version::Version(int major, int minor, int build)
        : _major(major),
          _minor(minor),
          _build(build),
          _revision(-1)
    {
        if (major < 0)
        {
            throw std::out_of_range("major");
        }

        if (minor < 0)
        {
            throw std::out_of_range("minor");
        }

        if (build < 0)
        {
            throw std::out_of_range("build");
        }
    }

    Version::Version(
        int major,
        int minor,
        int build,
        int revision)
        : _major(major),
          _minor(minor),
          _build(build),
          _revision(revision)
    {
        if (major < 0)
        {
            throw std::out_of_range("major");
        }

        if (minor < 0)
        {
            throw std::out_of_range("minor");
        }

        if (build < 0)
        {
            throw std::out_of_range("build");
        }

        if (revision < 0)
        {
            throw std::out_of_range("revision");
        }
    }

    int Version::Major() const noexcept
    {
        return _major;
    }

    int Version::Minor() const noexcept
    {
        return _minor;
    }

    int Version::Build() const noexcept
    {
        return _build;
    }

    int Version::Revision() const noexcept
    {
        return _revision;
    }

    std::string Version::ToString() const
    {
        std::string value
            = std::to_string(_major)
            + "."
            + std::to_string(_minor);

        if (_build >= 0)
        {
            value += ".";
            value += std::to_string(_build);
        }

        if (_revision >= 0)
        {
            value += ".";
            value += std::to_string(_revision);
        }

        return value;
    }

    std::optional<Version> Version::TryParse(
        std::string_view text) noexcept
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

            const std::size_t separator
                = text.find('.', start);

            const std::string_view component
                = separator == std::string_view::npos
                ? text.substr(start)
                : text.substr(start, separator - start);

            std::int32_t value = 0;

            if (!TryParseInt32(component, value) || value < 0)
            {
                return std::nullopt;
            }

            components[componentCount++] = value;

            if (separator == std::string_view::npos)
            {
                break;
            }

            start = separator + 1;

            if (start > text.size())
            {
                return std::nullopt;
            }
        }

        if (componentCount < 2 || componentCount > 4)
        {
            return std::nullopt;
        }

        try
        {
            if (componentCount == 2)
            {
                return Version(
                    components[0],
                    components[1]);
            }

            if (componentCount == 3)
            {
                return Version(
                    components[0],
                    components[1],
                    components[2]);
            }

            return Version(
                components[0],
                components[1],
                components[2],
                components[3]);
        }
        catch (...)
        {
            return std::nullopt;
        }
    }

    bool operator==(
        const Version& left,
        const Version& right) noexcept
    {
        return left._major == right._major
            && left._minor == right._minor
            && left._build == right._build
            && left._revision == right._revision;
    }

    bool operator!=(
        const Version& left,
        const Version& right) noexcept
    {
        return !(left == right);
    }

    bool operator<(
        const Version& left,
        const Version& right) noexcept
    {
        if (left._major != right._major)
        {
            return left._major < right._major;
        }

        if (left._minor != right._minor)
        {
            return left._minor < right._minor;
        }

        if (left._build != right._build)
        {
            return left._build < right._build;
        }

        return left._revision < right._revision;
    }

    bool operator<=(
        const Version& left,
        const Version& right) noexcept
    {
        return left < right || left == right;
    }

    bool operator>(
        const Version& left,
        const Version& right) noexcept
    {
        return right < left;
    }

    bool operator>=(
        const Version& left,
        const Version& right) noexcept
    {
        return right <= left;
    }
}

namespace MphRead
{
    const System::Version Program::Version(
        0,
        35,
        1,
        0);

    ProgramException::ProgramException(
        const std::string& message)
        : std::runtime_error(message)
    {
    }

    void Program::Main(
        const std::vector<std::string>& args)
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

        const std::vector<Argument> arguments
            = ParseArguments(args);

        if (Mods::ModEntry::TryHandle(args))
        {
            return;
        }

        if (arguments.empty())
        {
            Menu::ShowMenuPrompts();
        }
        else if (AnyName(arguments, "setup"))
        {
            const std::string archivesPath
                = Paths::Combine(
                    Paths::FileSystem(),
                    "archives");

            // Keep enumeration lazy and unsorted. Directory.EnumerateFiles()
            // specifies neither a sort nor pre-materialization.
            for (const std::filesystem::directory_entry& entry
                : std::filesystem::directory_iterator(
                    PathFromUtf8(archivesPath)))
            {
                if (!entry.is_directory())
                {
                    const std::string path
                        = PathToUtf8(entry.path());

                    Read::ExtractArchive(
                        GetFileNameWithoutExtension(path));
                }
            }
        }
        else
        {
            std::optional<std::string> exportValue;

            if (TryGetString(
                    arguments,
                    "export",
                    "e",
                    exportValue))
            {
                if (ToLowerInvariantForProgram(*exportValue)
                    == "layer2d")
                {
                    Export::Images::ExportHudLayers();
                }
                else if (ToLowerInvariantForProgram(*exportValue)
                    == "object2d")
                {
                    Export::Images::ExportHudObjects();
                }
                else if (ToLowerInvariantForProgram(*exportValue)
                    == "sfx")
                {
                    Formats::Sound::SoundRead::ExportSamples();
                }
                else if (ToLowerInvariantForProgram(*exportValue)
                    == "wfs")
                {
                    Formats::Sound::SoundRead::ExportWfsSamples();
                }
                else if (ToLowerInvariantForProgram(*exportValue)
                    == "strm")
                {
                    Formats::Sound::SoundRead::ExportStreams();
                }
                else if (ToLowerInvariantForProgram(*exportValue)
                    == "fhsfx")
                {
                    Formats::Sound::SoundRead::ExportAllFh();
                }
                else if (ToLowerInvariantForProgram(*exportValue)
                    == "movie")
                {
                    const Argument* exportArgument = nullptr;

                    (void)TryGetArgument(
                        arguments,
                        "export",
                        "e",
                        exportArgument);

                    // TryGetString above can succeed only when the same first
                    // matching Argument exists and has ValueOne, matching the
                    // C# null-forgiving access here.
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
                    const bool firstHunt
                        = AnyName(arguments, "fh");

                    Read::ReadAndExport(
                        *exportValue,
                        firstHunt);
                }

                return;
            }

            std::optional<std::string> extractValue;

            if (TryGetString(
                    arguments,
                    "extract",
                    "x",
                    extractValue))
            {
                Read::ExtractArchive(*extractValue);
                return;
            }

            std::vector<std::string> rooms;
            std::vector<std::pair<std::string, std::int32_t>> models;

            GameMode mode = GameMode::None;
            std::int32_t playerCount = 0;
            BossFlags bossFlags = BossFlags::None;
            std::int32_t nodeLayerMask = 0;
            std::int32_t entityLayerId = -1;

            std::int32_t roomId = 0;

            if (TryGetInt(
                    arguments,
                    "room",
                    "r",
                    roomId))
            {
                const RoomMetadata* meta
                    = Metadata::GetRoomById(roomId);

                if (meta == nullptr)
                {
                    Exit();
                }

                rooms.push_back(meta->Name());
            }
            else
            {
                std::optional<std::string> roomName;

                if (TryGetString(
                        arguments,
                        "room",
                        "r",
                        roomName))
                {
                    rooms.push_back(*roomName);
                }
            }

            std::int32_t modeValue = 0;

            if (TryGetInt(
                    arguments,
                    "mode",
                    "g",
                    modeValue))
            {
                mode = static_cast<GameMode>(modeValue);
            }

            std::int32_t playerValue = 0;

            if (TryGetInt(
                    arguments,
                    "players",
                    "p",
                    playerValue))
            {
                playerCount = playerValue;
            }

            std::int32_t bossValue = 0;

            if (TryGetInt(
                    arguments,
                    "boss",
                    "b",
                    bossValue))
            {
                bossFlags = static_cast<BossFlags>(bossValue);
            }

            std::int32_t nodeValue = 0;

            if (TryGetInt(
                    arguments,
                    "node",
                    "n",
                    nodeValue))
            {
                nodeLayerMask = nodeValue;
            }

            std::int32_t entityValue = 0;

            if (TryGetInt(
                    arguments,
                    "entity",
                    "l",
                    entityValue))
            {
                entityLayerId = entityValue;
            }

            for (const auto& pair
                : GetPairs(
                    arguments,
                    "model",
                    "m"))
            {
                models.push_back(pair);
            }

            if (rooms.size() > 1
                || (rooms.empty() && models.empty()))
            {
                Exit();
            }

            // C#:
            // using var renderer = new RenderWindow();
            //
            // Automatic storage gives the same lifetime boundary: destruction
            // occurs after Run returns and during exception unwinding, but not
            // when Exit() terminates the process directly.
            RenderWindow renderer;

            for (const std::string& room : rooms)
            {
                renderer.AddRoom(
                    room,
                    mode,
                    playerCount,
                    bossFlags,
                    nodeLayerMask,
                    entityLayerId);
            }

            const bool firstHunt
                = AnyName(arguments, "fh");

            for (const auto& [model, recolor] : models)
            {
                renderer.AddModel(
                    model,
                    recolor,
                    firstHunt);
            }

            renderer.Run();
        }
    }
}
