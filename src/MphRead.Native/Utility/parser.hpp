#pragma once

#include "Formats/Types.hpp"

#include <bit>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <iosfwd>
#include <limits>
#include <span>
#include <stdexcept>
#include <string>
#include <type_traits>
#include <vector>

namespace fruityprime::utility {

// Equivalent of Utility/Parser.ParseBytes for blittable native records. The
// explicit record size is kept in the API because the managed parser checks
// Marshal.SizeOf<T>() at every call site.
template <typename T>
[[nodiscard]] std::vector<T> parse_bytes(
    int size, int count, std::span<const std::uint8_t> bytes) {
    static_assert(std::is_trivially_copyable_v<T>,
                  "parse_bytes requires a trivially copyable record");
    if (size < 0 || count < 0
        || static_cast<std::size_t>(size) != sizeof(T)) {
        throw std::invalid_argument("record size does not match native type");
    }
    const std::size_t record_size = static_cast<std::size_t>(size);
    const std::size_t record_count = static_cast<std::size_t>(count);
    if (record_size == 0 || record_count > bytes.size() / record_size
        || record_count * record_size != bytes.size()) {
        throw std::invalid_argument("record byte array has the wrong size");
    }
    std::vector<T> result(record_count);
    for (std::size_t index = 0; index < result.size(); ++index) {
        std::memcpy(&result[index], bytes.data() + index * record_size,
                    record_size);
    }
    return result;
}

template <typename T>
[[nodiscard]] std::vector<T> parse_bytes(
    std::span<const std::uint8_t> bytes,
    std::size_t record_size = sizeof(T)) {
    if (record_size > static_cast<std::size_t>(std::numeric_limits<int>::max())
        || (record_size != 0
            && bytes.size() / record_size
                   > static_cast<std::size_t>(
                       std::numeric_limits<int>::max()))) {
        throw std::invalid_argument("record array is too large");
    }
    const int count = record_size == 0
        ? 0 : static_cast<int>(bytes.size() / record_size);
    return parse_bytes<T>(static_cast<int>(record_size), count, bytes);
}

struct FloatBits {
    double value = 0.0;
    double fixed_12 = 0.0;
};

[[nodiscard]] FloatBits parse_float_bits(std::uint64_t bits) noexcept;

struct PolygonAttribute {
    bool light1 = false;
    bool light2 = false;
    bool light3 = false;
    bool light4 = false;
    formats::PolygonMode polygon_mode = formats::PolygonMode::Modulate;
    bool back_face = false;
    bool front_face = false;
    bool set_new_depth = false;
    bool render_far = false;
    bool render_one_dot = false;
    bool equal_depth_test = false;
    bool fog = false;
    std::uint8_t alpha = 0;
    std::uint8_t polygon_id = 0;
};

[[nodiscard]] PolygonAttribute decode_polygon_attr(
    std::uint32_t value) noexcept;
[[nodiscard]] std::string describe_polygon_attr(std::uint32_t value);

// Parser.MainLoop with injectable streams so the same interaction is usable
// by the native console and deterministic tests.
void parser_main_loop(std::istream& input, std::ostream& output);

} // namespace fruityprime::utility

namespace MphReadNative {
namespace Parser = ::fruityprime::utility;
}
