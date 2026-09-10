#include "Program.hpp"

#include "Assets/game_assets.hpp"
#include "Export/export.hpp"
#include "Formats/sound_catalog.hpp"
#include "Formats/sound_export.hpp"
#include "Menu.hpp"
#include "Read.hpp"
#include "Utility/archive.hpp"
#include "Assets/compression.hpp"
#include "Formats/paths.hpp"
#include "Metadata/Rooms.hpp"
#include "Mods/branding.hpp"
#include "Mods/console_window.hpp"
#include "Mods/mod_entry.hpp"
#include "Utility/console_setup.hpp"
#include "Utility/extract.hpp"

#include <charconv>
#include <cctype>
#include <algorithm>
#include <fstream>
#include <iostream>
#include <iterator>
#include <limits>

namespace {

[[nodiscard]] bool is_space(char value) noexcept {
    return std::isspace(static_cast<unsigned char>(value)) != 0;
}

[[nodiscard]] std::string_view trim(std::string_view value) noexcept {
    while (!value.empty() && is_space(value.front())) {
        value.remove_prefix(1);
    }
    while (!value.empty() && is_space(value.back())) {
        value.remove_suffix(1);
    }
    return value;
}

// Int32.TryParse(string, out int) uses the Integer number style: optional
// surrounding whitespace and sign, followed by decimal digits, with no
// partial success or overflow.
[[nodiscard]] bool try_parse_int32(std::string_view text, int& result) noexcept {
    text = trim(text);
    if (text.empty()) {
        return false;
    }

    bool negative = false;
    if (text.front() == '+' || text.front() == '-') {
        negative = text.front() == '-';
        text.remove_prefix(1);
    }
    if (text.empty()) {
        return false;
    }

    unsigned int magnitude = 0;
    const auto parsed = std::from_chars(
        text.data(), text.data() + text.size(), magnitude, 10);
    if (parsed.ec != std::errc{} || parsed.ptr != text.data() + text.size()) {
        return false;
    }

    constexpr unsigned int max_positive =
        static_cast<unsigned int>(std::numeric_limits<int>::max());
    constexpr unsigned int max_negative = max_positive + 1U;
    if ((!negative && magnitude > max_positive)
        || (negative && magnitude > max_negative)) {
        return false;
    }
    if (negative) {
        result = magnitude == max_negative
            ? std::numeric_limits<int>::min()
            : -static_cast<int>(magnitude);
    } else {
        result = static_cast<int>(magnitude);
    }
    return true;
}

} // namespace

