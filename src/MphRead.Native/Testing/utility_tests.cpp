#include "Utility/console_setup.hpp"
#include "Mods/credits.hpp"
#include "Mods/Update/update_http.hpp"
#include "Utility/output.hpp"
#include "Utility/parser.hpp"

#include <array>
#include <bitset>
#include <cassert>
#include <cstdint>
#include <filesystem>
#include <iostream>
#include <sstream>
#include <stdexcept>

namespace {

struct Record {
    std::uint16_t value;
    std::uint16_t flags;
};

static_assert(sizeof(Record) == 4);

} // namespace

int main() {
    const auto& launch_directory =
        fruityprime::utility::console::launch_directory();
    assert(!launch_directory.empty());
    assert(fruityprime::utility::console::resolve_launch_path("fixture.bin")
           == (launch_directory / "fixture.bin").lexically_normal());
    assert(fruityprime::ConsoleSetup::LaunchDirectory() == launch_directory);
    assert(fruityprime::ConsoleSetup::ResolveLaunchPath("fixture.bin")
           == (launch_directory / "fixture.bin").lexically_normal());

    assert(fruityprime::credits::entries().size() == 12);
    assert(fruityprime::credits::summary().find("Fruity Prime")
           != std::string::npos);
    assert(fruityprime::credits::names().find("NoneGiven")
           == std::string::npos);
    assert(fruityprime::credits::compact().find("A fork of MphRead")
           != std::string::npos);
    assert(fruityprime::mods::Credits::Entries().size() == 12);
    assert(fruityprime::mods::Credits::Summary()
               == fruityprime::credits::summary());
    assert(fruityprime::mods::Credits::Compact()
               == fruityprime::credits::compact());
    std::ostringstream credit_output;
    fruityprime::mods::Credits::Print(credit_output);
    assert(credit_output.str().find("Fruity Prime") != std::string::npos);
    const auto empty_http = fruityprime::update::SyncHttp::Send("");
    assert(!empty_http.success() && !empty_http.error.empty());

    const std::array<std::uint8_t, 8> bytes{
        0x34, 0x12, 0x80, 0x01,
        0xef, 0xbe, 0x02, 0x00
    };
    const auto records = fruityprime::utility::parse_bytes<Record>(4, 2, bytes);
    assert(records.size() == 2);
    assert(records[0].value == 0x1234 && records[0].flags == 0x0180);
    assert(records[1].value == 0xbeef && records[1].flags == 0x0002);

    bool size_failed = false;
    try {
        static_cast<void>(fruityprime::utility::parse_bytes<Record>(bytes, 2));
    } catch (const std::invalid_argument&) {
        size_failed = true;
    }
    assert(size_failed);

    const auto float_bits = fruityprime::utility::parse_float_bits(
        0x3ff0000000000000ULL);
    assert(float_bits.value == 1.0 && float_bits.fixed_12 == 1.0 / 4096.0);

    constexpr std::uint32_t polygon =
        (1U << 0) | (1U << 2) | (2U << 4) | (1U << 6)
        | (1U << 11) | (1U << 15) | (17U << 16) | (42U << 24);
    const auto decoded = fruityprime::utility::decode_polygon_attr(polygon);
    assert(decoded.light1 && !decoded.light2 && decoded.light3);
    assert(decoded.polygon_mode == fruityprime::formats::PolygonMode::Toon);
    assert(decoded.back_face && !decoded.front_face && decoded.set_new_depth);
    assert(decoded.fog && decoded.alpha == 17 && decoded.polygon_id == 42);
    const auto description = fruityprime::utility::describe_polygon_attr(polygon);
    assert(description.substr(0, 32)
           == std::bitset<32>(polygon).to_string());
    assert(description.find("Polygon mode: Toon") != std::string::npos);
    assert(description.find("Alpha: 17") != std::string::npos);

    std::istringstream parser_input("1\n00010001\nx\nx\n");
    std::ostringstream parser_output;
    fruityprime::utility::parser_main_loop(parser_input, parser_output);
    assert(parser_output.str().find("Light 1: Yes") != std::string::npos);
    assert(parser_output.str().find("Alpha: 1") != std::string::npos);

    // Output.cs is a serialized worker rather than a direct cout wrapper:
    // verify that queued writes and the prompt/response hand-off both reach
    // the caller in order.
    auto* old_output = std::cout.rdbuf();
    auto* old_input = std::cin.rdbuf();
    std::ostringstream captured_output;
    std::istringstream supplied_input("native input\n");
    std::cout.rdbuf(captured_output.rdbuf());
    std::cin.rdbuf(supplied_input.rdbuf());
    fruityprime::utility::output::Console::begin();
    const auto batch = fruityprime::utility::output::Console::start_batch();
    fruityprime::utility::output::Console::write("queued line", batch);
    fruityprime::utility::output::Console::end_batch();
    const std::string input = fruityprime::utility::output::Console::read(
        "prompt: ");
    fruityprime::utility::output::Console::end();
    std::cin.rdbuf(old_input);
    std::cout.rdbuf(old_output);
    assert(input == "native input");
    assert(captured_output.str().find("queued line\nprompt: ")
           != std::string::npos);
    std::cout << "native utility parser tests passed\n";
    return 0;
}
