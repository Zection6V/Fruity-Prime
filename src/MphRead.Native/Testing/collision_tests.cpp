#include "Formats/collision_format.hpp"
#include "Formats/collision_query.hpp"

#include <cstdint>
#include <iostream>
#include <stdexcept>
#include <string>
#include <utility>
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

void put_vector4(std::vector<std::uint8_t>& bytes, std::size_t offset,
                 std::int32_t x, std::int32_t y, std::int32_t z,
                 std::int32_t w) {
    put_vector3(bytes, offset, x, y, z);
    put_i32(bytes, offset + 12, w);
}

void put_name(std::vector<std::uint8_t>& bytes, std::size_t offset,
              std::size_t width, const char* value) {
    for (std::size_t i = 0; value[i] != 0 && i < width; ++i) {
        bytes.at(offset + i) = static_cast<std::uint8_t>(value[i]);
    }
}

std::vector<std::uint8_t> make_mph() {
    constexpr std::size_t points = 84;
    constexpr std::size_t planes = points + 36;
    constexpr std::size_t point_indices = planes + 16;
    constexpr std::size_t data = point_indices + 6;
    constexpr std::size_t data_indices = data + 16;
    constexpr std::size_t entries = data_indices + 2;
    constexpr std::size_t portals = entries + 4;
    std::vector<std::uint8_t> bytes(portals + 224, 0);
    put_name(bytes, 0, 4, "wc01");
    put_u32(bytes, 4, 3);
    put_u32(bytes, 8, points);
    put_u32(bytes, 12, 1);
    put_u32(bytes, 16, planes);
    put_u32(bytes, 20, 3);
    put_u32(bytes, 24, point_indices);
    put_u32(bytes, 28, 1);
    put_u32(bytes, 32, data);
    put_u32(bytes, 36, 1);
    put_u32(bytes, 40, data_indices);
    put_i32(bytes, 44, 2);
    put_i32(bytes, 48, 3);
    put_i32(bytes, 52, 4);
    put_vector3(bytes, 56, -0x4000, -0x2000, 0);
    put_u32(bytes, 68, 1);
    put_u32(bytes, 72, entries);
    put_u32(bytes, 76, 1);
    put_u32(bytes, 80, portals);

    put_vector3(bytes, points, 0x1000, 0, 0);
    put_vector3(bytes, points + 12, 0, 0x1000, 0);
    put_vector3(bytes, points + 24, 0, 0, 0x1000);
    put_vector4(bytes, planes, 0, 0, 0x1000, 0);
    put_u16(bytes, point_indices, 0);
    put_u16(bytes, point_indices + 2, 1);
    put_u16(bytes, point_indices + 4, 2);
    put_i32(bytes, data, 99);
    put_u16(bytes, data + 4, 0);
    put_u16(bytes, data + 6, 0x201);
    put_u16(bytes, data + 8, 4);
    put_u16(bytes, data + 12, 3);
    put_u16(bytes, data + 14, 0);
    put_u16(bytes, data_indices, 0);
    put_u16(bytes, entries, 1);
    put_u16(bytes, entries + 2, 0);
    put_name(bytes, portals, 40, "port_a_b");
    put_name(bytes, portals + 40, 24, "a");
    put_name(bytes, portals + 64, 24, "b");
    put_u16(bytes, portals + 216, 1);
    put_u16(bytes, portals + 218, 4);
    put_u16(bytes, portals + 220, 4);
    return bytes;
}

std::vector<std::uint8_t> make_first_hunt() {
    constexpr std::size_t points = 72;
    constexpr std::size_t planes = points + 24;
    constexpr std::size_t vectors = planes + 16;
    constexpr std::size_t data = vectors + 6;
    constexpr std::size_t data_indices = data + 6;
    constexpr std::size_t entries = data_indices + 2;
    constexpr std::size_t tree_indices = entries + 28;
    constexpr std::size_t tree_nodes = tree_indices + 4;
    constexpr std::size_t portals = tree_nodes + 28;
    std::vector<std::uint8_t> bytes(portals + 96, 0);
    put_u32(bytes, 0, 2);
    put_u32(bytes, 4, points);
    put_u32(bytes, 8, 1);
    put_u32(bytes, 12, planes);
    put_u32(bytes, 16, 1);
    put_u32(bytes, 20, vectors);
    put_u16(bytes, 24, 1);
    put_u16(bytes, 26, 4);
    put_u32(bytes, 28, data);
    put_u32(bytes, 32, 1);
    put_u32(bytes, 36, data_indices);
    put_u32(bytes, 40, 1);
    put_u32(bytes, 44, entries);
    put_u32(bytes, 48, 1);
    put_u32(bytes, 52, tree_indices);
    put_u32(bytes, 56, 1);
    put_u32(bytes, 60, tree_nodes);
    put_u32(bytes, 64, 1);
    put_u32(bytes, 68, portals);

    put_vector3(bytes, points, 0x1000, 0x2000, 0x3000);
    put_vector3(bytes, points + 12, 0x4000, 0x5000, 0x6000);
    put_vector4(bytes, planes, 0, 0x1000, 0, -0x1000);
    put_u16(bytes, vectors, 0);
    put_u16(bytes, vectors + 2, 1);
    put_u16(bytes, vectors + 4, 0);
    put_u16(bytes, data, 0);
    put_u16(bytes, data + 2, 1);
    put_u16(bytes, data + 4, 0);
    put_u16(bytes, data_indices, 0);
    put_vector3(bytes, entries, -0x1000, -0x1000, -0x1000);
    put_vector3(bytes, entries + 12, 0x1000, 0x1000, 0x1000);
    put_u16(bytes, entries + 24, 1);
    put_u16(bytes, entries + 26, 0);
    put_i32(bytes, tree_indices, 7);
    put_vector3(bytes, tree_nodes, -0x2000, -0x2000, -0x2000);
    put_vector3(bytes, tree_nodes + 12, 0x2000, 0x2000, 0x2000);
    put_u16(bytes, tree_nodes + 24, 0x8000);
    put_u16(bytes, tree_nodes + 26, 0x8000);
    put_name(bytes, portals, 40, "fh_portal");
    put_name(bytes, portals + 40, 16, "room0");
    put_name(bytes, portals + 56, 16, "room1");
    put_u16(bytes, portals + 88, 1);
    put_u16(bytes, portals + 90, 0);
    bytes[portals + 92] = 3;
    bytes[portals + 93] = 4;
    return bytes;
}

} // namespace

