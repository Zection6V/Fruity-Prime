#pragma once

#include "Formats/Types.hpp"

#include <span>
#include <string_view>

namespace fruityprime::testing::parse {

// Native counterpart of Testing/TestParse.cs.  The helpers intentionally
// accept the same raw-byte text used in the reverse-engineering notes: signed
// 32-bit fixed-point words are written in little-endian byte order and matrix
// rows are supplied in the cartridge's byte order.
[[nodiscard]] formats::Matrix3 test_vectors(formats::Vector3 field58,
                                              formats::Vector3 field64,
                                              formats::Vector3 field70) noexcept;

[[nodiscard]] formats::Vector3 parse_vector3(std::string_view values);

[[nodiscard]] formats::Matrix4x3 parse_matrix12(
    std::span<const std::string_view> values);
[[nodiscard]] formats::Matrix4 parse_matrix16(
    std::span<const std::string_view> values);
[[nodiscard]] formats::Matrix4 parse_matrix16(std::string_view values);
[[nodiscard]] formats::Matrix4x3 parse_matrix48(std::string_view values);
[[nodiscard]] formats::Matrix4 parse_matrix64(std::string_view values);

} // namespace fruityprime::testing::parse
