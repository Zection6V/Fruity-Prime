#pragma once

#include "Formats/fixed.hpp"

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>
#include <limits>
#include <optional>
#include <string_view>
#include <vector>

// Native counterpart of Formats/Types.cs and the shared value enums from
// Formats/Enums.cs.  These types deliberately do not depend on OpenTK or a
// graphics API; room decoding, gameplay, exporters, and the Windows renderer
// can all use the same representation.
namespace fruityprime::formats {

struct Vector2 {
    float x = 0.0F;
    float y = 0.0F;

    // TypeExtensions.WithX/WithY/AddX/AddY.
    [[nodiscard]] constexpr Vector2 with_x(float value) const noexcept {
        return {value, y};
    }
    [[nodiscard]] constexpr Vector2 with_y(float value) const noexcept {
        return {x, value};
    }
    [[nodiscard]] constexpr Vector2 add_x(float value) const noexcept {
        return {x + value, y};
    }
    [[nodiscard]] constexpr Vector2 add_y(float value) const noexcept {
        return {x, y + value};
    }
};

struct Vector3 {
    float x = 0.0F;
    float y = 0.0F;
    float z = 0.0F;

    constexpr Vector3() = default;
    constexpr Vector3(float x_value, float y_value, float z_value)
        : x(x_value), y(y_value), z(z_value) {}

    [[nodiscard]] constexpr Vector3 operator+() const noexcept { return *this; }
    [[nodiscard]] constexpr Vector3 operator-() const noexcept {
        return {-x, -y, -z};
    }
    [[nodiscard]] constexpr Vector3 operator+(Vector3 other) const noexcept {
        return {x + other.x, y + other.y, z + other.z};
    }
    [[nodiscard]] constexpr Vector3 operator-(Vector3 other) const noexcept {
        return {x - other.x, y - other.y, z - other.z};
    }
    [[nodiscard]] constexpr Vector3 operator*(float scalar) const noexcept {
        return {x * scalar, y * scalar, z * scalar};
    }
    [[nodiscard]] constexpr Vector3 operator/(float scalar) const noexcept {
        return {x / scalar, y / scalar, z / scalar};
    }
    constexpr Vector3& operator+=(Vector3 other) noexcept {
        x += other.x;
        y += other.y;
        z += other.z;
        return *this;
    }
    constexpr Vector3& operator-=(Vector3 other) noexcept {
        x -= other.x;
        y -= other.y;
        z -= other.z;
        return *this;
    }
    constexpr Vector3& operator*=(float scalar) noexcept {
        x *= scalar;
        y *= scalar;
        z *= scalar;
        return *this;
    }

    [[nodiscard]] constexpr float length_squared() const noexcept {
        return x * x + y * y + z * z;
    }
    [[nodiscard]] float length() const noexcept {
        return std::sqrt(length_squared());
    }
    [[nodiscard]] Vector3 normalized() const noexcept {
        const float magnitude = length();
        return magnitude <= std::numeric_limits<float>::epsilon()
            ? Vector3{}
            : *this / magnitude;
    }

    // TypeExtensions.WithX/WithY/WithZ/AddX/AddY/AddZ.
    [[nodiscard]] constexpr Vector3 with_x(float value) const noexcept {
        return {value, y, z};
    }
    [[nodiscard]] constexpr Vector3 with_y(float value) const noexcept {
        return {x, value, z};
    }
    [[nodiscard]] constexpr Vector3 with_z(float value) const noexcept {
        return {x, y, value};
    }
    [[nodiscard]] constexpr Vector3 add_x(float value) const noexcept {
        return {x + value, y, z};
    }
    [[nodiscard]] constexpr Vector3 add_y(float value) const noexcept {
        return {x, y + value, z};
    }
    [[nodiscard]] constexpr Vector3 add_z(float value) const noexcept {
        return {x, y, z + value};
    }
};

[[nodiscard]] constexpr Vector3 operator*(float scalar, Vector3 value) noexcept {
    return value * scalar;
}

[[nodiscard]] constexpr float dot(Vector3 left, Vector3 right) noexcept {
    return left.x * right.x + left.y * right.y + left.z * right.z;
}

[[nodiscard]] constexpr Vector3 cross(Vector3 left, Vector3 right) noexcept {
    return {
        left.y * right.z - left.z * right.y,
        left.z * right.x - left.x * right.z,
        left.x * right.y - left.y * right.x
    };
}

struct Vector4 {
    float x = 0.0F;
    float y = 0.0F;
    float z = 0.0F;
    float w = 0.0F;

