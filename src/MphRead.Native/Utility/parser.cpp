#include "Utility/parser.hpp"

#include <bitset>
#include <iomanip>
#include <iostream>
#include <sstream>

namespace fruityprime::utility {
namespace {

[[nodiscard]] const char* polygon_mode_name(formats::PolygonMode mode) noexcept {
    switch (mode) {
    case formats::PolygonMode::Modulate: return "Modulate";
    case formats::PolygonMode::Decal: return "Decal";
    case formats::PolygonMode::Toon: return "Toon";
    case formats::PolygonMode::Shadow: return "Shadow";
    }
    return "Unknown";
}

void append_bool(std::ostringstream& output, const char* name, bool value) {
    output << name << ": " << (value ? "Yes" : "No") << '\n';
}

} // namespace

FloatBits parse_float_bits(std::uint64_t bits) noexcept {
    const double value = std::bit_cast<double>(bits);
    return {value, value / 4096.0};
}

PolygonAttribute decode_polygon_attr(std::uint32_t value) noexcept {
    return PolygonAttribute{
        (value & (1U << 0)) != 0,
        (value & (1U << 1)) != 0,
        (value & (1U << 2)) != 0,
        (value & (1U << 3)) != 0,
        static_cast<formats::PolygonMode>((value >> 4) & 0x3U),
        (value & (1U << 6)) != 0,
        (value & (1U << 7)) != 0,
        (value & (1U << 11)) != 0,
        (value & (1U << 12)) != 0,
        (value & (1U << 13)) != 0,
        (value & (1U << 14)) != 0,
        (value & (1U << 15)) != 0,
        static_cast<std::uint8_t>((value >> 16) & 0x1fU),
        static_cast<std::uint8_t>((value >> 24) & 0x3fU)
    };
}

std::string describe_polygon_attr(std::uint32_t value) {
    const auto decoded = decode_polygon_attr(value);
    std::ostringstream output;
    output << std::bitset<32>(value) << '\n';
    append_bool(output, "Light 1", decoded.light1);
    append_bool(output, "Light 2", decoded.light2);
    append_bool(output, "Light 3", decoded.light3);
    append_bool(output, "Light 4", decoded.light4);
    output << "Polygon mode: " << polygon_mode_name(decoded.polygon_mode)
           << '\n';
    append_bool(output, "Back face", decoded.back_face);
    append_bool(output, "Front face", decoded.front_face);
    append_bool(output, "Set new depth", decoded.set_new_depth);
    append_bool(output, "Render far", decoded.render_far);
    append_bool(output, "Render 1-dot", decoded.render_one_dot);
    append_bool(output, "Equal depth test", decoded.equal_depth_test);
    append_bool(output, "Enable fog", decoded.fog);
    output << "Alpha: " << static_cast<int>(decoded.alpha) << '\n'
           << "Polygon ID: " << static_cast<int>(decoded.polygon_id);
    return output.str();
}

void parser_main_loop(std::istream& input, std::ostream& output) {
    for (;;) {
        output << "1: POLYGON_ATTR\nx: quit\n";
        std::string choice;
        if (!std::getline(input, choice) || choice == "x" || choice == "X") {
            return;
        }
        if (choice != "1") {
            continue;
        }
        for (;;) {
            output << "Value: ";
            std::string value;
            if (!std::getline(input, value) || value == "x" || value == "X") {
                break;
            }
            if (value.size() > 8) {
                continue;
            }
            std::uint32_t parsed = 0;
            std::istringstream parser(value);
            parser >> std::hex >> parsed;
            if (!value.empty() && parser && parser.peek() == std::char_traits<char>::eof()) {
                output << describe_polygon_attr(parsed) << "\n\n";
            }
        }
    }
}

} // namespace fruityprime::utility
