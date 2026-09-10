#include "Mods/Network/demo.hpp"
#include "Formats/demo_info.hpp"
#include "Mods/Network/demo_library.hpp"
#include "Mods/Network/demo_playback.hpp"
#include "Mods/Network/demo_recorder.hpp"

#include <array>
#include <cassert>
#include <chrono>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <iterator>
#include <sstream>
#include <string>
#include <vector>

namespace {

[[nodiscard]] std::filesystem::path temporary_path(const char* suffix) {
    const auto stamp = std::chrono::steady_clock::now().time_since_epoch().count();
    return std::filesystem::temp_directory_path()
        / ("fruity_prime_demo_" + std::to_string(stamp) + suffix);
}

[[nodiscard]] std::vector<std::uint8_t> read_bytes(
    const std::filesystem::path& path) {
    std::ifstream input(path, std::ios::binary);
    return std::vector<std::uint8_t>(
        std::istreambuf_iterator<char>(input), std::istreambuf_iterator<char>());
}

void write_bytes(const std::filesystem::path& path,
                 const std::vector<std::uint8_t>& bytes) {
    std::ofstream output(path, std::ios::binary | std::ios::trunc);
    output.write(reinterpret_cast<const char*>(bytes.data()),
                 static_cast<std::streamsize>(bytes.size()));
}

void check_reader(const std::filesystem::path& path) {
    auto reader = fruityprime::demo::Reader::open(path);
    assert(reader.has_value());
    assert(reader->protocol_version() == fruityprime::net::NetConfig::ProtocolVersion);

    const auto first = reader->read_next();
    assert(first.has_value() && first->frame == 0
           && first->data == std::vector<std::uint8_t>({1, 2, 3}));
    const auto second = reader->read_next();
    assert(second.has_value() && second->frame == 3
           && second->data == std::vector<std::uint8_t>({'h', 'i'}));
    const auto third = reader->read_next();
    assert(third.has_value() && third->frame == 300
           && third->data.size() == 65'535);
    const auto clamped = reader->read_next();
    assert(clamped.has_value() && clamped->frame == 300
           && clamped->data == std::vector<std::uint8_t>({0}));
    assert(!reader->read_next().has_value());
}

void check_managed_style_deflate(const std::filesystem::path& path) {
    // A deterministic raw-DEFLATE dynamic block, generated from 200 records
    // with zlib's default compression strategy.  The native writer uses
    // stored blocks, so this fixture specifically exercises the reader path
    // used for files emitted by the managed DeflateStream writer.
    static constexpr std::array<std::uint8_t, 351> compressed{
        0x25, 0xc5, 0x57, 0x43, 0x8d, 0x01, 0x00, 0x00, 0xd0, 0x2f, 0x5d, 0x3b,
        0x7b, 0xef, 0x32, 0x42, 0xb6, 0x50, 0x88, 0x92, 0xca, 0x2e, 0x85, 0xe2,
        0x66, 0x67, 0x13, 0xd9, 0x2b, 0x32, 0xb3, 0xf7, 0xc8, 0x26, 0x5c, 0x42,
        0x14, 0xd9, 0x2b, 0xb2, 0xfe, 0x96, 0x87, 0x73, 0x5e, 0x4e, 0x10, 0x0a,
        0x82, 0x20, 0x1c, 0x89, 0x0a, 0x05, 0x51, 0x6a, 0xa4, 0x68, 0x85, 0xd4,
        0x58, 0x4d, 0xd4, 0x54, 0xcd, 0xd4, 0x5c, 0x2d, 0xd4, 0x52, 0x31, 0x6a,
        0xa5, 0xd6, 0x6a, 0xa3, 0xb6, 0x6a, 0xa7, 0xf6, 0xea, 0xa0, 0x8e, 0xea,
        0xa4, 0xce, 0xea, 0xa2, 0xae, 0xea, 0xa6, 0xee, 0xea, 0xa1, 0x9e, 0xea,
        0xa5, 0xde, 0xea, 0xa3, 0x58, 0xc5, 0xa9, 0xaf, 0xfa, 0xa9, 0xbf, 0x06,
        0x28, 0x5e, 0x03, 0x35, 0x48, 0x83, 0x95, 0xa0, 0x21, 0x1a, 0xaa, 0x61,
        0x1a, 0xae, 0x11, 0x1a, 0xa9, 0x51, 0x1a, 0xad, 0x44, 0x8d, 0xd1, 0x58,
        0x8d, 0x53, 0x92, 0x92, 0x35, 0x5e, 0x13, 0x34, 0x51, 0x29, 0x9a, 0xa4,
        0xc9, 0x4a, 0x55, 0x9a, 0xa6, 0x28, 0x5d, 0x53, 0x95, 0xa1, 0x4c, 0x65,
        0x69, 0x9a, 0xa6, 0x6b, 0x86, 0x66, 0x6a, 0x96, 0x66, 0x6b, 0x8e, 0xb2,
        0x95, 0xa3, 0xb9, 0xca, 0x55, 0x9e, 0xe6, 0x69, 0xbe, 0x16, 0x28, 0x5f,
        0x05, 0x5a, 0xa8, 0x45, 0x0a, 0xab, 0x50, 0x8b, 0xb5, 0x44, 0x4b, 0xb5,
        0x4c, 0xcb, 0xb5, 0x42, 0x2b, 0x55, 0xa4, 0x55, 0x5a, 0xad, 0x35, 0x5a,
        0xab, 0x75, 0x5a, 0xaf, 0x0d, 0xda, 0xa8, 0x4d, 0x2a, 0xd6, 0x66, 0x6d,
        0x51, 0x89, 0xb6, 0x6a, 0x9b, 0xb6, 0x6b, 0x87, 0x76, 0x6a, 0x97, 0x76,
        0x6b, 0x8f, 0xf6, 0x6a, 0x9f, 0xf6, 0xab, 0x54, 0x07, 0x74, 0x50, 0x65,
        0x3a, 0xa4, 0xc3, 0x3a, 0xa2, 0xa3, 0x3a, 0xa6, 0xe3, 0x2a, 0xd7, 0x09,
        0x9d, 0xd4, 0x29, 0x9d, 0xd6, 0x19, 0x9d, 0xd5, 0x39, 0x9d, 0xd7, 0x05,
        0x5d, 0xd4, 0x25, 0x5d, 0xd6, 0x15, 0x5d, 0xd5, 0x35, 0x5d, 0x57, 0x85,
        0x6e, 0xe8, 0xa6, 0x6e, 0xe9, 0xb6, 0xee, 0xe8, 0xae, 0xee, 0xe9, 0xbe,
        0x1e, 0xa8, 0x52, 0x0f, 0xf5, 0x48, 0x8f, 0x15, 0xd1, 0x13, 0x3d, 0x55,
        0x95, 0x9e, 0xe9, 0xb9, 0x5e, 0xa8, 0x5a, 0x2f, 0xf5, 0x4a, 0x35, 0xaa,
        0xd5, 0x6b, 0xbd, 0x51, 0x9d, 0xde, 0xea, 0x9d, 0xde, 0xeb, 0x83, 0x3e,
        0xea, 0x93, 0x3e, 0xeb, 0x8b, 0xbe, 0xea, 0x9b, 0xbe, 0xab, 0x5e, 0x3f,
        0xf4, 0x53, 0x0d, 0xfa, 0xa5, 0xdf, 0xfa, 0xa3, 0xbf, 0xfa, 0x17, 0x84,
        0x23, 0xff, 0x01
    };
    std::vector<std::uint8_t> bytes{'F', 'P', 'D', 'M', 2,
                                    fruityprime::net::NetConfig::ProtocolVersion};
    bytes.insert(bytes.end(), compressed.begin(), compressed.end());
    write_bytes(path, bytes);

    auto reader = fruityprime::demo::Reader::open(path);
    assert(reader.has_value());
    assert(!reader->had_deflate_error());
    assert(reader->decompressed_size() == 1'400);
    std::size_t records = 0;
    fruityprime::demo::Record last;
    while (const auto record = reader->read_next()) {
        last = *record;
        ++records;
    }
    assert(records == 200);
    assert(last.frame == 199);
    assert(last.data == std::vector<std::uint8_t>({199, 0, 0x5a, 0xa5}));
}

} // namespace

int main() {
    const auto path = temporary_path(".fpdemo");
    const auto truncated_path = temporary_path(".truncated.fpdemo");
    const auto invalid_path = temporary_path(".invalid.fpdemo");
    const auto dynamic_path = temporary_path(".dynamic.fpdemo");
    const auto recorder_path = temporary_path(".recorder.fpdemo");
    const auto playback_path = temporary_path(".playback.fpdemo");
    const auto library_path = temporary_path("_demos");
    try {
        {
            fruityprime::demo::Writer writer(path);
            writer.write_record(0, std::vector<std::uint8_t>{1, 2, 3});
            writer.write_record(3, std::vector<std::uint8_t>{'h', 'i'});
            writer.write_record(300, std::vector<std::uint8_t>(65'535, 0x5a));
            writer.write_record(250, std::vector<std::uint8_t>{0});
            writer.close();
        }
        check_reader(path);
        const auto info = fruityprime::demo::inspect(path);
        assert(info.has_value() && info->records == 4
               && info->first_frame == 0 && info->last_frame == 300
               && info->biggest_gap == 297 && info->snapshots == 0
               && info->packet_order == std::vector<std::uint8_t>({1, 'h', 0x5a, 0}));
        std::ostringstream report;
        fruityprime::demo::print(report, path, *info);
        assert(report.str().find("longest gap between records: 297")
               != std::string::npos);
        assert(report.str().find("format=FPDM version=2")
               == std::string::npos);

        auto bytes = read_bytes(path);
        assert(bytes.size() > fruityprime::demo::HeaderSize + 5);
        bytes.resize(bytes.size() - 5); // remove the final empty DEFLATE block
        write_bytes(truncated_path, bytes);
        auto truncated = fruityprime::demo::Reader::open(truncated_path);
        assert(truncated.has_value());
        assert(truncated->had_deflate_error());
        std::size_t records = 0;
        while (truncated->read_next().has_value()) {
            ++records;
        }
        assert(records == 4);

        write_bytes(invalid_path, std::vector<std::uint8_t>{'N', 'O', 'P'});
        assert(!fruityprime::demo::Reader::open(invalid_path).has_value());

        check_managed_style_deflate(dynamic_path);

        {
            fruityprime::demo::Recorder recorder;
            assert(recorder.start(recorder_path, 100));
            assert(!recorder.start(temporary_path(".second.fpdemo"), 100));
            recorder.record_bytes(100, std::vector<std::uint8_t>{'a'});
            recorder.record_packet(
                105, fruityprime::net::PacketType::Chat,
                std::vector<std::uint8_t>{'b', 'c'});
            recorder.record_bytes(99, std::vector<std::uint8_t>{'d'});
            recorder.stop();
            assert(!recorder.recording());
            auto recorded = fruityprime::demo::Reader::open(recorder_path);
            assert(recorded.has_value());
            const std::vector<std::uint8_t> chat_record{
                static_cast<std::uint8_t>(
                    fruityprime::net::PacketType::Chat), 'b', 'c'};
            const auto first = recorded->read_next();
            const auto second = recorded->read_next();
            const auto third = recorded->read_next();
            assert(first.has_value() && first->frame == 0
                   && first->data == std::vector<std::uint8_t>{'a'});
            assert(second.has_value() && second->frame == 5
                   && second->data == chat_record);
            assert(third.has_value() && third->frame == 5
                   && third->data == std::vector<std::uint8_t>{'d'});
            assert(!recorded->read_next().has_value());
        }
        const auto default_path = fruityprime::demo::Recorder::default_path(
            library_path.parent_path(), "Arena:/Test",
            std::chrono::system_clock::from_time_t(0));
        assert(default_path.filename().string().find("Arena__Test_") == 0);
        assert(default_path.extension() == fruityprime::demo::Extension);

        {
            fruityprime::net::MatchStatePacket match;
            match.room_key = "TEST ARENA";
            const auto match_payload = match.encode();
            fruityprime::net::SnapshotPacket snapshot;
            snapshot.header.player_count = 0;
            const auto snapshot_payload = snapshot.encode();
            std::vector<std::uint8_t> match_record{
                static_cast<std::uint8_t>(
                    fruityprime::net::PacketType::MatchState)};
            match_record.insert(match_record.end(), match_payload.begin(),
                                match_payload.end());
            std::vector<std::uint8_t> snapshot_record{
                static_cast<std::uint8_t>(
                    fruityprime::net::PacketType::Snapshot)};
            snapshot_record.insert(snapshot_record.end(),
                                   snapshot_payload.begin(),
                                   snapshot_payload.end());
            fruityprime::demo::Writer writer(playback_path);
            writer.write_record(2, match_record);
            writer.write_record(7, snapshot_record);
            writer.close();

            fruityprime::demo::Playback playback;
            fruityprime::net::MatchStatePacket first_match;
            std::string error;
            assert(playback.open(playback_path, first_match, error));
            assert(error.empty() && first_match.room_key == "TEST ARENA");
            assert(!playback.at_end());
            assert(playback.take_until(1).empty());
            const auto first_records = playback.take_until(2);
            assert(first_records.size() == 1
                   && first_records.front().frame == 2);
            assert(playback.take_until(2).empty());
            const auto second_records = playback.take_until(99);
            assert(second_records.size() == 1
                   && second_records.front().frame == 7);
            assert(playback.at_end());
            playback.stop();
            assert(!playback.active());
        }

        std::filesystem::create_directories(library_path);
        write_bytes(library_path / "Arena_2026-09-04_18-22-07.fpdemo",
                    {1, 2, 3});
        write_bytes(library_path / "Arena_2026-09-05_09-03-01.fpdemo",
                    {4, 5, 6, 7});
        write_bytes(library_path / "ignored.txt", {9});
        const auto recordings = fruityprime::demo::list_recordings(
            library_path);
        assert(recordings.size() == 2);
        assert(recordings[0].room == "Arena"
               && recordings[0].file_name()
                   == "Arena_2026-09-05_09-03-01.fpdemo");
        assert(recordings[1].room == "Arena"
               && recordings[1].bytes == 3);
        assert(fruityprime::demo::describe(recordings[0]).find("4 bytes")
               != std::string::npos);
        assert(fruityprime::demo::demos_directory(library_path.parent_path())
                   .filename() == "_demos");
    } catch (const std::exception& error) {
        std::cerr << "demo test exception: " << error.what() << '\n';
        std::error_code ignored;
        std::filesystem::remove(path, ignored);
        std::filesystem::remove(truncated_path, ignored);
        std::filesystem::remove(invalid_path, ignored);
        std::filesystem::remove(dynamic_path, ignored);
        std::filesystem::remove(recorder_path, ignored);
        std::filesystem::remove(playback_path, ignored);
        std::filesystem::remove_all(library_path, ignored);
        return 1;
    } catch (...) {
        std::cerr << "demo test exception: unknown\n";
        std::error_code ignored;
        std::filesystem::remove(path, ignored);
        std::filesystem::remove(truncated_path, ignored);
        std::filesystem::remove(invalid_path, ignored);
        return 1;
    }
    std::error_code ignored;
    std::filesystem::remove(path, ignored);
    std::filesystem::remove(truncated_path, ignored);
    std::filesystem::remove(invalid_path, ignored);
    std::filesystem::remove(dynamic_path, ignored);
    std::filesystem::remove(recorder_path, ignored);
    std::filesystem::remove(playback_path, ignored);
    std::filesystem::remove_all(library_path, ignored);
    return 0;
}