    constexpr Vector4() = default;
    constexpr Vector4(float x_value, float y_value, float z_value,
                      float w_value)
        : x(x_value), y(y_value), z(z_value), w(w_value) {}
    constexpr Vector4(Vector3 value, float w_value)
        : x(value.x), y(value.y), z(value.z), w(w_value) {}

    [[nodiscard]] constexpr Vector3 xyz() const noexcept { return {x, y, z}; }

    // TypeExtensions.WithX..WithW / AddX..AddW.
    [[nodiscard]] constexpr Vector4 with_x(float value) const noexcept {
        return {value, y, z, w};
    }
    [[nodiscard]] constexpr Vector4 with_y(float value) const noexcept {
        return {x, value, z, w};
    }
    [[nodiscard]] constexpr Vector4 with_z(float value) const noexcept {
        return {x, y, value, w};
    }
    [[nodiscard]] constexpr Vector4 with_w(float value) const noexcept {
        return {x, y, z, value};
    }
    [[nodiscard]] constexpr Vector4 add_x(float value) const noexcept {
        return {x + value, y, z, w};
    }
    [[nodiscard]] constexpr Vector4 add_y(float value) const noexcept {
        return {x, y + value, z, w};
    }
    [[nodiscard]] constexpr Vector4 add_z(float value) const noexcept {
        return {x, y, z + value, w};
    }
    [[nodiscard]] constexpr Vector4 add_w(float value) const noexcept {
        return {x, y, z, w + value};
    }
};

struct Matrix3 {
    float m11 = 1.0F;
    float m12 = 0.0F;
    float m13 = 0.0F;
    float m21 = 0.0F;
    float m22 = 1.0F;
    float m23 = 0.0F;
    float m31 = 0.0F;
    float m32 = 0.0F;
    float m33 = 1.0F;
};

struct Matrix4 {
    float m11 = 1.0F;
    float m12 = 0.0F;
    float m13 = 0.0F;
    float m14 = 0.0F;
    float m21 = 0.0F;
    float m22 = 1.0F;
    float m23 = 0.0F;
    float m24 = 0.0F;
    float m31 = 0.0F;
    float m32 = 0.0F;
    float m33 = 1.0F;
    float m34 = 0.0F;
    float m41 = 0.0F;
    float m42 = 0.0F;
    float m43 = 0.0F;
    float m44 = 1.0F;

    [[nodiscard]] constexpr Vector4 row(std::size_t index) const noexcept {
        switch (index) {
        case 0: return {m11, m12, m13, m14};
        case 1: return {m21, m22, m23, m24};
        case 2: return {m31, m32, m33, m34};
        default: return {m41, m42, m43, m44};
        }
    }

    // TypeExtensions.Keep3x3: the rotation part, with a zeroed fourth row --
    // not an identity one, which is what the managed code writes.
    [[nodiscard]] constexpr Matrix4 keep3x3() const noexcept {
        return {m11, m12, m13, 0.0F,
                m21, m22, m23, 0.0F,
                m31, m32, m33, 0.0F,
                0.0F, 0.0F, 0.0F, 0.0F};
    }
};

struct Matrix4x3 {
    float m11 = 1.0F;
    float m12 = 0.0F;
    float m13 = 0.0F;
    float m21 = 0.0F;
    float m22 = 1.0F;
    float m23 = 0.0F;
    float m31 = 0.0F;
    float m32 = 0.0F;
    float m33 = 1.0F;
    float m41 = 0.0F;
    float m42 = 0.0F;
    float m43 = 0.0F;