namespace MphReadNative::Program {

namespace {

constexpr Version MinimumExtractVersion{{0, 19, 0, 0}};
int g_exit_code = 0;

[[nodiscard]] bool check_version_file() {
    std::ifstream input("paths.txt", std::ios::binary);
    if (!input) {
        throw std::runtime_error("could not read paths.txt");
    }
    const std::string contents{
        std::istreambuf_iterator<char>(input),
        std::istreambuf_iterator<char>()};
    if (input.bad()) {
        throw std::runtime_error("could not read paths.txt");
    }
    std::string_view first_line(contents);
    if (first_line.size() >= 3
        && static_cast<unsigned char>(first_line[0]) == 0xEFU
        && static_cast<unsigned char>(first_line[1]) == 0xBBU
        && static_cast<unsigned char>(first_line[2]) == 0xBFU) {
        first_line.remove_prefix(3);
    }
    const std::size_t newline = first_line.find('\n');
    return detail::check_version(first_line.substr(0, newline));
}

void wait_for_key() {
    static_cast<void>(std::cin.get());
}

void extract_archive_like_csharp(const std::filesystem::path& path) {
    const std::string name = path.stem().string();
    const std::filesystem::path output = std::filesystem::absolute(
        path.parent_path() / ".." / "_archives" / path.stem())
        .lexically_normal();
    try {
        std::filesystem::create_directories(output);
        std::cout << "Reading " << name << "...";
        const auto bytes = fruityprime::read::file(path);
        std::size_t files_written = 0;
        if (bytes.size() >= 8
            && std::string_view(reinterpret_cast<const char*>(bytes.data()), 7)
                == fruityprime::archive::Archive::Magic
            && bytes[7] == 0) {
            std::cout << " Extracting archive...";
            const auto archive = fruityprime::archive::Archive::parse(bytes);
            files_written = archive.extract(output);
        } else if (!bytes.empty() && bytes[0] == 0x10) {
            auto& paths = fruityprime::formats::global_paths();
            std::filesystem::path temporary = paths.export_path().empty()
                ? std::filesystem::path("__temp")
                : std::filesystem::path(paths.export_path()) / "__temp";
            std::error_code cleanup_error;
            std::filesystem::remove_all(temporary, cleanup_error);
            std::filesystem::create_directories(temporary);
            const std::filesystem::path destination = temporary
                / (name + ".arc");
            std::cout << " Decompressing...";
            const auto decompressed = fruityprime::compression::lz10_decompress(
                bytes);
            if (!fruityprime::read::write_file(destination, decompressed)) {
                throw std::runtime_error("could not write "
                                         + destination.string());
            }
            std::cout << " Extracting archive...";
            const auto archive = fruityprime::archive::Archive::read_file(
                destination);
            files_written = archive.extract(output);
            std::filesystem::remove_all(temporary);
        }
        std::cout << '\n' << "Extracted " << files_written << " file"
                  << (files_written == 1 ? "" : "s") << ".\n";
    } catch (...) {
        std::cout << '\n'
                  << "Failed to extract archive. Verify an archive exists at "
                  << path.string() << ".\n";
    }
}

[[nodiscard]] bool has_setup_argument(
    std::span<const detail::Argument> arguments) noexcept {
    return std::any_of(arguments.begin(), arguments.end(),
                       [](const detail::Argument& argument) {
                           return argument.Name == "setup";
                       });
}

[[nodiscard]] bool raw_has_flag(std::span<const std::string> args,
                                std::string_view flag) noexcept {
    while (!flag.empty() && flag.front() == '-') {
        flag.remove_prefix(1);
    }
    return std::any_of(args.begin(), args.end(), [flag](const std::string& arg) {
        std::string_view value(arg);
        while (!value.empty() && value.front() == '-') {
            value.remove_prefix(1);
        }
        if (value.size() != flag.size()) {
            return false;
        }
        for (std::size_t index = 0; index < value.size(); ++index) {
            if (std::tolower(static_cast<unsigned char>(value[index]))
                != std::tolower(static_cast<unsigned char>(flag[index]))) {
                return false;
            }
        }
        return true;
    });
}

[[nodiscard]] bool windows_front_screen_request(
    std::span<const std::string> args) noexcept {
#ifdef _WIN32
    // The managed ModEntry launches the GUI before CheckSetup.  The Win32
    // entry adapter owns the actual message pump, so leave that request for it
    // instead of allowing Program.cs's missing-path diagnostic to pre-empt
    // the first-run game-files card.
    return (args.empty() || raw_has_flag(args, "-launcher"))
        && !raw_has_flag(args, "-menu");
#else
    static_cast<void>(args);
    return false;
#endif
}

bool check_setup(std::span<const std::string> args) {
    const std::filesystem::path paths_file = "paths.txt";
    if (std::filesystem::is_regular_file(paths_file)
        && !check_version_file()) {
        std::cout << "Your paths.txt file is not compatible with this version of "
                  << fruityprime::mods::Branding::Name
                  << " and needs to be recreated.\n";
        std::cout << "It is recommended that you delete the file as well as any "
                     "extracted game files, then perform setup again.\n\n";
        std::cout << "Press any key to exit...";
        wait_for_key();
        return true;
    }
    if (args.size() == 1 && !args.front().starts_with('-')
        && std::filesystem::is_regular_file(args.front())) {
        const auto result = fruityprime::utility::extract::setup(
            std::filesystem::current_path(), args.front(),
            [](std::string_view message) {
                std::cout << message << '\n';
            });
        if (!result.ok) {
            std::cout << "Press any key to exit...";
            wait_for_key();
        }
        return true;
    }
    if (!std::filesystem::is_regular_file(paths_file)) {
        std::cout << "Could not find the paths.txt file.\n";
        std::cout << "You may need to perform first-time setup by dragging a ROM "
                     "onto the "
                  << fruityprime::mods::Branding::Executable()
                  << " executable.\n\n";
        std::cout << "Press any key to exit...";
        wait_for_key();
        return true;
    }
    auto& paths = fruityprime::formats::global_paths();
    paths.update(std::filesystem::current_path());
    paths.choose_mph_path();
    paths.choose_fh_path();
    return false;
}

void exit_with_usage() {
    // Program.Exit is the normal Program.cs failure boundary.  Keep the
    // wording and ordering here instead of falling through to one of the
    // platform adapters, whose usage text belongs to a different executable.
    std::cout << fruityprime::mods::Branding::Executable() << " usage:\n";
    std::cout << "    -room <room_name -or- room_id>\n";
    std::cout << "    -model <model_name> [recolor_index]\n";
    std::cout << "At most one room may be specified. Any number of models may be specified.\n";
    std::cout << "To load First Hunt models, include -fh in the argument list.\n";
    std::cout << "Available room options: -mode, -players, -boss, -node, -entity\n";
    std::cout << "- or -\n";
    std::cout << "    -extract <archive_path>\n";
    std::cout << "If the target archive is LZ10-compressed, it will be decompressed.\n";
    std::cout << "- or -\n";
    std::cout << "    -export <target_name>\n";
    std::cout << "The export target may be a model or room name.\n";
    SetExitCode(1);
}

[[nodiscard]] bool normal_render_arguments(
    std::span<const detail::Argument> arguments) {
    // Preserve Program.cs's first-match ordering: a numeric first -room is
    // resolved as an id and aborts immediately when the id is unknown; only
    // when that first value is not an Int32 does the string-name branch run.
    if (const auto room_id = detail::try_get_int(arguments, "room", "r")) {
        if (fruityprime::metadata::get_room_by_id(*room_id, true) == nullptr) {
            return false;
        }
        return true;
    }
    if (detail::try_get_string(arguments, "room", "r").has_value()) {
        return true;
    }
    return !detail::get_pairs(arguments, "model", "m").empty();
}

[[nodiscard]] std::string lower_ascii(std::string value) {
    std::transform(value.begin(), value.end(), value.begin(),
                   [](unsigned char character) {
                       return static_cast<char>(std::tolower(character));
                   });
    return value;
}

[[nodiscard]] std::filesystem::path export_sound_directory() {
    return std::filesystem::path(
        fruityprime::formats::global_paths().export_path()) / "_SFX";
}

void export_sdat_streams() {
    auto& paths = fruityprime::formats::global_paths();
    const auto assets = fruityprime::assets::Store::from_directory(
        paths.file_system());
    const auto sdat = fruityprime::sound::Sdat::parse(
        assets.bytes("data/sound/sound_data.sdat"));

    // ReadSdat compacts both GetNames and GetStructs before it constructs a
    // SoundStream.  The resulting public id is therefore the compact STRM
    // ordinal, not the raw INFO index or the FAT file id.
    std::vector<fruityprime::sound::Stream> streams;
    std::size_t name_index = 0;
    for (const auto& info : sdat.streams()) {
        if (!info.present) {
            continue;
        }
        const std::string& name = sdat.stream_names().at(name_index++);
        auto stream = fruityprime::sound::Stream::parse(
            sdat.file(info.file_id),
            static_cast<std::uint32_t>(streams.size()), name);
        stream.volume = info.volume / 127.0F;
        streams.push_back(std::move(stream));
    }
    fruityprime::sound::export_streams(export_sound_directory(), streams);
}

[[nodiscard]] std::filesystem::path resolve_movie_path(
    std::string_view value) {
    const auto& file_system = fruityprime::formats::global_paths().file_system();
    const std::filesystem::path requested(value);
    const std::filesystem::path in_movies =
        std::filesystem::path(file_system) / "movies" / requested;
    if (std::filesystem::is_regular_file(in_movies)) {
        return in_movies;
    }
    const std::filesystem::path in_file_system =
        std::filesystem::path(file_system) / requested;
    if (std::filesystem::is_regular_file(in_file_system)) {
        return in_file_system;
    }
    // VxDecoder.Export falls back to the caller-provided path and lets the
    // file reader report the same failure as File.OpenRead.
    return requested;
}

void export_movie_file(const std::filesystem::path& path) {
    auto decoder = fruityprime::movie::VxDecoder::read_file(path);
    decoder.decode();
    const std::filesystem::path folder =
        std::filesystem::path(
            fruityprime::formats::global_paths().export_path())
        / path.stem();
    static_cast<void>(fruityprime::exporter::write_movie(decoder, folder));
}

void export_all_movies() {
    const auto movies = std::filesystem::path(
        fruityprime::formats::global_paths().file_system()) / "movies";
    std::vector<std::filesystem::path> files;
    for (const auto& entry : std::filesystem::directory_iterator(movies)) {
        if (entry.is_regular_file()) {
            files.push_back(entry.path());
        }
    }
    std::size_t index = 0;
    for (const auto& path : files) {
        // Path.GetExtension is case-sensitive in Movie.ExportAll.
        if (path.extension() != ".vx") {
            continue;
        }
        std::cout << "Exporting " << ++index << " of " << files.size()
                  << ": " << path.filename().string() << '\n';
        export_movie_file(path);
    }
    std::cout << "Done.\n";
}

[[nodiscard]] bool try_export_special(
    const detail::ArgumentList& arguments) {
    const detail::Argument* export_argument = detail::try_get_argument(
        arguments, "export", "e");
    if (export_argument == nullptr || !export_argument->ValueOne.has_value()) {
        return false;
    }

    const std::string target = lower_ascii(*export_argument->ValueOne);
    const auto& paths = fruityprime::formats::global_paths();
    if (target == "sfx" || target == "wfs") {
        const auto assets = fruityprime::assets::Store::from_directory(
            paths.file_system());
        const auto samples = fruityprime::sound::parse_sample_table(
            assets.bytes(target == "sfx"
                             ? fruityprime::sound::SoundSamplesFile
                             : fruityprime::sound::WfsSoundSamplesFile));
        fruityprime::sound::export_samples(
            export_sound_directory(), samples, false,
            target == "sfx" ? "mph_" : fruityprime::sound::WfsExportPrefix);
        return true;
    }
    if (target == "strm") {
        export_sdat_streams();
        return true;
    }
    if (target == "fhsfx") {
        fruityprime::sound::export_all_fh(
            paths.fh_file_system(), export_sound_directory());
        return true;
    }
    if (target == "movie") {
        if (export_argument->ValueTwo.has_value()) {
            export_movie_file(resolve_movie_path(*export_argument->ValueTwo));
        } else {
            export_all_movies();
        }
        return true;
    }
    return false;
}

} // namespace

MainResult Main(std::span<const std::string> args) {
    g_exit_code = 0;
    fruityprime::ConsoleSetup::Run();
#ifdef _WIN32
    fruityprime::mods::console::prepare(args);
#endif
    if (fruityprime::mods::try_handle_headless(args)) {
        return MainResult::Handled;
    }
    if (windows_front_screen_request(args)) {
        return MainResult::ContinueToNativeHost;
    }
    if (check_setup(args)) {
        return MainResult::Handled;
    }
    const detail::ArgumentList arguments = detail::parse_arguments(args);
    if (fruityprime::mods::try_handle(args)) {
        return MainResult::Handled;
    }
    if (arguments.empty()) {
        fruityprime::menu::ShowMenuPrompts();
        return MainResult::Handled;
    }
    if (has_setup_argument(arguments)) {
        const auto archive_directory = std::filesystem::path(
            fruityprime::formats::global_paths().file_system()) / "archives";
        for (const auto& entry : std::filesystem::directory_iterator(
                 archive_directory)) {
            if (!entry.is_regular_file()) {
                continue;
            }
            // Program.cs deliberately passes only the archive's stem to
            // Read.ExtractArchive; preserve that call shape rather than
            // silently changing the current-directory semantics.
            extract_archive_like_csharp(entry.path().stem());
        }
        return MainResult::Handled;
    }
    if (detail::try_get_argument(arguments, "export", "e") != nullptr) {
        if (try_export_special(arguments)) {
            return MainResult::Handled;
        }
        // The managed generic branch is Read.ReadAndExport, whose model,
        // room, image and COLLADA units are not the same as the existing
        // native command-line exporters.  Do not let the platform adapter
        // reinterpret -export as a normal render while that file-level port
        // is still pending.
        exit_with_usage();
        return MainResult::Handled;
    }
    if (const auto extract_value = detail::try_get_string(
            arguments, "extract", "x")) {
        extract_archive_like_csharp(*extract_value);
        return MainResult::Handled;
    }
    if (!normal_render_arguments(arguments)) {
        exit_with_usage();
        return MainResult::Handled;
    }
    return MainResult::ContinueToNativeHost;
}

void SetExitCode(int value) noexcept {
    g_exit_code = value;
}

int ExitCode() noexcept {
    return g_exit_code;
}

namespace detail {

std::optional<Version> parse_version(std::string_view value) {
    value = trim(value);
    if (value.empty()) return std::nullopt;
    Version result;
    std::size_t part = 0;
    while (!value.empty() && part < result.parts.size()) {
        const std::size_t dot = value.find('.');
        const std::string_view number = value.substr(0, dot);
        int parsed = 0;
        if (!try_parse_int32(number, parsed) || parsed < 0) {
            return std::nullopt;
        }
        result.parts[part++] = parsed;
        if (dot == std::string_view::npos) {
            value = {};
            break;
        }
        value.remove_prefix(dot + 1);
        if (value.empty()) return std::nullopt;
    }
    return value.empty() && part >= 2 && part <= result.parts.size()
        ? std::optional<Version>(result) : std::nullopt;
}

bool check_version(std::string_view value) noexcept {
    try {
        const auto parsed = parse_version(value);
        return parsed.has_value() && !(*parsed < MinimumExtractVersion);
    } catch (...) {
        return false;
    }
}

ArgumentList parse_arguments(std::span<const std::string> args) {
    ArgumentList arguments;
    arguments.reserve(args.size());
    for (std::size_t index = 0; index < args.size(); ++index) {
        const std::string& original = args[index];
        if (original.size() <= 1 || original.front() != '-') {
            continue;
        }

        Argument argument;
        argument.Name = original.substr(1);
        if (index + 1 >= args.size()) {
            arguments.push_back(std::move(argument));
            continue;
        }

        const std::string& value_one = args[index + 1];
        if (!value_one.empty() && value_one.front() == '-') {
            arguments.push_back(std::move(argument));
            continue;
        }

        argument.ValueOne = value_one;
        if (index + 2 < args.size()) {
            const std::string& value_two = args[index + 2];
            if (value_two.empty() || value_two.front() != '-') {
                argument.ValueTwo = value_two;
                ++index;
            }
        }
        arguments.push_back(std::move(argument));
        ++index;
    }
    return arguments;
}

const Argument* try_get_argument(std::span<const Argument> arguments,
                                  std::string_view full_name,
                                  std::string_view short_name) noexcept {
    for (const Argument& argument : arguments) {
        if (argument.Name == full_name || argument.Name == short_name) {
            return &argument;
        }
    }
    return nullptr;
}

std::optional<std::string> try_get_string(
    std::span<const Argument> arguments, std::string_view full_name,
    std::string_view short_name) {
    const Argument* argument =
        try_get_argument(arguments, full_name, short_name);
    if (argument == nullptr || !argument->ValueOne.has_value()) {
        return std::nullopt;
    }
    return argument->ValueOne;
}

std::optional<int> try_get_int(std::span<const Argument> arguments,
                               std::string_view full_name,
                               std::string_view short_name) {
    const auto string_value =
        try_get_string(arguments, full_name, short_name);
    if (!string_value.has_value()) {
        return std::nullopt;
    }
    int value = 0;
    if (!try_parse_int32(*string_value, value)) {
        return std::nullopt;
    }
    return value;
}

std::vector<std::pair<std::string, int>> get_pairs(
    std::span<const Argument> arguments, std::string_view full_name,
    std::string_view short_name) {
    std::vector<std::pair<std::string, int>> pairs;
    for (const Argument& argument : arguments) {
        if ((argument.Name != full_name && argument.Name != short_name)
            || !argument.ValueOne.has_value()) {
            continue;
        }
        int value_two = 0;
        if (argument.ValueTwo.has_value()) {
            (void)try_parse_int32(*argument.ValueTwo, value_two);
        }
        pairs.emplace_back(*argument.ValueOne, value_two);
    }
    return pairs;
}

} // namespace detail

} // namespace MphReadNative::Program
