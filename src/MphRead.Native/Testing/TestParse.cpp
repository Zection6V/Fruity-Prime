#include "Testing/test_parse.hpp"

#include <array>
#include <charconv>
#include <cctype>
#include <stdexcept>
#include <string>
#include <vector>

namespace fruityprime::testing::parse {
namespace {

[[nodiscard]] std::vector<std::string_view> split_words(
    std::string_view values) {
    std::vector<std::string_view> words;
    std::size_t offset = 0;
    while (offset < values.size()) {
        while (offset < values.size()
               && std::isspace(static_cast<unsigned char>(values[offset]))) {
            ++offset;
        }
        if (offset == values.size()) {
            break;
        }
        const std::size_t start = offset;
        while (offset < values.size()
               && !std::isspace(static_cast<unsigned char>(values[offset]))) {
            ++offset;
        }
        words.push_back(values.substr(start, offset - start));
    }
    return words;
}

[[nodiscard]] std::int32_t parse_hex_word(std::string_view value,
                                           std::size_t width) {
    if (value.size() != width) {
        throw std::invalid_argument("hex word has an unexpected width");
    }
    std::uint32_t raw = 0;
    const auto [end, error] = std::from_chars(
        value.data(), value.data() + value.size(), raw, 16);
    if (error != std::errc{} || end != value.data() + value.size()) {
        throw std::invalid_argument("invalid hexadecimal word");
    }
    return static_cast<std::int32_t>(raw);
}

[[nodiscard]] float fixed_word(std::string_view value) {
    return static_cast<float>(parse_hex_word(value, 8)) / 4096.0F;
}

template <std::size_t N>
[[nodiscard]] std::array<std::string, N> make_words(
    std::span<const std::string_view> bytes) {
    if (bytes.size() != N * 4) {
        throw std::invalid_argument("matrix has an unexpected byte count");
    }
    std::array<std::string, N> words{};
    for (std::size_t index = 0; index < N; ++index) {
        words[index].reserve(8);
        for (std::size_t byte = 0; byte < 4; ++byte) {
            words[index].append(bytes[index * 4 + 3 - byte]);
        }
    }
    return words;
}

template <std::size_t N>
[[nodiscard]] std::array<std::string_view, N> views(
    const std::array<std::string, N>& words) noexcept {
    std::array<std::string_view, N> result{};
    for (std::size_t index = 0; index < N; ++index) {
        result[index] = words[index];
    }
    return result;
}

} // namespace

formats::Matrix3 test_vectors(formats::Vector3 field58,
                              formats::Vector3 field64,
                              formats::Vector3 field70) noexcept {
    formats::Matrix3 result{
        field58.x, 0.0F, field58.z,
        field64.x, field64.y, field64.z,
        field70.x, 0.0F, field70.y};
    const formats::Vector3 row0{result.m11, result.m12, result.m13};
    const formats::Vector3 row1{result.m21, result.m22, result.m23};
    const formats::Vector3 row2 = formats::cross(row0, row1);
    result.m31 = row2.x;
    result.m32 = row2.y;
    result.m33 = row2.z;
    const formats::Vector3 corrected_row1 = formats::cross(row2, row0);
    result.m21 = corrected_row1.x;
    result.m22 = corrected_row1.y;
    result.m23 = corrected_row1.z;

    const formats::Vector3 normalized0 = formats::Vector3{
        result.m11, result.m12, result.m13}.normalized();
    const formats::Vector3 normalized1 = formats::Vector3{
        result.m21, result.m22, result.m23}.normalized();
    const formats::Vector3 normalized2 = formats::Vector3{
        result.m31, result.m32, result.m33}.normalized();
    return {
        normalized0.x, normalized0.y, normalized0.z,
        normalized1.x, normalized1.y, normalized1.z,
        normalized2.x, normalized2.y, normalized2.z};
}

formats::Vector3 parse_vector3(std::string_view values) {
    const auto bytes = split_words(values);
    if (bytes.size() != 12) {
        throw std::invalid_argument("Vector3 needs 12 byte words");
    }
    const auto words = make_words<3>(bytes);
    return {fixed_word(words[0]), fixed_word(words[1]), fixed_word(words[2])};
}

formats::Matrix4x3 parse_matrix12(
    std::span<const std::string_view> values) {
    if (values.size() != 12) {
        throw std::invalid_argument("Matrix4x3 needs 12 words");
    }
    std::array<float, 12> parsed{};
    for (std::size_t index = 0; index < parsed.size(); ++index) {
        parsed[index] = fixed_word(values[index]);
    }
    return {
        parsed[0], parsed[1], parsed[2], parsed[3],
        parsed[4], parsed[5], parsed[6], parsed[7],
        parsed[8], parsed[9], parsed[10], parsed[11]};
}

formats::Matrix4 parse_matrix16(std::span<const std::string_view> values) {
    if (values.size() != 16) {
        throw std::invalid_argument("Matrix4 needs 16 words");
    }
    std::array<float, 16> parsed{};
    for (std::size_t index = 0; index < parsed.size(); ++index) {
        parsed[index] = fixed_word(values[index]);
    }
    return {
        parsed[0], parsed[1], parsed[2], parsed[3],
        parsed[4], parsed[5], parsed[6], parsed[7],
        parsed[8], parsed[9], parsed[10], parsed[11],
        parsed[12], parsed[13], parsed[14], parsed[15]};
}

formats::Matrix4 parse_matrix16(std::string_view values) {
    const auto words = split_words(values);
    return parse_matrix16(words);
}

formats::Matrix4x3 parse_matrix48(std::string_view values) {
    const auto bytes = split_words(values);
    const auto words = make_words<12>(bytes);
    const auto word_views = views(words);
    return parse_matrix12(word_views);
}

formats::Matrix4 parse_matrix64(std::string_view values) {
    const auto bytes = split_words(values);
    const auto words = make_words<16>(bytes);
    const auto word_views = views(words);
    return parse_matrix16(word_views);
}

} // namespace fruityprime::testing::parse