    // TypeExtensions.AsMatrix3 / AsMatrix4 / Keep3x3.  AsMatrix4 promotes the
    // three rows and the translation row with a zero fourth column, which is
    // what OpenTK's Matrix4(Vector3...) constructors produce.
    [[nodiscard]] constexpr Matrix3 as_matrix3() const noexcept {
        return {m11, m12, m13, m21, m22, m23, m31, m32, m33};
    }
    [[nodiscard]] constexpr Matrix4 as_matrix4() const noexcept {
        return {m11, m12, m13, 0.0F,
                m21, m22, m23, 0.0F,
                m31, m32, m33, 0.0F,
                m41, m42, m43, 0.0F};
    }
    [[nodiscard]] constexpr Matrix4 keep3x3() const noexcept {
        return {m11, m12, m13, 0.0F,
                m21, m22, m23, 0.0F,
                m31, m32, m33, 0.0F,
                0.0F, 0.0F, 0.0F, 0.0F};
    }
};

// Formats/Types.cs conversions declared alongside the fixed-point types.
inline Vector3 Vector3Fx::to_float_vector() const noexcept {
    return {x.to_float(), y.to_float(), z.to_float()};
}

inline Vector4 Vector4Fx::to_float_vector() const noexcept {
    return {x.to_float(), y.to_float(), z.to_float(), w.to_float()};
}

inline Matrix4 Matrix44Fx::to_float_matrix() const noexcept {
    const Vector4 r1 = one.to_float_vector();
    const Vector4 r2 = two.to_float_vector();
    const Vector4 r3 = three.to_float_vector();
    const Vector4 r4 = four.to_float_vector();
    return {r1.x, r1.y, r1.z, r1.w, r2.x, r2.y, r2.z, r2.w,
            r3.x, r3.y, r3.z, r3.w, r4.x, r4.y, r4.z, r4.w};
}

inline Vector3i to_fixed_vector(const Vector3& value) noexcept {
    return {Fixed::to_int(value.x), Fixed::to_int(value.y),
            Fixed::to_int(value.z)};
}

inline Vector3Fx to_vector3_fx(const Vector3& value) noexcept {
    return {Fixed{Fixed::to_int(value.x)}, Fixed{Fixed::to_int(value.y)},
            Fixed{Fixed::to_int(value.z)}};
}

struct MatrixOps {
    [[nodiscard]] static Matrix3 get_transform3(Vector3 vector1,
                                                  Vector3 vector2) noexcept {
        const Vector3 up = cross(vector2, vector1).normalized();
        const Vector3 direction = cross(vector1, up);
        return {
            up.x, up.y, up.z,
            direction.x, direction.y, direction.z,
            vector1.x, vector1.y, vector1.z
        };
    }

    [[nodiscard]] static Matrix4 get_transform4(Vector3 vector1,
                                                  Vector3 vector2,
                                                  Vector3 position) noexcept {
        const Matrix3 rotation = get_transform3(vector1, vector2);
        return {
            rotation.m11, rotation.m12, rotation.m13, 0.0F,
            rotation.m21, rotation.m22, rotation.m23, 0.0F,
            rotation.m31, rotation.m32, rotation.m33, 0.0F,
            position.x, position.y, position.z, 1.0F
        };
    }

    [[nodiscard]] static Matrix4 get_transform_srt(Vector3 scale,
                                                    Vector3 angle,
                                                    Vector3 position) noexcept {
        const float sin_ax = std::sin(angle.x);
        const float sin_ay = std::sin(angle.y);
        const float sin_az = std::sin(angle.z);
        const float cos_ax = std::cos(angle.x);
        const float cos_ay = std::cos(angle.y);
        const float cos_az = std::cos(angle.z);
        const float v18 = cos_ax * cos_az;
        const float v19 = cos_ax * sin_az;
        const float v20 = cos_ax * cos_ay;
        const float v22 = sin_ax * sin_ay;
        const float v17 = v19 * sin_ay;
        return {
            scale.x * cos_ay * cos_az,
            scale.x * cos_ay * sin_az,
            scale.x * -sin_ay,
            0.0F,
            scale.y * ((v22 * cos_az) - v19),
            scale.y * ((v22 * sin_az) + v18),
            scale.y * sin_ax * cos_ay,
            0.0F,
            scale.z * (v18 * sin_ay + sin_ax * sin_az),
            scale.z * (v17 + (v19 * sin_ay) - (sin_ax * cos_az)),
            scale.z * v20,
            0.0F,
            position.x, position.y, position.z, 1.0F
        };
    }

    // EntityBase.GetTransformMatrix: build an orientation from a facing and
    // an up vector.  The up vector is re-derived from the orthogonalised
    // right vector, so a caller may pass an up that is not perpendicular to
    // the facing and still get an orthonormal basis.
    [[nodiscard]] static Matrix4 get_transform_matrix(
        Vector3 facing, Vector3 up, Vector3 position = {}) noexcept {
        const Vector3 right = cross(up, facing).normalized();
        const Vector3 fixed_up = cross(facing, right);
        return {right.x, right.y, right.z, 0.0F,
                fixed_up.x, fixed_up.y, fixed_up.z, 0.0F,
                facing.x, facing.y, facing.z, 0.0F,
                position.x, position.y, position.z, 1.0F};
    }

    // EntityBase.RightVector
    [[nodiscard]] static Vector3 right_vector(const Matrix4& transform) noexcept {
        return Vector3{transform.m11, transform.m12, transform.m13}
            .normalized();
    }

