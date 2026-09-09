#include "Utility/archive.hpp"
#include "Formats/ai_personality.hpp"
#include "Utility/binary_reader.hpp"
#include "Assets/compression.hpp"
#include "Utility/Compress.hpp"
#include "Formats/culling.hpp"
#include "Formats/effects.hpp"
#include "Formats/enemy_spawn.hpp"
#include "Assets/nds_rom.hpp"
#include "Formats/fixed.hpp"
#include "Formats/frontend.hpp"
#include "Formats/node_data.hpp"
#include "Formats/raw_formats.hpp"
#include "Read.hpp"

#include <algorithm>
#include <cassert>
#include <chrono>
#if defined(_WIN32)
#include <process.h>
#else
#include <unistd.h>
#endif
#include <filesystem>
#include <fstream>
#include <iostream>
#include <iterator>
#include <sstream>
#include <string>
#include <utility>
#include <vector>

namespace {

using fruityprime::core::BinaryReader;

std::string process_id_string() {
#if defined(_WIN32)
    return std::to_string(static_cast<unsigned long long>(_getpid()));
#else
    return std::to_string(static_cast<unsigned long long>(getpid()));
#endif
}

void put_u16_le(std::vector<std::uint8_t>& bytes, std::size_t offset,
                std::uint16_t value) {
    bytes[offset] = static_cast<std::uint8_t>(value);
    bytes[offset + 1] = static_cast<std::uint8_t>(value >> 8);
}

void put_u32_le(std::vector<std::uint8_t>& bytes, std::size_t offset,
                std::uint32_t value) {
    bytes[offset] = static_cast<std::uint8_t>(value);
    bytes[offset + 1] = static_cast<std::uint8_t>(value >> 8);
    bytes[offset + 2] = static_cast<std::uint8_t>(value >> 16);
    bytes[offset + 3] = static_cast<std::uint8_t>(value >> 24);
}

void put_text(std::vector<std::uint8_t>& bytes, std::size_t offset,
              std::string_view text) {
    for (std::size_t i = 0; i < text.size(); ++i) {
        bytes[offset + i] = static_cast<std::uint8_t>(text[i]);
    }
}

void write_bytes(const std::filesystem::path& path,
                 const std::vector<std::uint8_t>& bytes) {
    std::ofstream output(path, std::ios::binary | std::ios::trunc);
    assert(output);
    output.write(reinterpret_cast<const char*>(bytes.data()),
                 static_cast<std::streamsize>(bytes.size()));
    assert(output);
}

[[nodiscard]] std::vector<std::uint8_t> read_bytes(
    const std::filesystem::path& path) {
    std::ifstream input(path, std::ios::binary);
    assert(input);
    return {std::istreambuf_iterator<char>(input),
            std::istreambuf_iterator<char>()};
}

template <typename Function>
void assert_throws(Function&& function) {
    bool thrown = false;
    try {
        function();
    } catch (const std::exception&) {
        thrown = true;
    }
    assert(thrown);
}

void test_binary_reader() {
    const std::vector<std::uint8_t> bytes{
        0x7f, 0x34, 0x12, 0x78, 0x56, 0x34, 0x12,
        0x00, 0x00, 0x80, 0x3f
    };
    BinaryReader reader(bytes);
    assert(reader.read_u8() == 0x7f);
    assert(reader.read_u16_le() == 0x1234);
    assert(reader.read_u32_le() == 0x12345678);
    assert(reader.read_f32_le() == 1.0F);
    assert(reader.remaining() == 0);
    assert_throws([&reader]() { static_cast<void>(reader.read_u8()); });

    const fruityprime::formats::Fixed fixed{4096};
    assert(fixed.to_float() == 1.0F);
    assert(fruityprime::formats::Fixed::to_int(1.5F) == 6144);

    const std::vector<std::uint8_t> reverse_fixture{
        'd', 'a', 't', 'a', 0, 0, 0, 0
    };
    assert(fruityprime::compression::lz_backward_decompress(reverse_fixture)
           == std::vector<std::uint8_t>({'d', 'a', 't', 'a'}));

    const std::vector<std::uint8_t> repeated{
        'A', 'B', 'C', 'A', 'B', 'C', 'A', 'B', 'C', 'A', 'B', 'C',
        0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 'x', 'y', 'z'
    };
    const auto encoded = fruityprime::compression::lz10_compress(repeated);
    assert(encoded.size() < repeated.size() + 8);
    assert(fruityprime::compression::lz10_decompress(encoded) == repeated);

    assert(fruityprime::LZ10::MagicByte == 0x10);
    int displacement = -1;
    const int occurrence = fruityprime::LZUtil::GetOccurrenceLength(
        repeated.data() + 3, 9, repeated.data(), 3, displacement);
    assert(occurrence == 9 && displacement == 3);
    displacement = -1;
    assert(fruityprime::LZUtil::GetOccurrenceLength(
               repeated.data() + 3, 9, repeated.data(), 3, displacement, 3)
           == 0);
    assert(displacement == 0);
    assert(fruityprime::LZ10::Compress(repeated) == encoded);
    assert(fruityprime::LZ10::Decompress(encoded) == repeated);

    std::istringstream compress_input(
        std::string(reinterpret_cast<const char*>(repeated.data()),
                    repeated.size()),
        std::ios::binary);
    std::ostringstream compress_output(std::ios::binary);
    assert(fruityprime::LZ10::Compress(
               compress_input, static_cast<std::int64_t>(repeated.size()),
               compress_output)
           == static_cast<int>(encoded.size()));
    assert(compress_output.str()
           == std::string(reinterpret_cast<const char*>(encoded.data()),
                          encoded.size()));
    assert(fruityprime::LZBackward::ToNDSu32(
               std::array<std::uint8_t, 4>{0x78, 0x56, 0x34, 0x12}, 0)
           == 0x12345678U);

    const std::array<std::uint8_t, 5> short_overlay{1, 2, 3, 4, 5};
    const auto short_overlay_encoded =
        fruityprime::compression::lz_backward_compress(short_overlay);
    assert(short_overlay_encoded.size() == short_overlay.size() + 4);
    assert(std::equal(short_overlay.begin(), short_overlay.end(),
                      short_overlay_encoded.begin()));

    const std::array<std::uint8_t, 256> repeated_overlay = [] {
        std::array<std::uint8_t, 256> value{};
        for (std::size_t i = 0; i < value.size(); ++i) {
            value[i] = static_cast<std::uint8_t>(i & 3U);
        }
        return value;
    }();
    const auto overlay_encoded =
        fruityprime::compression::lz_backward_compress(repeated_overlay);
    assert(overlay_encoded.size() < repeated_overlay.size());
    assert((overlay_encoded.size() & 3U) == 0);
    assert(overlay_encoded[overlay_encoded.size() - 5] == 9);
    assert(overlay_encoded[overlay_encoded.size() - 4]
           == static_cast<std::uint8_t>(repeated_overlay.size()
                                         - overlay_encoded.size()));
    assert(overlay_encoded[overlay_encoded.size() - 3] == 0);
    assert(overlay_encoded[overlay_encoded.size() - 2] == 0);
    assert(overlay_encoded.back() == 0);
    assert_throws([]() {
        static_cast<void>(fruityprime::compression::lz10_compress({}));
    });

    // RawFormats.cs layout contracts are part of the parser surface.  These
    // assertions make accidental enum-width or padding changes fail in the
    // native build instead of corrupting a later animation/entity read.
    assert(fruityprime::raw::Sizes::Header == 100);
    assert(fruityprime::raw::Sizes::Material == 132);
    assert(fruityprime::raw::Sizes::Node == 240);
    assert(fruityprime::raw::layout().size() == 24);
    assert(fruityprime::raw::validate_layout());
    assert(sizeof(fruityprime::raw::RawCollisionVolume) == 64);
    assert(sizeof(fruityprime::raw::FhRawCollisionVolume) == 64);
    assert(static_cast<std::uint32_t>(
               fruityprime::formats::Message::SetTriggerState) == 42);
    assert(static_cast<std::uint16_t>(
               fruityprime::formats::WeaponUnlockBits::OmegaCannon) == 0x100);

    std::vector<std::uint8_t> records(32, 0);
    put_u16_le(records, 8, 0x1234);
    put_u16_le(records, 10, 0x5678);
    put_u16_le(records, 12, 0x9abc);
    put_u16_le(records, 14, 0xdef0);
    put_u32_le(records, 16, 0);
    const auto first = fruityprime::read::read_struct<fruityprime::raw::RawMesh>(
        records, 8);
    assert(first.material_id == 0x1234 && first.dlist_id == 0x5678);
    const auto offset_records = fruityprime::read::do_offsets<
        fruityprime::raw::RawMesh>(records, 8, 2);
    assert(offset_records.size() == 2
           && offset_records[1].material_id == 0x9abc);
    const auto list = fruityprime::read::do_list_null_end(records, 16);
    assert(list.empty());

    const std::vector<std::uint8_t> scalar_bytes{
        0x78, 0x56, 0x34, 0x12, 0xfe, 0xff, 0xff, 0xff, 0xcd, 0xab,
        'o',  'n',  'e',  0,    0,    't',  'w',  'o',  0
    };
    std::size_t cursor = 0;
    assert(fruityprime::read::span_read_u32(scalar_bytes, cursor)
           == 0x12345678U);
    assert(fruityprime::read::span_read_i32(scalar_bytes, cursor) == -2);
    assert(fruityprime::read::span_read_u16(scalar_bytes, cursor)
           == 0xabcdU);
    assert(cursor == 10);
    assert(fruityprime::read::span_read_u32_at(scalar_bytes, 0)
           == 0x12345678U);
    assert(fruityprime::read::span_read_u16_at(scalar_bytes, 8) == 0xabcdU);

    const std::vector<std::uint8_t> string_bytes{
        'A', 0x80, 0, 'T', 0x80, 0, 'o', 'n', 'e', 0, 0, 't', 'w', 'o', 0
    };
    assert(fruityprime::read::read_string(string_bytes, 0, 3)
           == std::string("A?"));
    assert(fruityprime::read::read_string_table(string_bytes, 3, 3)
           == std::string({'T', static_cast<char>(0x80)}));
    const auto strings = fruityprime::read::read_strings(string_bytes, 6, 3);
    assert(strings.size() == 3 && strings[0] == "one" && strings[1].empty()
           && strings[2] == "two");
    assert(fruityprime::read::do_offset<fruityprime::raw::RawMesh>(records, 8)
               .material_id
           == 0x1234);
    assert_throws([&records]() {
        static_cast<void>(fruityprime::read::do_offset<fruityprime::raw::RawMesh>(
            records, 0));
    });
    assert(fruityprime::read::do_list_null_end(records, 0).empty());
    assert_throws([&scalar_bytes]() {
        static_cast<void>(fruityprime::read::span_read_u32_at(scalar_bytes, 17));
    });
    assert_throws([&string_bytes]() {
        static_cast<void>(fruityprime::read::read_strings(string_bytes, 6, 4));
    });

    std::vector<std::uint8_t> kanji_11(8 + 2 * 11, 0);
    put_u32_le(kanji_11, 0, 1);
    put_u16_le(kanji_11, 4, 16);
    put_u16_le(kanji_11, 6, 11);
    kanji_11[8] = 0x80; // x=0, y=0 -> tile row 3, low nibble
    kanji_11[9] = 0x01; // x=15, y=0 -> tile row 3, high nibble
    const auto font_11 = fruityprime::read::read_kanji_font(kanji_11);
    assert(font_11.count == 1 && font_11.width == 16
           && font_11.height == 11 && font_11.character_data.size() == 128);
    assert(font_11.character_data[3 * 4] == 3);
    assert(font_11.character_data[11 * 4 + 3] == 0x30);

    std::vector<std::uint8_t> kanji_16(8 + 2 * 16, 0);
    put_u32_le(kanji_16, 0, 1);
    put_u16_le(kanji_16, 4, 16);
    put_u16_le(kanji_16, 6, 16);
    kanji_16[8] = 0x80; // x=0, y=0 remains at the first tile row
    const auto font_16 = fruityprime::read::read_kanji_font(kanji_16);
    assert(font_16.character_data.size() == 128
           && font_16.character_data[0] == 3);
    assert_throws([&kanji_11]() {
        auto invalid = kanji_11;
        put_u16_le(invalid, 4, 8);
        static_cast<void>(fruityprime::read::read_kanji_font(invalid));
    });
    assert_throws([&kanji_11]() {
        auto invalid = kanji_11;
        invalid.pop_back();
        static_cast<void>(fruityprime::read::read_kanji_font(invalid));
    });
}

void test_frontend() {
    std::vector<std::uint8_t> bytes(192, 0);
    put_text(bytes, 0, "MARM");
    put_u16_le(bytes, 4, 0x1234);
    bytes[6] = 6;
    bytes[7] = 7;
    put_u32_le(bytes, 8, 20);
    put_u32_le(bytes, 12, 172);
    put_u32_le(bytes, 16, 0);

    put_u32_le(bytes, 20, 64);
    put_u32_le(bytes, 24, 0);
    put_u32_le(bytes, 64, 0x1010);
    put_u32_le(bytes, 72, 100);
    bytes[64 + 28] = 3;

    put_u32_le(bytes, 100, 112);
    put_u32_le(bytes, 104, 0);
    put_u32_le(bytes, 112, 0x2222);
    put_u32_le(bytes, 132, 0x3333);
    bytes[112 + 52] = 9;

    put_u32_le(bytes, 172, 180);
    put_u32_le(bytes, 176, 0);
    put_u32_le(bytes, 180, 0x4444);
    put_u32_le(bytes, 184, 0x5555);

    const auto parsed = fruityprime::frontend::File::parse(bytes);
    assert(parsed.is_marm());
    assert(parsed.header().field4 == 0x1234);
    assert(parsed.menu1().size() == 1);
    assert(parsed.menu1()[0].value.field0 == 0x1010);
    assert(parsed.menu1()[0].value.index == 3);
    assert(parsed.menu1()[0].children.size() == 1);
    assert(parsed.menu1()[0].children[0].field14 == 0x3333);
    assert(parsed.menu1()[0].children[0].field34 == 9);
    assert(parsed.menu2().size() == 1);
    assert(parsed.menu2()[0].field4 == 0x5555);
    assert_throws([&bytes]() {
        bytes[0] = 'X';
        static_cast<void>(fruityprime::frontend::File::parse(bytes));
    });
}

void test_effect_format() {
    std::vector<std::uint8_t> bytes(256, 0);
    put_u32_le(bytes, 0, 0xcccccccc);
    put_u32_le(bytes, 4, 1);   // one function
    put_u32_le(bytes, 8, 32);  // function offset table
    put_u32_le(bytes, 12, 1);  // one list2 entry
    put_u32_le(bytes, 16, 40); // list2 table
    put_u32_le(bytes, 20, 1);  // one element
    put_u32_le(bytes, 24, 44); // element offset table

    put_u32_le(bytes, 32, 64);
    put_u32_le(bytes, 40, 88);
    put_u32_le(bytes, 44, 100);

    put_u32_le(bytes, 56, 4096);
    put_u32_le(bytes, 60, static_cast<std::uint32_t>(-4096));
    put_u32_le(bytes, 64, 4);  // function id
    put_u32_le(bytes, 68, 56); // parameter table before function
    put_u32_le(bytes, 88, 0xabcdef01);

    put_text(bytes, 100, "sparks");
    put_text(bytes, 132, "fxmodel");
    put_u32_le(bytes, 164, 1);   // particle count
    put_u32_le(bytes, 168, 220); // particle offset table
    put_u32_le(bytes, 172, 0x00000004); // UseMesh
    put_u32_le(bytes, 176, 4096);
    put_u32_le(bytes, 180, 0);
    put_u32_le(bytes, 184, static_cast<std::uint32_t>(-4096));
    put_u32_le(bytes, 188, 17);
    put_u32_le(bytes, 192, 2 * 4096);
    put_u32_le(bytes, 196, 4096);
    put_u32_le(bytes, 200, 2048);
    put_u32_le(bytes, 204, 6);
    put_u32_le(bytes, 208, 1);    // one action pair
    put_u32_le(bytes, 212, 224);  // action table
    put_u32_le(bytes, 220, 240);
    put_u32_le(bytes, 224, 14);   // IncreaseParticleAmount
    put_u32_le(bytes, 228, 64);
    put_text(bytes, 240, "particle");

    const auto parsed = fruityprime::effects::File::parse(
        bytes, 7, "sparks_PS.bin");
    assert(parsed.id() == 7 && parsed.name() == "sparks_PS.bin");
    assert(parsed.header().field0 == 0xcccccccc);
    assert(parsed.functions().size() == 1);
    assert(parsed.functions().at(64).id == 4);
    assert(parsed.functions().at(64).parameters.size() == 2);
    assert(parsed.functions().at(64).parameters[0] == 4096);
    assert(parsed.functions().at(64).parameters[1] == -4096);
    assert(parsed.list2().size() == 1 && parsed.list2()[0] == 88);
    assert(parsed.elements().size() == 1);
    const auto& element = parsed.elements()[0];
    assert(element.name == "sparks" && element.model_name == "fxmodel");
    assert(element.particle_names.size() == 1
           && element.particle_names[0] == "particle");
    assert(static_cast<std::uint32_t>(element.flags) == 4);
    assert(element.acceleration.x == 1.0F && element.acceleration.z == -1.0F);
    assert(element.child_effect_id == 17 && element.draw_type == 6);
    assert(element.actions.at(14) == 64);
    assert_throws([&bytes]() {
        auto invalid = bytes;
        put_u32_le(invalid, 68, 252);
        static_cast<void>(fruityprime::effects::File::parse(invalid));
    });
}

void test_enemy_spawn() {
    using fruityprime::formats::EnemyType;
    using fruityprime::formats::FhEnemyType;
    using fruityprime::formats::ItemType;
    using fruityprime::formats::Message;

    assert(fruityprime::enemy_spawn::spawner_type(EnemyType::Zoomer)
           == fruityprime::enemy_spawn::SpawnerType::Common);
    assert(fruityprime::enemy_spawn::spawner_type(EnemyType::WarWasp)
           == fruityprime::enemy_spawn::SpawnerType::WarWasp);
    assert(fruityprime::enemy_spawn::spawner_type(EnemyType::Shriekbat)
           == fruityprime::enemy_spawn::SpawnerType::Shriekbat);
    assert(fruityprime::enemy_spawn::spawner_type(EnemyType::Gorea2)
           == fruityprime::enemy_spawn::SpawnerType::Gorea2);
    assert(fruityprime::enemy_spawn::spawner_type(EnemyType::Unknown7)
           == fruityprime::enemy_spawn::SpawnerType::Unknown);

    std::vector<std::uint8_t> bytes(fruityprime::enemy_spawn::Data::Size, 0);
    bytes[40] = static_cast<std::uint8_t>(EnemyType::BarbedWarWasp);
    put_u32_le(bytes, 44, 0x11223344); // subtype
    put_u32_le(bytes, 48, 0x55667788); // version
    // The barbed-wasp union starts after subtype/version.  Use spheres for
    // the three volumes so their fixed-point positions/radii are observable.
    for (std::size_t i = 0; i < 3; ++i) {
        const std::size_t volume = 52 + i * 64;
        put_u32_le(bytes, volume, 2);
        put_u32_le(bytes, volume + 4, static_cast<std::uint32_t>(
            (i + 1) * 4096));
        put_u32_le(bytes, volume + 16, 2 * 4096);
    }
    put_u32_le(bytes, 52 + 192 + 15 * 12, 0x00010000);
    bytes[52 + 384] = 3;
    put_u32_le(bytes, 52 + 388, 9);

    put_u16_le(bytes, 444, static_cast<std::uint16_t>(-7));
    bytes[446] = 4;
    bytes[447] = 2;
    bytes[448] = 1;
    bytes[449] = 1;
    bytes[450] = 0;
    bytes[451] = 25;
    put_u16_le(bytes, 452, 300);
    put_u16_le(bytes, 454, 40);
    put_u16_le(bytes, 456, 12);
    put_u32_le(bytes, 460, 8 * 4096);
    put_u32_le(bytes, 464, 4 * 4096);
    put_text(bytes, 468, "spawn_node");
    put_u16_le(bytes, 484, static_cast<std::uint16_t>(-2));
    put_u32_le(bytes, 488, static_cast<std::uint32_t>(Message::Activate));
    put_u16_le(bytes, 492, static_cast<std::uint16_t>(-3));
    put_u32_le(bytes, 496, static_cast<std::uint32_t>(Message::Destroyed));
    put_u16_le(bytes, 500, static_cast<std::uint16_t>(-4));
    put_u32_le(bytes, 504, static_cast<std::uint32_t>(Message::Death));
    put_u32_le(bytes, 508, static_cast<std::uint32_t>(ItemType::MissileBig));

    const auto parsed = fruityprime::enemy_spawn::decode(bytes);
    assert(parsed.enemy_type == EnemyType::BarbedWarWasp);
    assert(parsed.fields.spawner_type
           == fruityprime::enemy_spawn::SpawnerType::BarbedWarWasp);
    assert(parsed.fields.enemy_subtype == 0x11223344);
    assert(parsed.fields.enemy_version == 0x55667788);
    assert(parsed.fields.war_wasp.position_count == 3);
    assert(parsed.fields.war_wasp.movement_type == 9);
    assert(parsed.fields.war_wasp.movement_vectors[15].x.to_float() == 16.0F);
    assert(parsed.fields.war_wasp.volumes[0].type
           == fruityprime::formats::VolumeType::Sphere);
    assert(parsed.fields.war_wasp.volumes[0].data.sphere.radius.to_float()
           == 2.0F);
    assert(parsed.linked_entity_id == -7);
    assert(parsed.spawn_total == 4 && parsed.spawn_limit == 2
           && parsed.spawn_count == 1 && parsed.active);
    assert(parsed.active_distance.to_float() == 8.0F);
    assert(parsed.enemy_active_distance.to_float() == 4.0F);
    assert(parsed.node_name == "spawn_node");
    assert(parsed.messages[0].target_id == -2
           && parsed.messages[0].message == Message::Activate);
    assert(parsed.messages[2].target_id == -4
           && parsed.messages[2].message == Message::Death);
    assert(parsed.item_type == ItemType::MissileBig);

    std::vector<std::uint8_t> first_hunt(
        fruityprime::enemy_spawn::FirstHuntData::Size, 0);
    put_u32_le(first_hunt, 40, 0);
    put_u32_le(first_hunt, 104, 1);
    put_u32_le(first_hunt, 168, 2);
    put_u32_le(first_hunt, 172, 3 * 4096);
    put_u32_le(first_hunt, 232, static_cast<std::uint32_t>(FhEnemyType::Metroid));
    first_hunt[236] = 5;
    first_hunt[237] = 3;
    first_hunt[238] = 2;
    put_u16_le(first_hunt, 240, 60);
    put_u16_le(first_hunt, 242, 18);
    put_text(first_hunt, 244, "fh_spawn");
    put_u16_le(first_hunt, 260, static_cast<std::uint16_t>(-9));
    put_u32_le(first_hunt, 264, 5);

    const auto parsed_first_hunt =
        fruityprime::enemy_spawn::decode_first_hunt(first_hunt);
    assert(parsed_first_hunt.box.type == fruityprime::formats::FhVolumeType::Sphere);
    assert(parsed_first_hunt.cylinder.type
           == fruityprime::formats::FhVolumeType::Box);
    assert(parsed_first_hunt.sphere.type
           == fruityprime::formats::FhVolumeType::Cylinder);
    assert(parsed_first_hunt.sphere.data.cylinder.position.x.to_float() == 3.0F);
    assert(parsed_first_hunt.enemy_type == FhEnemyType::Metroid);
    assert(parsed_first_hunt.spawn_total == 5
           && parsed_first_hunt.spawn_limit == 3
           && parsed_first_hunt.spawn_count == 2);
    assert(parsed_first_hunt.node_name == "fh_spawn");
    assert(parsed_first_hunt.parent_id == -9);
    assert(parsed_first_hunt.empty_message
           == fruityprime::formats::FhMessage::Activate);

    assert_throws([]() {
        static_cast<void>(fruityprime::enemy_spawn::decode(
            std::vector<std::uint8_t>(100, 0)));
    });
    assert_throws([]() {
        auto invalid = std::vector<std::uint8_t>(
            fruityprime::enemy_spawn::FirstHuntData::Size, 0);
        put_u32_le(invalid, 40, 99);
        static_cast<void>(fruityprime::enemy_spawn::decode_first_hunt(invalid));
    });
}

void test_ai_personality() {
    // Root -> one child, one switch row, one precondition, and two function
    // lists.  All offsets are deliberately non-zero because Read.DoOffsets
    // treats offset zero as the null table used by the cartridge format.
    std::vector<std::uint8_t> bytes(196, 0);
    put_u32_le(bytes, 36, 24);   // root function 4
    put_u32_le(bytes, 40, 1);    // one child
    put_u32_le(bytes, 44, 72);   // child table
    put_u32_le(bytes, 48, 1);    // one data2 row
    put_u32_le(bytes, 52, 108);  // data2 table
    put_u32_le(bytes, 56, 1);    // init function
    put_u32_le(bytes, 60, 132);  // init table
    put_u32_le(bytes, 64, 1);    // process function
    put_u32_le(bytes, 68, 136);  // process table

    put_u32_le(bytes, 72, 30);   // child function 4
    put_u32_le(bytes, 108, 210); // data5/func3 type
    put_u32_le(bytes, 112, 1);   // one precondition
    put_u32_le(bytes, 116, 156); // data4 table
    put_u32_le(bytes, 120, 0);   // select root child 0
    put_u32_le(bytes, 124, 7);   // weight
    put_u32_le(bytes, 128, 172); // two int parameters
    put_u32_le(bytes, 132, 5);
    put_u32_le(bytes, 136, 6);
    put_u32_le(bytes, 156, 211); // precondition function
    put_u32_le(bytes, 160, 180); // one int parameter
    put_u32_le(bytes, 172, 100);
    put_u32_le(bytes, 176, 200);
    put_u32_le(bytes, 180, 300);

    const auto parsed = fruityprime::ai::File::parse(bytes, 36);
    assert(parsed.root != nullptr);
    assert(parsed.root->label == "Root");
    assert(parsed.root->func24_id == 24);
    assert(parsed.root->data1.size() == 1);
    assert(parsed.root->data1[0]->label == "A");
    assert(parsed.root->data1[0]->parent.lock() == parsed.root);
    assert(parsed.root->data2.size() == 1);
    assert(parsed.root->data2[0].func3_id == 210);
    assert(parsed.root->data2[0].weight == 7);
    assert(parsed.root->data2[0].parameters.param1 == 100);
    assert(parsed.root->data2[0].parameters.param2 == 200);
    assert(parsed.root->data2[0].data4.size() == 1);
    assert(parsed.root->data2[0].data4[0].func3_id == 211);
    assert(parsed.root->data2[0].data4[0].parameters.param1 == 300);
    assert(parsed.root->data3a[0] == 5 && parsed.root->data3b[0] == 6);
    assert(parsed.node_count == 2);

    assert(fruityprime::ai::select_offset(2, 2, 7) == 32932);
    assert(fruityprime::ai::select_offset(2, 1, 1) == 33196);
    assert(fruityprime::ai::select_offset(5, 0, 0) == 45696);
    assert(fruityprime::ai::select_offset(14, 0, 0) == 45220);
    assert_throws([&bytes]() {
        static_cast<void>(fruityprime::ai::File::parse(bytes, 0));
    });
}

void test_node_data() {
    // Version 6: one set, one sub-list, two nodes, and one shared u16 value
    // table.  The offsets deliberately exercise the index conversion used by
    // NodeData3::index1/index2.
    std::vector<std::uint8_t> bytes(114, 0);
    put_u16_le(bytes, 0, 6);
    put_u16_le(bytes, 2, 1); // data count
    put_u32_le(bytes, 4, 14); // set-index offset
    put_u32_le(bytes, 8, 16); // struct1 offset
    put_u16_le(bytes, 12, 1); // set-index count
    put_u16_le(bytes, 14, 0);
    put_u32_le(bytes, 16, 24); // struct1 -> struct2
    put_u16_le(bytes, 20, 1);
    put_u16_le(bytes, 22, 0x5c);
    put_u32_le(bytes, 24, 32); // struct2 -> struct3
    put_u16_le(bytes, 28, 2);
    put_u16_le(bytes, 30, 0x5c);

    put_u16_le(bytes, 32, 0); // navigation
    put_u16_le(bytes, 34, 7);
    put_u16_le(bytes, 36, 9);
    put_u16_le(bytes, 38, 2);
    put_u32_le(bytes, 40, 4096);
    put_u32_le(bytes, 44, 8192);
    put_u32_le(bytes, 48, 12288);
    put_u32_le(bytes, 52, 5 * 4096);
    put_u32_le(bytes, 56, 104);
    put_u32_le(bytes, 60, 108);
    put_u32_le(bytes, 64, 0);

    put_u16_le(bytes, 68, 5); // hazard
    put_u16_le(bytes, 70, 8);
    put_u16_le(bytes, 72, 10);
    put_u16_le(bytes, 74, 1);
    put_u32_le(bytes, 76, 10 * 4096);
    put_u32_le(bytes, 80, 0);
    put_u32_le(bytes, 84, 0);
    put_u32_le(bytes, 88, 1 * 4096);
    put_u32_le(bytes, 92, 110);
    put_u32_le(bytes, 96, 112);
    put_u32_le(bytes, 100, 0);

    put_u16_le(bytes, 104, 11);
    put_u16_le(bytes, 106, 22);
    put_u16_le(bytes, 108, 33);
    put_u16_le(bytes, 110, 44);
    put_u16_le(bytes, 112, 55);

    const auto node_data = fruityprime::node::NodeData::from_bytes(bytes);
    assert(node_data.header().version == 6);
    assert(node_data.set_indices().size() == 1);
    assert(node_data.simple());
    assert(node_data.data().size() == 1);
    assert(node_data.data()[0].size() == 1);
    const auto& nodes = node_data.data()[0][0];
    assert(nodes.size() == 2);
    assert(nodes[0].id == 7);
    assert(nodes[0].index1 == 0 && nodes[0].index2 == 2);
    assert(nodes[0].values().size() == 5 && nodes[0].values()[2] == 33);
    assert(nodes[0].position.x == 1.0F && nodes[0].position.z == 3.0F);
    assert(nodes[0].transform.m41 == 1.0F);
    assert(nodes[0].color.x == 1.0F && nodes[0].color.y == 0.0F);
    assert(nodes[1].node_type == fruityprime::node::NodeType::Hazard);
    assert(nodes[1].color.x == 1.0F && nodes[1].color.y == 1.0F);
    assert(node_data.closest_node({1.25F, 2.0F, 3.0F})->id == 7);
    assert(node_data.closest_node({100.0F, 100.0F, 100.0F}, true) == nullptr);

    std::vector<std::uint8_t> first_hunt(28, 0);
    put_u16_le(first_hunt, 0, 0);
    put_u16_le(first_hunt, 2, 1);
    put_u16_le(first_hunt, 4, 3);
    put_u16_le(first_hunt, 6, 4);
    put_u32_le(first_hunt, 8, static_cast<std::uint32_t>(-4096));
    put_u32_le(first_hunt, 12, 2048);
    put_u32_le(first_hunt, 16, 8192);
    put_u32_le(first_hunt, 20, 0);
    put_u32_le(first_hunt, 24, 0);
    const auto fh_data = fruityprime::node::NodeData::from_bytes(first_hunt,
                                                                   true);
    assert(fh_data.simple() && fh_data.data()[0][0].size() == 1);
    assert(fh_data.data()[0][0][0].position.x == -1.0F);
    assert(fh_data.data()[0][0][0].position.y == 0.5F);

    auto invalid = bytes;
    put_u32_le(invalid, 56, 103); // an unaligned Offset1
    assert_throws([&invalid]() {
        static_cast<void>(fruityprime::node::NodeData::from_bytes(invalid));
    });
}

void test_culling_contract() {
    const auto none = fruityprime::culling::NodeRef::none();
    assert(none.part_index == -1 && none.node_index == -1
           && none.model_index == -1);
    const fruityprime::culling::NodeRef first{"room-a", 1, 2, 3};
    const fruityprime::culling::NodeRef same_indices{"room-b", 1, 2, 3};
    const fruityprime::culling::NodeRef different{"room-a", 1, 2, 4};
    assert(first == same_indices);
    assert(first != different);

    fruityprime::culling::RoomFrustumItem item;
    assert(item.node_ref == none);
    assert(item.info.planes.size() == 10);
    item.info.count = 1;
    item.info.planes[0].plane = {0.0F, 1.0F, 0.0F, 2.0F};
    assert(item.info.planes[0].plane.w == 2.0F);
}

void test_archive(const std::filesystem::path& root) {
    const auto first = root / "first.bin";
    const auto second = root / "second.bin";
    write_bytes(first, {1, 2, 3, 4, 5});
    write_bytes(second, std::vector<std::uint8_t>(40, 0xa5));

    const auto archive_path = root / "sound.arc";
    fruityprime::archive::Archive::create(archive_path, {first, second});
    const auto archive = fruityprime::archive::Archive::read_file(archive_path);
    assert(archive.entries().size() == 2);
    assert(archive.entries()[0].filename == "first.bin");
    assert(archive.entries()[0].target_size == 5);
    assert(archive.entries()[0].padded_size == 5);
    assert(archive.entries()[1].padded_size == 64);
    assert(archive.file(0) == std::vector<std::uint8_t>({1, 2, 3, 4, 5}));

    // Archives in the cartridge are often wrapped in LZ-0x10. Encode this
    // fixture as literals so the test exercises the decoder without needing
    // a second compression implementation in the test itself.
    const auto raw_archive = read_bytes(archive_path);
    std::vector<std::uint8_t> compressed{
        0x10,
        static_cast<std::uint8_t>(raw_archive.size()),
        static_cast<std::uint8_t>(raw_archive.size() >> 8),
        static_cast<std::uint8_t>(raw_archive.size() >> 16)
    };
    for (std::size_t offset = 0; offset < raw_archive.size();) {
        compressed.push_back(0);
        const std::size_t count = std::min<std::size_t>(8,
            raw_archive.size() - offset);
        compressed.insert(compressed.end(), raw_archive.begin()
            + static_cast<std::ptrdiff_t>(offset), raw_archive.begin()
            + static_cast<std::ptrdiff_t>(offset + count));
        offset += count;
    }
    const auto compressed_archive_path = root / "sound-compressed.arc";
    write_bytes(compressed_archive_path, compressed);
    const auto decoded_archive = fruityprime::archive::Archive::read_file(
        compressed_archive_path);
    assert(decoded_archive.entries().size() == 2);
    assert(decoded_archive.file(1).size() == 40);

    const auto extracted = root / "extracted";
    assert(archive.extract(extracted) == 2);
    assert(read_bytes(extracted / "first.bin")
           == std::vector<std::uint8_t>({1, 2, 3, 4, 5}));
    assert(read_bytes(extracted / "second.bin").size() == 40);

    const auto convenience_output = root / "convenience-extracted";
    const auto convenience = fruityprime::read::extract_archive(
        compressed_archive_path, convenience_output);
    assert(convenience.output_directory == convenience_output
           && convenience.files_written == 2);
    assert(read_bytes(convenience_output / "first.bin")
           == std::vector<std::uint8_t>({1, 2, 3, 4, 5}));

    auto invalid = read_bytes(archive_path);
    invalid[0] = 'X';
    assert_throws([&invalid]() {
        static_cast<void>(fruityprime::archive::Archive::parse(invalid));
    });
}

void test_nds_rom(const std::filesystem::path& root) {
    std::vector<std::uint8_t> bytes(0x1a0, 0);
    put_text(bytes, 0x00, "SYNTHETIC");
    put_text(bytes, 0x0c, "AMHE");
    put_text(bytes, 0x10, "01");
    bytes[0x1e] = 1;

    constexpr std::uint32_t fnt_offset = 0x100;
    constexpr std::uint32_t fnt_size = 0x24;
    constexpr std::uint32_t fat_offset = 0x140;
    constexpr std::uint32_t fat_size = 16;
    put_u32_le(bytes, 0x40, fnt_offset);
    put_u32_le(bytes, 0x44, fnt_size);
    put_u32_le(bytes, 0x48, fat_offset);
    put_u32_le(bytes, 0x4c, fat_size);

    // Two FNT directory records: root and one child directory.
    put_u32_le(bytes, 0x100, 0x10);
    put_u16_le(bytes, 0x104, 0);
    put_u16_le(bytes, 0x106, 2);
    put_u32_le(bytes, 0x108, 0x1d);
    put_u16_le(bytes, 0x10c, 1);
    put_u16_le(bytes, 0x10e, 0xf001);

    // Root: a.bin, then the subdirectory named "sub".
    bytes[0x110] = 5;
    put_text(bytes, 0x111, "a.bin");
    bytes[0x116] = 0x83;
    put_text(bytes, 0x117, "sub");
    put_u16_le(bytes, 0x11a, 0xf001);
    bytes[0x11c] = 0;
    // Child: b.bin.
    bytes[0x11d] = 5;
    put_text(bytes, 0x11e, "b.bin");
    bytes[0x123] = 0;

    put_u32_le(bytes, 0x140, 0x180);
    put_u32_le(bytes, 0x144, 0x185);
    put_u32_le(bytes, 0x148, 0x190);
    put_u32_le(bytes, 0x14c, 0x195);
    put_text(bytes, 0x180, "hello");
    put_text(bytes, 0x190, "world");

    const auto rom = fruityprime::nds::Rom::from_bytes(std::move(bytes));
    assert(rom.header().title == "SYNTHETIC");
    assert(rom.header().game_code == "AMHE");
    assert(rom.files().size() == 2);
    assert(rom.files()[0].path == "a.bin");
    assert(rom.files()[1].path == "sub/b.bin");
    assert(rom.file("a.bin") == std::vector<std::uint8_t>({'h', 'e', 'l', 'l', 'o'}));
    assert(rom.file("sub/b.bin") == std::vector<std::uint8_t>({'w', 'o', 'r', 'l', 'd'}));

    const auto extracted = root / "rom-extracted";
    assert(rom.extract(extracted) == 2);
    assert(read_bytes(extracted / "sub" / "b.bin")
           == std::vector<std::uint8_t>({'w', 'o', 'r', 'l', 'd'}));
}

} // namespace

int main() {
    test_binary_reader();
    test_frontend();
    test_effect_format();
    test_enemy_spawn();
    test_ai_personality();
    test_node_data();
    test_culling_contract();
    const auto root = std::filesystem::temp_directory_path()
        / ("fruity_prime_native_formats_"
           + std::to_string(std::chrono::steady_clock::now()
                                .time_since_epoch().count())
           + "_" + process_id_string());
    std::filesystem::create_directories(root);
    try {
        test_archive(root);
        test_nds_rom(root);
    } catch (const std::exception& error) {
        std::filesystem::remove_all(root);
        std::cerr << "format test failed: " << error.what() << '\n';
        return 1;
    } catch (...) {
        std::filesystem::remove_all(root);
        std::cerr << "format test failed: unknown exception\n";
        return 1;
    }
    std::filesystem::remove_all(root);
    return 0;
}
