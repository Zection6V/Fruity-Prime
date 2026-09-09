#include "Mods/branding.hpp"
#include "Mods/Update/build_version.hpp"
#include "Mods/debug_log.hpp"
#include "Mods/Network/net_protocol.hpp"
#include "Mods/pause_menu.hpp"
#include "Mods/screen_capture.hpp"
#include "Mods/thumbnail_batch.hpp"
#include "Mods/thumbnail_capture.hpp"
#include "Mods/thumbnail_generator.hpp"
#include "Mods/thumbnail_host.hpp"
#include "Mods/thumbnail_log.hpp"
#include "Mods/thumbnail_mode.hpp"
#include "Mods/Update/update.hpp"
#include "Mods/window_mode.hpp"
#include "Mods/world_events.hpp"

#include <array>
#include <chrono>
#include <cstdio>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <iterator>
#include <iostream>
#include <stdexcept>
#include <string>
#include <span>
#include <vector>

#include <zlib.h>

namespace {

void require(bool condition, const std::string& message) {
    if (!condition) {
        throw std::runtime_error(message);
    }
}

void append_little16(std::vector<std::uint8_t>& output, std::uint16_t value) {
    output.push_back(static_cast<std::uint8_t>(value & 0xffU));
    output.push_back(static_cast<std::uint8_t>((value >> 8) & 0xffU));
}

void append_little32(std::vector<std::uint8_t>& output, std::uint32_t value) {
    output.push_back(static_cast<std::uint8_t>(value & 0xffU));
    output.push_back(static_cast<std::uint8_t>((value >> 8) & 0xffU));
    output.push_back(static_cast<std::uint8_t>((value >> 16) & 0xffU));
    output.push_back(static_cast<std::uint8_t>((value >> 24) & 0xffU));
}

[[nodiscard]] std::vector<std::uint8_t> raw_deflate(
    std::span<const std::uint8_t> plain, int window_bits) {
    z_stream stream{};
    require(deflateInit2(&stream, Z_DEFAULT_COMPRESSION, Z_DEFLATED,
                         window_bits, 8, Z_DEFAULT_STRATEGY) == Z_OK,
            "could not initialize test compressor");
    std::vector<std::uint8_t> output(compressBound(
        static_cast<uLong>(plain.size())));
    stream.next_in = const_cast<Bytef*>(
        reinterpret_cast<const Bytef*>(plain.data()));
    stream.avail_in = static_cast<uInt>(plain.size());
    stream.next_out = reinterpret_cast<Bytef*>(output.data());
    stream.avail_out = static_cast<uInt>(output.size());
    const int result = deflate(&stream, Z_FINISH);
    require(result == Z_STREAM_END, "could not compress test archive");
    output.resize(stream.total_out);
    deflateEnd(&stream);
    return output;
}

[[nodiscard]] std::uint32_t crc32_of(std::span<const std::uint8_t> data) {
    return static_cast<std::uint32_t>(crc32(
        crc32(0L, Z_NULL, 0), reinterpret_cast<const Bytef*>(data.data()),
        static_cast<uInt>(data.size())));
}

struct TestZipEntry {
    std::string name;
    std::vector<std::uint8_t> plain;
    std::vector<std::uint8_t> compressed;
    std::uint16_t method = 0;
    std::uint32_t local_offset = 0;
};

[[nodiscard]] std::vector<std::uint8_t> make_test_zip() {
    TestZipEntry binary{
        "FruityPrime.exe",
        {'n', 'a', 't', 'i', 'v', 'e', '-', 'b', 'i', 'n', 'a', 'r', 'y'},
        {}, 0, 0};
    binary.compressed = binary.plain;
    TestZipEntry readme{
        "nested/readme.txt",
        {'u', 'p', 'd', 'a', 't', 'e', '-', 'o', 'k'},
        {}, 8, 0};
    readme.compressed = raw_deflate(readme.plain, -MAX_WBITS);
    std::array<TestZipEntry*, 2> entries{&binary, &readme};

    std::vector<std::uint8_t> output;
    for (TestZipEntry* entry : entries) {
        entry->local_offset = static_cast<std::uint32_t>(output.size());
        append_little32(output, 0x04034b50U);
        append_little16(output, 20);
        append_little16(output, 0);
        append_little16(output, entry->method);
        append_little16(output, 0);
        append_little16(output, 0);
        append_little32(output, crc32_of(entry->plain));
        append_little32(output, static_cast<std::uint32_t>(
                                      entry->compressed.size()));
        append_little32(output, static_cast<std::uint32_t>(entry->plain.size()));
        append_little16(output, static_cast<std::uint16_t>(entry->name.size()));
        append_little16(output, 0);
        output.insert(output.end(), entry->name.begin(), entry->name.end());
        output.insert(output.end(), entry->compressed.begin(),
                      entry->compressed.end());
    }
    const std::uint32_t central_offset = static_cast<std::uint32_t>(output.size());
    for (const TestZipEntry* entry : entries) {
        append_little32(output, 0x02014b50U);
        append_little16(output, 20);
        append_little16(output, 20);
        append_little16(output, 0);
        append_little16(output, entry->method);
        append_little16(output, 0);
        append_little16(output, 0);
        append_little32(output, crc32_of(entry->plain));
        append_little32(output, static_cast<std::uint32_t>(
                                      entry->compressed.size()));
        append_little32(output, static_cast<std::uint32_t>(entry->plain.size()));
        append_little16(output, static_cast<std::uint16_t>(entry->name.size()));
        append_little16(output, 0);
        append_little16(output, 0);
        append_little16(output, 0);
        append_little16(output, 0);
        append_little32(output, 0);
        append_little32(output, entry->local_offset);
        output.insert(output.end(), entry->name.begin(), entry->name.end());
    }
    const std::uint32_t central_size =
        static_cast<std::uint32_t>(output.size()) - central_offset;
    append_little32(output, 0x06054b50U);
    append_little16(output, 0);
    append_little16(output, 0);
    append_little16(output, static_cast<std::uint16_t>(entries.size()));
    append_little16(output, static_cast<std::uint16_t>(entries.size()));
    append_little32(output, central_size);
    append_little32(output, central_offset);
    append_little16(output, 0);
    return output;
}

void put_octal(std::uint8_t* field, std::size_t length, std::uint64_t value) {
    std::array<char, 32> text{};
    const int written = std::snprintf(text.data(), text.size(), "%0*llo",
                                      static_cast<int>(length - 1),
                                      static_cast<unsigned long long>(value));
    require(written > 0 && static_cast<std::size_t>(written) < length,
            "test tar octal field did not fit");
    std::memcpy(field, text.data(), static_cast<std::size_t>(written));
    field[length - 1] = 0;
}

void append_tar_entry(std::vector<std::uint8_t>& output,
                      std::string_view name,
                      std::span<const std::uint8_t> data) {
    std::array<std::uint8_t, 512> header{};
    require(name.size() < 100, "test tar name did not fit");
    std::memcpy(header.data(), name.data(), name.size());
    put_octal(header.data() + 100, 8, 0777);
    put_octal(header.data() + 108, 8, 0);
    put_octal(header.data() + 116, 8, 0);
    put_octal(header.data() + 124, 12, data.size());
    put_octal(header.data() + 136, 12, 0);
    std::fill(header.begin() + 148, header.begin() + 156, 0x20);
    header[156] = '0';
    std::memcpy(header.data() + 257, "ustar\0", 6);
    std::memcpy(header.data() + 263, "00", 2);
    std::uint32_t checksum = 0;
    for (const std::uint8_t value : header) {
        checksum += value;
    }
    put_octal(header.data() + 148, 8, checksum);
    header[155] = ' ';
    output.insert(output.end(), header.begin(), header.end());
    output.insert(output.end(), data.begin(), data.end());
    output.insert(output.end(),
                  (512U - (data.size() % 512U)) % 512U, std::uint8_t{0});
}

[[nodiscard]] std::vector<std::uint8_t> make_test_tar_gz() {
    const std::vector<std::uint8_t> binary{
        't', 'a', 'r', '-', 'b', 'i', 'n', 'a', 'r', 'y'};
    const std::vector<std::uint8_t> readme{'t', 'a', 'r', '-', 'o', 'k'};
    std::vector<std::uint8_t> tar;
    append_tar_entry(tar, "FruityPrime", binary);
    append_tar_entry(tar, "nested/readme.txt", readme);
    tar.insert(tar.end(), 1024, std::uint8_t{0});
    return raw_deflate(tar, MAX_WBITS + 16);
}

void write_bytes(const std::filesystem::path& path,
                 std::span<const std::uint8_t> data) {
    std::ofstream output(path, std::ios::binary | std::ios::trunc);
    require(static_cast<bool>(output), "could not create test archive");
    output.write(reinterpret_cast<const char*>(data.data()),
                 static_cast<std::streamsize>(data.size()));
    require(static_cast<bool>(output), "could not write test archive");
}

[[nodiscard]] std::string read_text(const std::filesystem::path& path) {
    std::ifstream input(path, std::ios::binary);
    return {std::istreambuf_iterator<char>(input), std::istreambuf_iterator<char>()};
}

} // namespace