    [[nodiscard]] static Vector3 vec3_mult_mtx4(Vector3 value,
                                                 const Matrix4& matrix) noexcept {
        return {
            value.x * matrix.m11 + value.y * matrix.m21
                + value.z * matrix.m31 + matrix.m41,
            value.x * matrix.m12 + value.y * matrix.m22
                + value.z * matrix.m32 + matrix.m42,
            value.x * matrix.m13 + value.y * matrix.m23
                + value.z * matrix.m33 + matrix.m43
        };
    }

    [[nodiscard]] static Vector3 vec3_mult_mtx3(Vector3 value,
                                                 const Matrix4& matrix) noexcept {
        return {
            value.x * matrix.m11 + value.y * matrix.m21 + value.z * matrix.m31,
            value.x * matrix.m12 + value.y * matrix.m22 + value.z * matrix.m32,
            value.x * matrix.m13 + value.y * matrix.m23 + value.z * matrix.m33
        };
    }

    [[nodiscard]] static Vector3 vec4_mult_mtx4x3(Vector4 value,
                                                   const Matrix4x3& matrix) noexcept {
        return {
            value.w * matrix.m41 + value.z * matrix.m31
                + value.x * matrix.m11 + value.y * matrix.m21,
            value.w * matrix.m42 + value.z * matrix.m32
                + value.x * matrix.m12 + value.y * matrix.m22,
            value.w * matrix.m43 + value.z * matrix.m33
                + value.x * matrix.m13 + value.y * matrix.m23
        };
    }

    [[nodiscard]] static Matrix4x3 concat43(const Matrix4x3& first,
                                             const Matrix4x3& second) noexcept {
        Matrix4x3 output{};
        output.m11 = first.m13 * second.m31 + first.m11 * second.m11
            + first.m12 * second.m21;
        output.m12 = first.m13 * second.m32 + first.m11 * second.m12
            + first.m12 * second.m22;
        output.m13 = first.m13 * second.m33 + first.m11 * second.m13
            + first.m12 * second.m23;
        output.m21 = first.m23 * second.m31 + first.m21 * second.m11
            + first.m22 * second.m21;
        output.m22 = first.m23 * second.m32 + first.m21 * second.m12
            + first.m22 * second.m22;
        output.m23 = first.m23 * second.m33 + first.m21 * second.m13
            + first.m22 * second.m23;
        output.m31 = first.m33 * second.m31 + first.m31 * second.m11
            + first.m32 * second.m21;
        output.m32 = first.m33 * second.m32 + first.m31 * second.m12
            + first.m32 * second.m22;
        output.m33 = first.m33 * second.m33 + first.m31 * second.m13
            + first.m32 * second.m23;
        output.m41 = second.m41 + first.m43 * second.m31
            + first.m41 * second.m11 + first.m42 * second.m21;
        output.m42 = second.m42 + first.m43 * second.m32
            + first.m41 * second.m12 + first.m42 * second.m22;
        output.m43 = second.m43 + first.m43 * second.m33
            + first.m41 * second.m13 + first.m42 * second.m23;
        return output;
    }

    // The managed implementation intentionally preserves only the 3x2 terms
    // used by the DS transform path; keep that behavior for replay/export
    // compatibility instead of silently switching to a generic 4x4 multiply.
    [[nodiscard]] static Matrix4 multiply44(const Matrix4& first,
                                             const Matrix4& second) noexcept {
        Matrix4 output{};
        output.m11 = first.m13 * second.m31 + first.m11 * second.m11
            + first.m12 * second.m21;
        output.m12 = first.m13 * second.m32 + first.m11 * second.m12
            + first.m12 * second.m22;
        output.m21 = first.m23 * second.m31 + first.m21 * second.m11
            + first.m22 * second.m21;
        output.m22 = first.m23 * second.m32 + first.m21 * second.m12
            + first.m22 * second.m22;
        output.m31 = first.m33 * second.m31 + first.m31 * second.m11
            + first.m32 * second.m21;
        output.m32 = first.m33 * second.m32 + first.m31 * second.m12
            + first.m32 * second.m22;
        return output;
    }

    [[nodiscard]] static Matrix3 rotate_align(Vector3 from,
                                              Vector3 to) noexcept {
        const Vector3 axis = cross(from, to);
        const float cosine = dot(from, to);
        const float k = 1.0F / (1.0F + cosine);
        return {
            axis.x * axis.x * k + cosine,
            axis.z * axis.x * k - axis.z,
            axis.z * axis.x * k + axis.y,
            axis.x * axis.y * k + axis.z,
            axis.y * axis.y * k + cosine,
            axis.z * axis.y * k - axis.x,
            axis.x * axis.z * k - axis.y,
            axis.y * axis.z * k + axis.x,
            axis.z * axis.z * k + cosine
        };
    }

