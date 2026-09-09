#pragma once

#include <cstdint>
#include <string>
#include <string_view>

namespace fruityprime::formats {

// types.hpp includes this header for the fixed-point primitives, so the float
// vector and matrix types are only declared here.  The conversions below keep
// the managed member API and are defined in types.hpp, once those types are
// complete.
struct Vector3;
struct Vector4;
struct Matrix4;

struct Fixed {
    std::int32_t value = 0;

    [[nodiscard]] float to_float() const noexcept {
        return static_cast<float>(value) / 4096.0F;
    }

    [[nodiscard]] static float to_float(std::int64_t raw) noexcept {
        return static_cast<float>(raw) / 4096.0F;
    }

    // Fixed.ToFloat(string): the managed metadata tables spell raw values as
    // hexadecimal text, with or without an 0x prefix, and parse them as a
    // signed 32-bit quantity.
    [[nodiscard]] static float to_float(std::string_view text) {
        return to_float(static_cast<std::int64_t>(parse_hex(text)));
    }

    [[nodiscard]] static std::int32_t to_int(float value) noexcept {
        return static_cast<std::int32_t>(value * 4096.0F);
    }

    [[nodiscard]] static std::int32_t parse_hex(std::string_view text) {
        std::string_view digits = text;
        if (digits.size() > 2 && digits[0] == '0'
            && (digits[1] == 'x' || digits[1] == 'X')) {
            digits.remove_prefix(2);
        }
        return static_cast<std::int32_t>(static_cast<std::uint32_t>(
            std::stoul(std::string(digits), nullptr, 16)));
    }
};

// Vector3i: the managed integer vector used wherever a position is kept in
// raw 1/4096 units rather than converted to float.
struct Vector3i {
    std::int32_t x = 0;
    std::int32_t y = 0;
    std::int32_t z = 0;
};

struct Vector3Fx {
    Fixed x;
    Fixed y;
    Fixed z;

    [[nodiscard]] Vector3 to_float_vector() const noexcept;
    [[nodiscard]] Vector3i to_int_vector() const noexcept {
        return {x.value, y.value, z.value};
    }
};

struct Vector4Fx {
    Fixed x;
    Fixed y;
    Fixed z;
    Fixed w;

    [[nodiscard]] Vector4 to_float_vector() const noexcept;
};

struct Matrix43Fx {
    Vector3Fx one;
    Vector3Fx two;
    Vector3Fx three;
    Vector3Fx four;
};

struct Matrix44Fx {
    Vector4Fx one;
    Vector4Fx two;
    Vector4Fx three;
    Vector4Fx four;

    [[nodiscard]] Matrix4 to_float_matrix() const noexcept;
};

// TypeExtensions.ToFixedVector / ToVector3Fx.
[[nodiscard]] Vector3i to_fixed_vector(const Vector3& value) noexcept;
[[nodiscard]] Vector3Fx to_vector3_fx(const Vector3& value) noexcept;

} // namespace fruityprime::formats