int main() {
    const auto directory = std::filesystem::temp_directory_path()
        / ("fruity_prime_native_mods_test_"
           + std::to_string(std::chrono::steady_clock::now()
                                .time_since_epoch().count()));
    try {
        require(fruityprime::update::BuildVersion::parse("v1.2.3")
                    == fruityprime::update::Version{1, 2, 3},
                "release version did not parse");
        require(fruityprime::update::BuildVersion::parse("1.2")
                    == fruityprime::update::Version{1, 2, 0},
                "short release version did not normalize");
        require(!fruityprime::update::BuildVersion::parse("1.0.0"),
                "unstamped SDK version was treated as a release");
        require(!fruityprime::update::BuildVersion::parse("1.2.3-rc1"),
                "pre-release version was treated as a release");
        require(fruityprime::update::BuildVersion::display(std::nullopt)
                    == "a local build",
                "local build version display changed");

        const std::string release_json = R"({
            "tag_name": "v1.4.0",
            "body": "Fixes \u0048UD and networking.",
            "html_url": "https://github.com/liveteklol/Fruity-Prime/releases/tag/v1.4.0",
            "assets": [
              {"name":"FruityPrime-v1.4.0-win-x64.zip",
               "browser_download_url":"https://github.com/liveteklol/Fruity-Prime/releases/download/v1.4.0/FruityPrime-v1.4.0-win-x64.zip",
               "size":12345},
              {"name":"FruityPrime-v1.4.0-server-win-x64.zip",
               "browser_download_url":"https://objects.githubusercontent.com/server.zip",
               "size":6789}
            ]
        })";
        const auto update = fruityprime::update::UpdateCheck::parse(
            release_json, fruityprime::update::Version{1, 3, 9});
        require(update.has_value() && update->tag == "v1.4.0"
                    && update->asset_name == "FruityPrime-v1.4.0-win-x64.zip"
                    && update->asset_size == 12345
                    && update->notes.find("HUD") != std::string::npos,
                "release JSON did not select the game package");
        const auto server_update = fruityprime::update::UpdateCheck::parse(
            release_json, fruityprime::update::Version{1, 3, 9}, true);
        require(server_update.has_value()
                    && server_update->asset_name.find("-server-")
                           != std::string::npos,
                "release JSON did not select the server package");
        require(!fruityprime::update::UpdateCheck::parse(
                    release_json, fruityprime::update::Version{1, 4, 0}),
                "current release was reported as an update");
        require(fruityprime::update::UpdateCheck::last_reason().find(
                    "already the latest") != std::string::npos,
                "current release reason was not retained");
        require(!fruityprime::update::UpdateCheck::parse(
                    "{not a release}", fruityprime::update::Version{1, 0, 1}),
                "malformed release JSON was accepted");
        require(fruityprime::update::UpdateCheck::is_allowed_url(
                    "https://github.com/liveteklol/Fruity-Prime/releases")
                    && fruityprime::update::UpdateCheck::is_allowed_url(
                        "https://objects.githubusercontent.com/a.zip")
                    && !fruityprime::update::UpdateCheck::is_allowed_url(
                        "http://github.com/a")
                    && !fruityprime::update::UpdateCheck::is_allowed_url(
                        "https://evil.example/a")
                    && !fruityprime::update::UpdateCheck::is_allowed_url(
                        "https://github.com@evil.example/a"),
                "update URL allow-list changed");
        fruityprime::update::Updater updater;
        require(updater.describe(*update).find("v1.4.0 is available")
                    != std::string::npos,
                "updater description did not include the release");
        const auto bad_download = fruityprime::update::UpdateDownload::fetch(
            "http://github.com/not-allowed", (directory / "package.zip").string());
        require(!bad_download.ok && !std::filesystem::exists(directory / "package.zip.part"),
                "invalid update download was not rejected");

        const std::vector<std::uint8_t> black_pixels(2 * 2 * 3, 0);
        require(fruityprime::mods::screen_capture::lit_fraction(black_pixels)
                    == 0.0
                && !fruityprime::mods::screen_capture::save_rgb(
                    directory / "black.png", 2, 2, black_pixels),
                "black screenshot was accepted");
        const std::vector<std::uint8_t> bottom_up_pixels{
            0, 0, 0, 0, 0, 0,
            0, 0, 0, 255, 24, 8
        };
        const auto screenshot_path = directory / "frame.png";
        require(fruityprime::mods::screen_capture::lit_fraction(
                    bottom_up_pixels) == 0.25
                    && fruityprime::mods::screen_capture::save_rgb(
                        screenshot_path, 2, 2, bottom_up_pixels),
                "rendered screenshot was not saved");
        std::ifstream screenshot(screenshot_path, std::ios::binary);
        std::array<char, 8> png_signature{};
        screenshot.read(png_signature.data(),
                        static_cast<std::streamsize>(png_signature.size()));
        require(screenshot.gcount() == 8
                    && std::string(png_signature.data(), 8)
                        == std::string("\x89PNG\r\n\x1a\n", 8),
                "screenshot output was not a PNG");
        screenshot.close();

        fruityprime::mods::thumbnail::Controller thumbnail_mode;
        float sfx_volume = 0.72F;
        float music_volume = 0.58F;
        require(thumbnail_mode.enter(sfx_volume, music_volume)
                    && thumbnail_mode.active() && sfx_volume == 0.0F
                    && music_volume == 0.0F,
                "thumbnail mode did not mute both audio channels");
        require(!thumbnail_mode.enter(sfx_volume, music_volume),
                "thumbnail mode overwrote saved volume on re-entry");
        require(thumbnail_mode.exit(sfx_volume, music_volume)
                    && !thumbnail_mode.active()
                    && sfx_volume == 0.72F && music_volume == 0.58F,
                "thumbnail mode did not restore audio volumes");
        require(!thumbnail_mode.exit(sfx_volume, music_volume),
                "thumbnail mode treated an inactive exit as active");

        fruityprime::mods::thumbnail::CaptureState capture_state;
        for (int frame = 0;
             frame < fruityprime::mods::thumbnail::CaptureState::SettleFrames;
             ++frame) {
            require(!capture_state.capture_due(),
                    "thumbnail capture became due before settle frames elapsed");
        }
        require(capture_state.capture_due(),
                "thumbnail capture did not become due after settling");
        const auto hidden_failure = capture_state.finish_capture(false, false);
        require(!hidden_failure.captured && hidden_failure.show_window
                    && !hidden_failure.close_window
                    && hidden_failure.attempt == 1
                    && capture_state.attempts() == 1,
                "thumbnail hidden-window retry policy changed");
        for (int frame = 0;
             frame < fruityprime::mods::thumbnail::CaptureState::RetryFrames;
             ++frame) {
            require(!capture_state.capture_due(),
                    "thumbnail retry became due before retry frames elapsed");
        }
        require(capture_state.capture_due(),
                "thumbnail retry did not become due after its delay");
        const auto visible_failure = capture_state.finish_capture(false, true);
        require(!visible_failure.captured && !visible_failure.show_window
                    && !visible_failure.close_window
                    && visible_failure.attempt == 2,
                "thumbnail visible-window retry policy changed");
        for (int frame = 0;
             frame < fruityprime::mods::thumbnail::CaptureState::RetryFrames;
             ++frame) {
            require(!capture_state.capture_due(),
                    "thumbnail final retry became due before retry frames elapsed");
        }
        require(capture_state.capture_due(),
                "thumbnail final retry did not become due");
        const auto final_failure = capture_state.finish_capture(false, true);
        require(!final_failure.captured && final_failure.close_window
                    && capture_state.gave_up()
                    && final_failure.attempt
                        == fruityprime::mods::thumbnail::CaptureState::MaxAttempts,
                "thumbnail max-attempt close policy changed");
        require(!capture_state.capture_due(),
                "thumbnail capture continued after giving up");

        fruityprime::mods::thumbnail::CaptureState success_state;
        for (int frame = 0;
             frame < fruityprime::mods::thumbnail::CaptureState::SettleFrames;
             ++frame) {
            (void)success_state.capture_due();
        }
        require(success_state.capture_due(),
                "thumbnail success path did not become due");
        const auto success = success_state.finish_capture(true, false);
        require(success.captured && success.close_window
                    && !success.show_window && success.attempt == 1
                    && success_state.captured() && !success_state.gave_up(),
                "thumbnail successful capture policy changed");

        const auto thumbnail_root = directory / "game-files";
        require(fruityprime::mods::thumbnail::safe_room_key(
                    "MP4 HIGHGROUND - EXPANDED")
                    == "mp4_highground___expanded",
                "thumbnail room key normalization changed");
        require(fruityprime::mods::thumbnail::path_for(
                    thumbnail_root, "MP1 SANCTORUS")
                    == thumbnail_root / "thumbnails" / "mp1_sanctorus.png",
                "thumbnail path did not use the cache directory");
        const std::array<std::string_view, 3> thumbnail_rooms{
            "MP1 SANCTORUS", "MP2 HARVESTER", "EMPTY"};
        require(fruityprime::mods::thumbnail::ensure_cache_directory(
                    thumbnail_root),
                "thumbnail cache directory could not be created");
        {
            std::ofstream output(fruityprime::mods::thumbnail::path_for(
                thumbnail_root, "MP1 SANCTORUS"), std::ios::binary);
            output << 'x';
        }
        {
            std::ofstream output(fruityprime::mods::thumbnail::path_for(
                thumbnail_root, "MP2 HARVESTER"), std::ios::binary);
        }
        const auto missing_thumbnails = fruityprime::mods::thumbnail::missing(
            thumbnail_root, thumbnail_rooms);
        require(missing_thumbnails.size() == 2
                    && missing_thumbnails[0] == "MP2 HARVESTER"
                    && missing_thumbnails[1] == "EMPTY"
                    && fruityprime::mods::thumbnail::exists(
                        thumbnail_root, "MP1 SANCTORUS")
                    && !fruityprime::mods::thumbnail::exists(
                        thumbnail_root, "MP2 HARVESTER"),
                "thumbnail cache hit and zero-byte retry rules changed");

        const std::array<std::string_view, 5> batch_rooms{
            "MP1 SANCTORUS", "MP2 HARVESTER", "MP3 PROVING GROUND",
            "MP4 HIGHGROUND", "MP5 FUEL SLUICE"};
        const auto batch_shares = fruityprime::mods::thumbnail::shares(
            batch_rooms, 2);
        require(batch_shares.size() == 2
                    && batch_shares[0].size() == 3
                    && batch_shares[1].size() == 2
                    && batch_shares[0][1] == "MP3 PROVING GROUND"
                    && batch_shares[1][1] == "MP4 HIGHGROUND",
                "thumbnail rooms were not dealt round-robin");
        const auto commands = fruityprime::mods::thumbnail::worker_commands(
            "FruityPrime.exe", "C:/games", batch_rooms, 2, 320, 180);
        require(commands.size() == 2
                    && commands[0].arguments.size() == 8
                    && commands[0].arguments[0] == "-thumbnail"
                    && commands[0].arguments[1] == "MP1 SANCTORUS"
                    && commands[0].arguments[6] == "-size"
                    && commands[0].arguments[7] == "320x180"
                    && commands[1].rooms[0] == "MP2 HARVESTER",
                "thumbnail worker command shape changed");
        int serial_reports = 0;
        const auto serial = fruityprime::mods::thumbnail::run_serial(
            thumbnail_root,
            std::array<std::string_view, 1>{"EMPTY"}, 64, 36,
            [&thumbnail_root](std::string_view room, int width, int height) {
                require(room == "EMPTY" && width == 64 && height == 36,
                        "thumbnail serial callback arguments changed");
                std::ofstream output(fruityprime::mods::thumbnail::path_for(
                    thumbnail_root, room), std::ios::binary);
                output << "png";
                return true;
            },
            [&serial_reports](std::string_view line) {
                if (line.find("ok") != std::string_view::npos) {
                    ++serial_reports;
                }
            });
        require(serial.written == 1 && serial.failed.empty()
                    && serial_reports == 1,
                "thumbnail serial fallback did not verify its cache result");

        fruityprime::mods::thumbnail::Host thumbnail_host;
        require(!thumbnail_host.can_render("C:/missing/FruityPrime.exe"),
                "thumbnail host reported an unavailable renderer");
        thumbnail_host.set_renderer(
            [](const std::filesystem::path&, std::span<const std::string_view> rooms,
               int width, int height, const fruityprime::mods::thumbnail::Report&) {
                require(rooms.size() == 1 && rooms[0] == "HOST ROOM"
                            && width == 1600 && height == 900,
                        "thumbnail host did not pass the missing room list");
                return fruityprime::mods::thumbnail::RunResult{1, {}};
            });
        const auto hosted = thumbnail_host.render_missing(
            thumbnail_root,
            std::array<std::string_view, 2>{"MP1 SANCTORUS", "HOST ROOM"},
            1600, 900);
        require(hosted.written == 1 && hosted.failed.empty()
                    && thumbnail_host.can_render("C:/missing/FruityPrime.exe"),
                "thumbnail host renderer seam did not run");

        const auto thumbnail_log_path = fruityprime::mods::thumbnail::log_path(
            thumbnail_root);
        fruityprime::mods::thumbnail::Log thumbnail_log;
        require(thumbnail_log.begin(thumbnail_log_path, 2, "a local build",
                                     "native-test", "test-file")
                    && thumbnail_log.write("MP1 SANCTORUS: captured")
                    && !thumbnail_log.failed(),
                "thumbnail log did not start and append");
        std::ifstream thumbnail_log_file(thumbnail_log_path);
        const std::string thumbnail_log_text(
            (std::istreambuf_iterator<char>(thumbnail_log_file)),
            std::istreambuf_iterator<char>());
        require(thumbnail_log_text.find(
                    "=== Fruity Prime preview generation,")
                    != std::string::npos
                    && thumbnail_log_text.find(
                           "build a local build, assembly native-test, file test-file")
                           != std::string::npos
                    && thumbnail_log_text.find(
                           "MP1 SANCTORUS: captured") != std::string::npos,
                "thumbnail log content did not match the managed format");
        thumbnail_log_file.close();

        require(fruityprime::branding::executable_name(
                    "C:/games/FruityPrime.exe") == "FruityPrime",
                "branding executable name was not normalized");
        require(fruityprime::branding::name_and_version().find(
                    "Fruity Prime") != std::string::npos,
                "branding display name was not used");
        require(std::string(fruityprime::mods::Branding::Name)
                    == fruityprime::branding::Name
                    && std::string(fruityprime::mods::Branding::FileName)
                        == fruityprime::branding::FileName,
                "managed Branding constants were not exposed");
        require(!fruityprime::mods::Branding::Executable().empty(),
                "managed Branding executable name was empty");
        require(fruityprime::mods::Branding::NameAndVersion().find(
                    fruityprime::mods::Branding::Name) == 0,
                "managed Branding version text was not reproduced");

        require(fruityprime::window::parse_start_mode(
                    "  BORDERLESS FULLSCREEN  ")
                    == fruityprime::window::StartMode::BorderlessFullscreen,
                "window mode did not trim and parse");
        fruityprime::window::State window;
        window.startup = fruityprime::window::StartMode::BorderlessFullscreen;
        window.apply_startup();
        require(window.fullscreen && window.topmost,
                "borderless startup did not enter fullscreen state");
        window.toggle();
        require(!window.fullscreen && !window.topmost,
                "window toggle did not leave fullscreen state");
        window.set_topmost(true);
        window.leave();
        require(!window.topmost, "leaving fullscreen kept topmost state");

        fruityprime::mods::pause::Controller pause;
        require(!pause.open(), "pause menu started open");
        require(pause.handle_escape({10, 20, 960, 720}) && pause.open(),
                "pause menu did not consume opening Escape");
        require(pause.window_moved()
                    && pause.window_rect()
                           == fruityprime::mods::pause::WindowRect{
                               10, 20, 960, 720},
                "pause menu did not track its first window rectangle");
        pause.clear_window_moved();
        require(!pause.window_moved(),
                "pause menu kept a cleared window-moved edge");
        pause.update_window_rect({11, 20, 960, 720});
        require(pause.window_moved(),
                "pause menu missed a moved window rectangle");
        require(pause.handle_escape({11, 20, 960, 720}) && !pause.open(),
                "pause menu did not consume closing Escape");
        require(pause.refocus_requested(),
                "pause menu did not request game-window focus");
        pause.clear_refocus_request();
        pause.request_leave();
        pause.poll();
        require(pause.left_match() && !pause.open(),
                "pause menu leave request was not applied");
        pause.reset();
        require(!pause.left_match() && !pause.quit_program(),
                "pause menu reset did not clear result flags");
        pause.request_quit();
        pause.poll();
        require(pause.quit_program() && !pause.open(),
                "pause menu quit request was not applied");

        fruityprime::world::Events events;
        events.reset();
        events.note_jump_pad(0, 11);
        require(events.jump_pads_for(0) == 0,
                "world event was recorded while watching was disabled");
        events.watching = true;
        events.note_jump_pad(0, 11);
        events.note_jump_pad(0, 12);
        events.note_teleport(7, 99);
        events.note_teleport(-1, 100);
        require(events.jump_pads_for(0) == 2
                    && events.last_jump_pad_id(0) == 12
                    && events.teleports_for(7) == 1
                    && events.last_teleporter_id(7) == 99,
                "world event counters did not record valid slots");
        require(events.jump_pads_for(-1) == 0
                    && events.last_teleporter_id(8) == -1,
                "world event invalid slot handling changed");
        require(fruityprime::world::WorldEvents::JumpPadsFor(0) == 2
                    && fruityprime::world::WorldEvents::TeleportsFor(7) == 1,
                "Events adapter did not share the static WorldEvents state");
        fruityprime::world::Events second_events;
        require(second_events.watching
                    && second_events.last_jump_pad_id(0) == 12,
                "WorldEvents was still isolated per native instance");

        std::filesystem::create_directories(directory);
        require(!fruityprime::update::DesktopUpdate::supported(directory),
                "local build was offered in-place update support");
        fruityprime::update::UpdateInfo no_package;
        require(!fruityprime::update::UpdateInstall::can_install(
                    directory, no_package),
                "update installer accepted a release without an asset");

        const auto zip_archive = directory / "fixture-update.zip";
        write_bytes(zip_archive, make_test_zip());
        const auto zip_output = directory / "fixture-zip-output";
        std::string extraction_error;
        require(fruityprime::update::DesktopUpdate::extract_package(
                    zip_archive, zip_output, true, extraction_error),
                "native ZIP update extraction failed: " + extraction_error);
        require(read_text(zip_output / "FruityPrime.exe") == "native-binary"
                    && read_text(zip_output / "nested" / "readme.txt")
                        == "update-ok",
                "native ZIP update contents changed");

        const auto tar_archive = directory / "fixture-update.tar.gz";
        write_bytes(tar_archive, make_test_tar_gz());
        const auto tar_output = directory / "fixture-tar-output";
        extraction_error.clear();
        require(fruityprime::update::DesktopUpdate::extract_package(
                    tar_archive, tar_output, false, extraction_error),
                "native tar.gz update extraction failed: " + extraction_error);
        require(read_text(tar_output / "FruityPrime") == "tar-binary"
                    && read_text(tar_output / "nested" / "readme.txt")
                        == "tar-ok",
                "native tar.gz update contents changed");

        fruityprime::debug::Log log;
        require(!log.attach(directory, false),
                "debug log attached when logging was disabled");
        require(log.attach(directory, true), "debug log could not attach");
        log.line("test", "portable logging works");
        const auto log_path = log.path();
        log.detach();
        require(!log.active() && std::filesystem::exists(log_path),
                "debug log did not close a created file");
        std::ifstream log_file(log_path);
        const std::string log_text((std::istreambuf_iterator<char>(log_file)),
                                   std::istreambuf_iterator<char>());
        require(log_text.find("[test] portable logging works")
                    != std::string::npos
                    && log_text.find(std::to_string(
                           fruityprime::net::NetConfig::ProtocolVersion))
                           != std::string::npos,
                "debug log did not write category and protocol");
        log_file.close();

        std::error_code cleanup_error;
        std::filesystem::remove_all(directory, cleanup_error);
        require(!std::filesystem::exists(directory),
                "mods test directory could not be removed");
        std::cout << "native portable mods tests passed\n";
        return 0;
    } catch (const std::exception& error) {
        std::error_code cleanup_error;
        std::filesystem::remove_all(directory, cleanup_error);
        std::cerr << error.what() << '\n';
        return 1;
    }
}