    // Returns the managed depth (w). screen is normalized top-left based
    // coordinates when depth is positive, and zero otherwise.
    [[nodiscard]] static float project_position(Vector3 position,
                                                 const Matrix4& view,
                                                 const Matrix4& projection,
                                                 Vector2& screen) noexcept {
        const Vector3 transformed = vec3_mult_mtx4(position, view);
        // The OpenTK row/column access in the managed code uses Row3.W and
        // Row0.W/Row1.W/Row2.W.  In named fields those are m44/m14/m24/m34.
        const float corrected_depth = projection.m44 + transformed.x * projection.m14
            + transformed.y * projection.m24 + transformed.z * projection.m34;
        if (corrected_depth <= 0.0F) {
            screen = {};
            return corrected_depth;
        }
        const float x = (projection.m41 + transformed.x * projection.m11
            + transformed.y * projection.m21 + transformed.z * projection.m31)
            / corrected_depth;
        const float y = (projection.m42 + transformed.x * projection.m12
            + transformed.y * projection.m22 + transformed.z * projection.m32)
            / corrected_depth;
        screen.x = (x + 1.0F) * 0.5F;
        screen.y = (1.0F - y) * 0.5F;
        return corrected_depth;
    }

    // Formats/Types.cs Matrix.GetProjectedValues.  This deliberately keeps
    // the two coordinate spaces separate: target is a world-space point one
    // unit toward the camera, while screen_position is the projected NDC
    // pair (not the top-left converted coordinates returned above).
    static void get_projected_values(
        Vector3 position, Vector3 camera_position, const Matrix4& view,
        const Matrix4& projection, float& distance, float& depth,
        float& scale_inverse, Vector3& target,
        Vector2& screen_position) noexcept {
        const Vector3 between = (camera_position - position).normalized();
        target = position + between;
        distance = (camera_position - target).length();
        const Vector3 transformed = vec3_mult_mtx4(position, view);
        depth = projection.m44 + transformed.x * projection.m14
            + transformed.y * projection.m24 + transformed.z * projection.m34;
        if (depth <= 0.0F) {
            scale_inverse = 0.0F;
            screen_position = {};
            return;
        }
        scale_inverse = 1.0F / depth;
        const float x = projection.m41 + transformed.x * projection.m11
            + transformed.y * projection.m21 + transformed.z * projection.m31;
        const float y = projection.m42 + transformed.x * projection.m12
            + transformed.y * projection.m22 + transformed.z * projection.m32;
        screen_position = {x * scale_inverse, y * scale_inverse};
    }
};

struct ColorRgb {
    std::uint8_t red = 0;
    std::uint8_t green = 0;
    std::uint8_t blue = 0;

    [[nodiscard]] Vector3 as_vector3() const noexcept {
        return {red / 255.0F, green / 255.0F, blue / 255.0F};
    }
    [[nodiscard]] Vector4 as_vector4(float alpha = 1.0F) const noexcept {
        return {red / 255.0F, green / 255.0F, blue / 255.0F, alpha};
    }

    [[nodiscard]] Vector3 operator/(float divisor) const noexcept {
        return {red / divisor, green / divisor, blue / divisor};
    }
    [[nodiscard]] constexpr bool operator==(const ColorRgb&) const = default;
};

struct ColorRgba {
    std::uint8_t red = 0;
    std::uint8_t green = 0;
    std::uint8_t blue = 0;
    std::uint8_t alpha = 0;

    // ColorRgba(uint value, byte alpha): expand a packed BGR555 colour, which
    // is how every cartridge palette entry reaches the renderer.
    [[nodiscard]] static ColorRgba from_rgb555(
        std::uint32_t value, std::uint8_t alpha_value = 255) noexcept {
        const auto expand = [](std::uint32_t channel) {
            return static_cast<std::uint8_t>(
                std::lround(static_cast<float>(channel & 0x1f) / 31.0F
                            * 255.0F));
        };
        return {expand(value), expand(value >> 5), expand(value >> 10),
                alpha_value};
    }

