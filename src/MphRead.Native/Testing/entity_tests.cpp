#include "Formats/entity_format.hpp"

#include <cstdint>
#include <iostream>
#include <stdexcept>
#include <string>
#include <vector>

namespace {

void require(bool condition, const char* message) {
    if (!condition) {
        throw std::runtime_error(message);
    }
}

void put_u16(std::vector<std::uint8_t>& bytes, std::size_t offset,
             std::uint16_t value) {
    bytes.at(offset) = static_cast<std::uint8_t>(value);
    bytes.at(offset + 1) = static_cast<std::uint8_t>(value >> 8);
}

void put_i16(std::vector<std::uint8_t>& bytes, std::size_t offset,
             std::int16_t value) {
    put_u16(bytes, offset, static_cast<std::uint16_t>(value));
}

void put_u32(std::vector<std::uint8_t>& bytes, std::size_t offset,
             std::uint32_t value) {
    bytes.at(offset) = static_cast<std::uint8_t>(value);
    bytes.at(offset + 1) = static_cast<std::uint8_t>(value >> 8);
    bytes.at(offset + 2) = static_cast<std::uint8_t>(value >> 16);
    bytes.at(offset + 3) = static_cast<std::uint8_t>(value >> 24);
}

void put_i32(std::vector<std::uint8_t>& bytes, std::size_t offset,
             std::int32_t value) {
    put_u32(bytes, offset, static_cast<std::uint32_t>(value));
}

void put_vector3(std::vector<std::uint8_t>& bytes, std::size_t offset,
                 std::int32_t x, std::int32_t y, std::int32_t z) {
    put_i32(bytes, offset, x);
    put_i32(bytes, offset + 4, y);
    put_i32(bytes, offset + 8, z);
}

void put_name(std::vector<std::uint8_t>& bytes, std::size_t offset,
              std::size_t width, const char* value) {
    for (std::size_t i = 0; value[i] != 0 && i < width; ++i) {
        bytes.at(offset + i) = static_cast<std::uint8_t>(value[i]);
    }
}

void put_data_header(std::vector<std::uint8_t>& bytes, std::size_t offset,
                     std::uint16_t type, std::int16_t id) {
    put_u16(bytes, offset, type);
    put_i16(bytes, offset + 2, id);
    put_vector3(bytes, offset + 4, 0x1000, 0x2000, 0x3000);
    put_vector3(bytes, offset + 16, 0, 0x1000, 0);
    put_vector3(bytes, offset + 28, 0, 0, 0x1000);
}

std::vector<std::uint8_t> make_version_two() {
    constexpr std::size_t data_offset = 100;
    constexpr std::size_t payload_length = 44;
    std::vector<std::uint8_t> bytes(data_offset + payload_length, 0);
    put_u32(bytes, 0, 2);
    put_u16(bytes, 4, 1);
    put_name(bytes, 36, 16, "room_node");
    put_u16(bytes, 52, 3);
    put_u16(bytes, 54, payload_length);
    put_u32(bytes, 56, data_offset);
    put_u32(bytes, 80, 0);
    put_data_header(bytes, data_offset, 8, -4);
    bytes[data_offset + 40] = 0xaa;
    bytes[data_offset + 41] = 0xbb;
    bytes[data_offset + 42] = 0xcc;
    bytes[data_offset + 43] = 0xdd;
    return bytes;
}

std::vector<std::uint8_t> make_first_hunt() {
    constexpr std::size_t data_offset = 64;
    constexpr std::size_t payload_size = 268;
    std::vector<std::uint8_t> bytes(data_offset + payload_size, 0);
    put_u32(bytes, 0, 1);
    put_name(bytes, 4, 16, "fh_node");
    put_u32(bytes, 20, data_offset);
    put_u32(bytes, 24, 0);
    put_data_header(bytes, data_offset, 6, 12);
    bytes[data_offset + payload_size - 1] = 0xee;
    return bytes;
}

} // namespace

int main() {
    try {
        const fruityprime::entity::File version_two =
            fruityprime::entity::File::from_bytes(make_version_two());
        require(!version_two.is_first_hunt() && version_two.version() == 2,
                "version 2 entity file was not detected");
        require(version_two.header().lengths[0] == 1,
                "entity layer count mismatch");
        require(version_two.records().size() == 1,
                "version 2 entity record count mismatch");
        require(version_two.records()[0].entry.node_name == "room_node",
                "entity node name mismatch");
        require(version_two.records()[0].header.type == 8
                    && version_two.records()[0].header.entity_id == -4,
                "entity data header mismatch");
        require(version_two.records()[0].payload.size() == 44
                    && version_two.records()[0].payload.back() == 0xdd,
                "entity payload mismatch");

        const fruityprime::entity::File first_hunt =
            fruityprime::entity::File::from_bytes(make_first_hunt());
        require(first_hunt.is_first_hunt() && first_hunt.version() == 1,
                "First Hunt entity file was not detected");
        require(first_hunt.first_hunt_records().size() == 1,
                "First Hunt entity record count mismatch");
        require(first_hunt.first_hunt_records()[0].entry.node_name == "fh_node",
                "First Hunt entity node name mismatch");
        require(first_hunt.first_hunt_records()[0].header.type == 6,
                "First Hunt data header mismatch");
        require(first_hunt.first_hunt_records()[0].payload.size() == 268
                    && first_hunt.first_hunt_records()[0].payload.back() == 0xee,
                "First Hunt fixed-size payload mismatch");

        bool rejected = false;
        try {
            static_cast<void>(fruityprime::entity::File::from_bytes(
                std::vector<std::uint8_t>{2, 0, 0, 0}));
        } catch (const std::runtime_error&) {
            rejected = true;
        }
        require(rejected, "truncated entity file was accepted");
        std::cout << "native entity format tests passed\n";
        return 0;
    } catch (const std::exception& error) {
        std::cerr << "native entity format tests failed: " << error.what()
                  << '\n';
        return 1;
    }
}