int main() {
    try {
        const fruityprime::collision::File mph =
            fruityprime::collision::File::from_bytes(make_mph());
        require(mph.is_mph(), "MPH collision was not detected");
        require(mph.mph().header.parts_x == 2, "MPH partition mismatch");
        require(mph.mph().points.size() == 3, "MPH point count mismatch");
        require(mph.mph().data[0].flags == 0x201,
                "MPH collision flags mismatch");
        require(mph.mph().portals[0].name == "port_a_b",
                "MPH portal name mismatch");

        auto layers = mph.mph();
        layers.data.push_back(layers.data[0]);
        layers.data[0].layer_mask = 0x80;
        layers.data[1].layer_mask = 4;
        layers.data_indices = {0, 1, 0, 1};
        layers.entries = {{2, 0}, {2, 2}, {0, 123}};
        const auto common = layers.with_layer_mask(0);
        require(common.data.size() == 1 && common.data_indices.size() == 2
                    && common.data_indices[0] == 0 && common.data_indices[1] == 0
                    && common.entries[1].data_start_index == 1
                    && common.entries[2].data_start_index == 123,
                "collision layer filtering lost duplicate indices or empty entries");
        const auto selected = layers.with_layer_mask(0x80);
        require(selected.data.size() == 2 && selected.data_indices[2] == 0,
                "collision layer filtering failed to retain shared and selected layers");
        require(layers.with_layer_mask(-1).data_indices == layers.data_indices,
                "unfiltered collision reordered data");
        require(layers.part_index_from_entry(9) == std::array<int,3>{1,1,0},
                "collision partition uses the wrong axis order");
        require(layers.entry_index_from_point(-4.1F,-2,0) == 0,
                "collision entry conversion must truncate toward zero");
        fruityprime::collision::MphPortal portal;
        portal.plane.z.value = 4096;
        portal.planes[0].x.value = 4096;
        portal.planes[1].x.value = -4096;
        portal.planes[2].y.value = 4096;
        portal.planes[3].y.value = -4096;
        for (auto& plane : portal.planes) plane.w.value = -4096;
        require(fruityprime::collision::check_port_between_points(
                    portal,{0,0,1},{0,0,-1},false), "portal crossing missed");
        require(!fruityprime::collision::check_port_between_points(
                    portal,{0,0,-1},{0,0,1},false), "portal crossing accepted wrong side");
        require(fruityprime::collision::check_port_between_points(
                    portal,{0,0,-1},{0,0,1},true), "portal reverse crossing missed");
        require(!fruityprime::collision::check_port_between_points(
                    portal,{3,0,1},{3,0,-1},false), "portal edge rejection missed");

        auto floor_bytes = make_mph();
        put_vector3(floor_bytes, 84, 0, 0, 0);
        put_vector3(floor_bytes, 96, 0x1000, 0, 0);
        put_vector3(floor_bytes, 108, 0, 0x1000, 0);
        auto ignored_bytes = floor_bytes;
        const auto floor = fruityprime::collision::File::from_bytes(
            std::move(floor_bytes));
        const auto hit = fruityprime::collision::sweep_sphere(
            floor, {0.2F, 0.2F, 1.0F}, {0.2F, 0.2F, -1.0F}, 0.1F);
        require(hit.has_value() && hit->fraction > 0.4F
                    && hit->fraction < 0.6F && hit->contact.z > -0.001F
                    && hit->contact.z < 0.001F,
                "MPH sphere sweep missed the synthetic floor");
        put_u16(ignored_bytes, 142 + 6, 0x2201);
        const auto ignored_floor = fruityprime::collision::File::from_bytes(
            std::move(ignored_bytes));
        const auto ignored = fruityprime::collision::sweep_sphere(
            ignored_floor, {0.2F, 0.2F, 1.0F}, {0.2F, 0.2F, -1.0F},
            0.1F, 0x2000);
        require(!ignored.has_value(), "MPH sphere sweep ignored mask failed");

        const fruityprime::collision::File first_hunt =
            fruityprime::collision::File::from_bytes(make_first_hunt());
        require(first_hunt.is_first_hunt(), "First Hunt collision was not detected");
        require(first_hunt.first_hunt().header.data_start_index == 4,
                "First Hunt data start mismatch");
        require(first_hunt.first_hunt().vectors[0].point2_index == 1,
                "First Hunt vector mismatch");
        require(first_hunt.first_hunt().tree_nodes[0].left_index == 0x8000,
                "First Hunt tree node mismatch");
        require(first_hunt.first_hunt().portals[0].name == "fh_portal",
                "First Hunt portal name mismatch");

        bool rejected = false;
        try {
            static_cast<void>(fruityprime::collision::File::from_bytes(
                std::vector<std::uint8_t>(3)));
        } catch (const std::runtime_error&) {
            rejected = true;
        }
        require(rejected, "short collision input was accepted");
        std::cout << "native collision format tests passed\n";
        return 0;
    } catch (const std::exception& error) {
        std::cerr << "native collision format tests failed: " << error.what()
                  << '\n';
        return 1;
    }
}