    [[nodiscard]] Vector4 as_vector4() const noexcept {
        return {red / 255.0F, green / 255.0F, blue / 255.0F,
                alpha / 255.0F};
    }
    [[nodiscard]] constexpr ColorRgba with_alpha(
        std::uint8_t alpha_value) const noexcept {
        return {red, green, blue, alpha_value};
    }
    [[nodiscard]] constexpr std::uint32_t to_uint() const noexcept {
        return static_cast<std::uint32_t>(red)
            | (static_cast<std::uint32_t>(green) << 8)
            | (static_cast<std::uint32_t>(blue) << 16)
            | (static_cast<std::uint32_t>(alpha) << 24);
    }
    [[nodiscard]] constexpr bool operator==(const ColorRgba&) const = default;
};

enum class RenderItemType : std::uint8_t {
    Mesh = 0, Box = 1, Cylinder = 2, Sphere = 3, Quad = 4, Ngon = 5,
    Particle = 6, TrailSingle = 7, TrailMulti = 8, TrailStack = 9
};
enum class BillboardMode : std::uint8_t { None = 0, Sphere = 1, Cylinder = 2 };
enum class PolygonMode : std::uint32_t { Modulate = 0, Decal = 1, Toon = 2, Shadow = 3 };
enum class RepeatMode : std::uint8_t { Clamp = 0, Repeat = 1, Mirror = 2 };
enum class RenderMode : std::uint8_t {
    Normal = 0, Decal = 1, Translucent = 2, Unknown3 = 3, Unknown4 = 4
};
enum class TexgenMode : std::uint32_t {
    None = 0, Texcoord = 1, Normal = 2, Vertex = 3
};
enum class CullingMode : std::uint8_t { Neither = 0, Front = 1, Back = 2 };
enum class TextureFormat : std::uint8_t {
    Palette2Bit = 0, Palette4Bit = 1, Palette8Bit = 2, PaletteA5I3 = 4,
    DirectRgb = 5, PaletteA3I5 = 6
};

struct LightInfo {
    Vector3 light1_vector;
    Vector3 light1_color;
    Vector3 light2_vector;
    Vector3 light2_color;

    [[nodiscard]] static LightInfo zero() noexcept { return {}; }
};

struct RenderItem {
    RenderItemType type = RenderItemType::Mesh;
    std::int32_t polygon_id = 0;
    float alpha = 0.0F;
    PolygonMode polygon_mode = PolygonMode::Modulate;
    RenderMode render_mode = RenderMode::Normal;
    CullingMode culling_mode = CullingMode::Neither;
    BillboardMode billboard_mode = BillboardMode::None;
    bool wireframe = false;
    bool lighting = false;
    bool no_lines = false;
    Vector3 diffuse{};
    Vector3 ambient{};
    Vector3 specular{};
    Vector3 emission{};
    LightInfo light_info{};
    TexgenMode texgen_mode = TexgenMode::None;
    RepeatMode x_repeat = RepeatMode::Clamp;
    RepeatMode y_repeat = RepeatMode::Clamp;
    bool has_texture = false;
    std::int32_t texture_binding_id = 0;
    Matrix4 texcoord_matrix{};
    Matrix4 transform{};
    std::int32_t list_id = 0;
    std::int32_t matrix_stack_count = 0;
    std::array<float, 16 * 31> matrix_stack{};
    std::optional<Vector4> override_color;
    std::optional<Vector4> palette_override;
    std::vector<Vector3> points;
    std::int32_t item_count = 0;
    float scale_s = 0.0F;
    float scale_t = 0.0F;
};

enum class EntityType : std::uint16_t {
    Platform = 0, Object = 1, PlayerSpawn = 2, Door = 3, ItemSpawn = 4,
    ItemInstance = 5, EnemySpawn = 6, TriggerVolume = 7, AreaVolume = 8,
    JumpPad = 9, PointModule = 10, MorphCamera = 11, OctolithFlag = 12,
    FlagBase = 13, Teleporter = 14, NodeDefense = 15, LightSource = 16,
    Artifact = 17, CameraSequence = 18, ForceField = 19, BeamEffect = 21,
    Bomb = 22, EnemyInstance = 23, Halfturret = 24, Player = 25,
    BeamProjectile = 26, ListHead = 27, FhUnknown0 = 100,
    FhPlayerSpawn = 101, FhUnknown2 = 102, FhDoor = 103,
    FhItemSpawn = 104, FhItemInstance = 105, FhEnemySpawn = 106,
    FhEffectInstance = 107, FhBomb = 108, FhTriggerVolume = 109,
    FhAreaVolume = 110, FhPlatform = 111, FhJumpPad = 112,
    FhPointModule = 113, FhMorphCamera = 114, FhEnemyInstance = 115,
    FhPlayer = 116, FhBeamProjectile = 117, Room = 200, Model = 201,
    All = 255
};
enum class VolumeType : std::uint32_t { Box = 0, Cylinder = 1, Sphere = 2 };
enum class FhVolumeType : std::uint32_t { Sphere = 0, Box = 1, Cylinder = 2 };
enum class DoorType : std::uint32_t { Standard = 0, MorphBall = 1, Boss = 2, Thin = 3 };
enum class ItemType : std::int32_t {
    None = -1, HealthMedium = 0, HealthSmall = 1, HealthBig = 2,
    DoubleDamage = 3, EnergyTank = 4, VoltDriver = 5, MissileExpansion = 6,
    Battlehammer = 7, Imperialist = 8, Judicator = 9, Magmaul = 10,
    ShockCoil = 11, OmegaCannon = 12, UASmall = 13, UABig = 14,
    MissileSmall = 15, MissileBig = 16, Cloak = 17, UAExpansion = 18,
    ArtifactKey = 19, Deathalt = 20, AffinityWeapon = 21,
    PickWpnMissile = 22
};
enum class FhItemType : std::int32_t {
    None = -1, AmmoSmall = 0, AmmoBig = 1, HealthSmall = 2,
    HealthBig = 3, DoubleDamage = 4, PowerBeam = 5, ElectroLob = 6,
    Missile = 7
};
enum class BeamType : std::int8_t {
    None = -1, PowerBeam = 0, VoltDriver = 1, Missile = 2, Battlehammer = 3,
    Imperialist = 4, Judicator = 5, Magmaul = 6, ShockCoil = 7,
    OmegaCannon = 8, Platform = 9, Enemy = 10
};
enum class WeaponUnlockBits : std::uint16_t {
    PowerBeam = 1u << 0,
    VoltDriver = 1u << 1,
    Missile = 1u << 2,
    Battlehammer = 1u << 3,
    Imperialist = 1u << 4,
    Judicator = 1u << 5,
    Magmaul = 1u << 6,
    ShockCoil = 1u << 7,
    OmegaCannon = 1u << 8
};
enum class BombType : std::uint8_t { MorphBall = 0, Stinglarva = 1, Lockjaw = 2 };
enum class Affliction : std::uint8_t { None = 0, Freeze = 1, Disrupt = 2, Burn = 4 };
enum class FadeType : std::uint8_t {
    None = 0, FadeInBlack = 1, FadeOutBlack = 2, FadeInWhite = 3,
    FadeOutWhite = 4, FadeOutInBlack = 5, FadeOutInWhite = 6
};
enum class Terrain : std::uint8_t {
    Metal = 0, OrangeHolo = 1, GreenHolo = 2, BlueHolo = 3, Ice = 4,
    Snow = 5, Sand = 6, Rock = 7, Lava = 8, Acid = 9, Gorea = 10,
    Unknown11 = 11, All = 12
};
enum class Button : std::int32_t {
    A = 0, B = 1, Select = 2, Start = 3, Right = 4, Left = 5,
    Up = 6, Down = 7, R = 8, L = 9, X = 10, Y = 11
};
enum class ButtonFlags : std::uint16_t {
    None = 0x0000, A = 0x0001, B = 0x0002, Select = 0x0004,
    Start = 0x0008, Right = 0x0010, Left = 0x0020, Up = 0x0040,
    Down = 0x0080, R = 0x0100, L = 0x0200, X = 0x0400, Y = 0x0800
};
enum class PressFlags : std::uint16_t {
    None = 0x0000, Touch = 0x0001, Pressed = 0x0004,
    Released = 0x0008, Repeated = 0x0010
};
enum class ModelType : std::int32_t {
    Generic, Room, Item, Object, Placeholder, JumpPad, JumpPadBeam,
    Enemy, Player, Platform
};
enum class Team : std::int32_t { None, Orange, Green };
enum class Hunter : std::uint8_t {
    Samus = 0, Kanden = 1, Trace = 2, Sylux = 3, Noxus = 4, Spire = 5,
    Weavel = 6, Guardian = 7, Random = 8
};
enum class Language : std::uint8_t {
    English = 0, Japanese = 1, French = 2, Spanish = 3, German = 4, Italian = 5
};
enum class WaveFormat : std::int8_t { None = -1, PCM8 = 0, PCM16 = 1, ADPCM = 2 };
enum class TriggerType : std::uint32_t {
    Volume = 0, Threshold = 1, Relay = 2, Automatic = 3, StateBits = 4
};
enum class FhTriggerType : std::uint32_t {
    Sphere = 0, Box = 1, Cylinder = 2, Threshold = 3
};

// Cartridge message values are distinct from the smaller queue enum in
// messaging.hpp. Keep the format numbering complete for readers and entity
// decoders that need to preserve values not handled by the runtime yet.
enum class Message : std::uint32_t {
    None = 0, SetActive = 5, Destroyed = 6, Damage = 7, Trigger = 9,
    UpdateMusic = 12, Gravity = 15, Unlock = 16, Lock = 17, Activate = 18,
    Complete = 19, Impact = 20, Death = 21, Unused22 = 22, ShipHatch = 23,
    Unused24 = 24, Unused25 = 25, ShowPrompt = 26, ShowWarning = 27,
    ShowOverlay = 28, MoveItemSpawner = 29, SetCamSeqAi = 30,
    PlayerCollideWith = 31, BeamCollideWith = 32, UnlockConnectors = 33,
    LockConnectors = 34, PreventFormSwitch = 35, Gorea2Trigger = 36,
    SetTriggerState = 42, ClearTriggerState = 43, PlatformWakeup = 44,
    PlatformSleep = 45, DripMoatPlatform = 46, ActivateTurret = 48,
    DecreaseTurretLights = 49, IncreaseTurretLights = 50,
    DeactivateTurret = 51, SetBeamReflection = 52, SetPlatformIndex = 53,
    PlaySfxScript = 54, UnlockOubliette = 56, Checkpoint = 57,
    EscapeUpdate1 = 58, SetSeekPlayerY = 59, LoadOubliette = 60,
    EscapeUpdate2 = 61
};
enum class FhMessage : std::uint32_t {
    None = 0, Activate = 5, Destroyed = 6, Damage = 7, Trigger = 9,
    Gravity = 15, Unlock = 16, SetActive = 17, Complete = 18,
    Impact = 19, Death = 20, Unknown21 = 21
};
enum class EnemyType : std::uint8_t {
    WarWasp = 0, Zoomer = 1, Temroid = 2, Petrasyl1 = 3, Petrasyl2 = 4,
    Petrasyl3 = 5, Petrasyl4 = 6, Unknown7 = 7, Unknown8 = 8, Unknown9 = 9,
    BarbedWarWasp = 10, Shriekbat = 11, Geemer = 12, Unknown13 = 13,
    Unknown14 = 14, Unknown15 = 15, Blastcap = 16, Unknown17 = 17,
    AlimbicTurret = 18, Cretaphid = 19, CretaphidEye = 20,
    CretaphidCrystal = 21, Unknown22 = 22, PsychoBit1 = 23, Gorea1A = 24,
    GoreaHead = 25, GoreaArm = 26, GoreaLeg = 27, Gorea1B = 28,
    GoreaSealSphere1 = 29, Trocra = 30, Gorea2 = 31,
    GoreaSealSphere2 = 32, GoreaMeteor = 33, PsychoBit2 = 34,
    Voldrum2 = 35, Voldrum1 = 36, Quadtroid = 37, CrashPillar = 38,
    FireSpawn = 39, Spawner = 40, Slench = 41, SlenchShield = 42,
    SlenchNest = 43, SlenchSynapse = 44, SlenchTurret = 45,
    LesserIthrak = 46, GreaterIthrak = 47, Hunter = 48,
    ForceFieldLock = 49, HitZone = 50, CarnivorousPlant = 51
};
enum class FhEnemyType : std::uint32_t {
    WarWasp = 0, Zoomer = 1, Metroid = 2, Mochtroid1 = 3,
    Mochtroid2 = 4, Mochtroid3 = 5, Mochtroid4 = 6
};
enum class SingleType : std::int32_t {
    Death, Fuzzball, Lore, LoreDim, Enemy, EnemyDim,
    Object, ObjectDim, Equipment, EquipmentDim, Red, RedDim
};
enum class SaveWhen : std::int32_t { Never, Always, Prompt };
enum class EffElemFlags : std::uint32_t {
    None = 0x00000000, UseTransform = 0x00000001,
    UseAcceleration = 0x00000002, UseMesh = 0x00000004,
    SpawnUnitVecs = 0x00000008, KeepAlive = 0x00000010,
    ParticleExtension = 0x00000020, CheckCollision = 0x00000040,
    SpawnChildEffect = 0x00000080, DestroyOnDetach = 0x00000100,
    ElementExtension = 0x00080000, DrawEnabled = 0x00100000
};

} // namespace fruityprime::formats

namespace MphReadNative {
namespace Types = ::fruityprime::formats;
}

