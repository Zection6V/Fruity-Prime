#pragma once

// OpenTK.Mathematics, as the C# gets it from the OpenTK 4.9.4 package: the
// vector and matrix types and the members of them the port calls, written
// the way OpenTK writes them so every result rounds as the C#'s does.
// MphRead's own extensions on these types (Formats/Types.cs) stay in
// Formats/Types.hpp.

#include "../System/Exceptions.hpp"
#include "../System/Managed.hpp"

#include <any>
#include <concepts>
#include <array>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <iterator>
#include <memory>
#include <span>
#include <new>
#include <optional>
#include <stdexcept>
#include <string>
#include <string_view>
#include <type_traits>
#include <utility>
#include <vector>

namespace OpenTK::Mathematics
{
    struct Vector2
    {
        float X = 0.0F;
        float Y = 0.0F;

        constexpr Vector2() noexcept = default;
        constexpr Vector2(float x, float y) noexcept
            : X(x), Y(y)
        {
        }

        static const Vector2 Zero;
    };

    struct Vector3
    {
        float X = 0.0F;
        float Y = 0.0F;
        float Z = 0.0F;

        constexpr Vector3() noexcept = default;
        constexpr Vector3(float x, float y, float z) noexcept
            : X(x), Y(y), Z(z)
        {
        }

        [[nodiscard]] constexpr float LengthSquared() const noexcept
        {
            return (X * X) + (Y * Y) + (Z * Z);
        }

        [[nodiscard]] Vector3 Normalized() const;
        [[nodiscard]] std::string ToString() const;
        [[nodiscard]] static Vector3 Cross(Vector3 left, Vector3 right) noexcept;
        [[nodiscard]] static float Dot(Vector3 left, Vector3 right) noexcept;
        [[nodiscard]] static float Distance(Vector3 left, Vector3 right);

        static const Vector3 Zero;
        static const Vector3 UnitX;
        static const Vector3 UnitY;
        static const Vector3 UnitZ;
    };

    [[nodiscard]] constexpr Vector3 operator-(Vector3 value) noexcept
    {
        return Vector3(-value.X, -value.Y, -value.Z);
    }

    [[nodiscard]] constexpr Vector3 operator+(Vector3 left, Vector3 right) noexcept
    {
        return Vector3(left.X + right.X, left.Y + right.Y, left.Z + right.Z);
    }

    [[nodiscard]] constexpr Vector3 operator-(Vector3 left, Vector3 right) noexcept
    {
        return Vector3(left.X - right.X, left.Y - right.Y, left.Z - right.Z);
    }

    struct Vector3i
    {
        std::int32_t X = 0;
        std::int32_t Y = 0;
        std::int32_t Z = 0;

        constexpr Vector3i() noexcept = default;
        constexpr Vector3i(std::int32_t x, std::int32_t y, std::int32_t z) noexcept
            : X(x), Y(y), Z(z)
        {
        }
    };

    struct Vector4
    {
        float X = 0.0F;
        float Y = 0.0F;
        float Z = 0.0F;
        float W = 0.0F;

        constexpr Vector4() noexcept = default;
        constexpr Vector4(float x, float y, float z, float w) noexcept
            : X(x), Y(y), Z(z), W(w)
        {
        }
        constexpr explicit Vector4(Vector3 xyz) noexcept
            : X(xyz.X), Y(xyz.Y), Z(xyz.Z), W(0.0F)
        {
        }
        constexpr Vector4(Vector3 xyz, float w) noexcept
            : X(xyz.X), Y(xyz.Y), Z(xyz.Z), W(w)
        {
        }

        // Vector4.Dot(left, right).
        [[nodiscard]] static constexpr float Dot(Vector4 left, Vector4 right) noexcept
        {
            return left.X * right.X + left.Y * right.Y + left.Z * right.Z + left.W * right.W;
        }

        [[nodiscard]] constexpr Vector3 Xyz() const noexcept
        {
            return Vector3(X, Y, Z);
        }

        static const Vector4 Zero;
    };

    // OpenTK 4.9.4's MathHelper and the Vector3/Vector4 members the port calls
    // as free functions (Length(v), Normalize(v)), written once. Each file
    // used to carry its own copy, and some computed a different answer:
    // RadiansToDegrees from the literal 57.29577951F is one ULP above
    // OpenTK's 180f / MathF.PI, and Normalize by dividing each component is
    // not OpenTK's multiply by 1 / Length. Unqualified calls with a vector
    // argument find these by argument-dependent lookup.
    namespace MathHelper
    {
        inline constexpr float Pi = 3.1415927F;
        // MathHelper.Clamp(n, min, max): Math.Max(Math.Min(n, max), min), in
        // that order, so it is not Math.Clamp -- it never throws, and a -0
        // clamped at 0 comes out +0.
        [[nodiscard]] inline float Clamp(float n, float min, float max) noexcept
        {
            return ::MphRead::NativeRuntime::MathMax(::MphRead::NativeRuntime::MathMin(n, max), min);
        }
        inline constexpr float RadToDeg = 180.0F / Pi;
        inline constexpr float DegToRad = Pi / 180.0F;

        [[nodiscard]] constexpr float DegreesToRadians(float degrees) noexcept
        {
            return degrees * DegToRad;
        }

        [[nodiscard]] constexpr float RadiansToDegrees(float radians) noexcept
        {
            return radians * RadToDeg;
        }
    }

    [[nodiscard]] constexpr float LengthSquared(Vector3 value) noexcept
    {
        return value.LengthSquared();
    }

    [[nodiscard]] inline float Length(Vector3 value) noexcept
    {
        return std::sqrt(value.LengthSquared());
    }

    // Vector3.Normalize(vec).
    [[nodiscard]] inline Vector3 Normalize(Vector3 value) noexcept
    {
        const float scale = 1.0F / Length(value);
        value.X *= scale;
        value.Y *= scale;
        value.Z *= scale;
        return value;
    }

    [[nodiscard]] constexpr float LengthSquared(Vector4 value) noexcept
    {
        return (value.X * value.X) + (value.Y * value.Y) + (value.Z * value.Z) + (value.W * value.W);
    }

    [[nodiscard]] inline float Length(Vector4 value) noexcept
    {
        return std::sqrt(LengthSquared(value));
    }

    // Vector4.Normalize(vec).
    [[nodiscard]] inline Vector4 Normalize(Vector4 value) noexcept
    {
        const float scale = 1.0F / Length(value);
        value.X *= scale;
        value.Y *= scale;
        value.Z *= scale;
        value.W *= scale;
        return value;
    }

    struct Matrix3
    {
        float M11 = 0.0F;
        float M12 = 0.0F;
        float M13 = 0.0F;
        float M21 = 0.0F;
        float M22 = 0.0F;
        float M23 = 0.0F;
        float M31 = 0.0F;
        float M32 = 0.0F;
        float M33 = 0.0F;

        constexpr Matrix3() noexcept = default;
        constexpr Matrix3(
            float m11, float m12, float m13,
            float m21, float m22, float m23,
            float m31, float m32, float m33) noexcept
            : M11(m11), M12(m12), M13(m13),
              M21(m21), M22(m22), M23(m23),
              M31(m31), M32(m32), M33(m33)
        {
        }
        constexpr Matrix3(Vector3 row0, Vector3 row1, Vector3 row2) noexcept
            : Matrix3(
                row0.X, row0.Y, row0.Z,
                row1.X, row1.Y, row1.Z,
                row2.X, row2.Y, row2.Z)
        {
        }

        [[nodiscard]] constexpr Vector3 Row0() const noexcept { return Vector3(M11, M12, M13); }
        [[nodiscard]] constexpr Vector3 Row1() const noexcept { return Vector3(M21, M22, M23); }
        [[nodiscard]] constexpr Vector3 Row2() const noexcept { return Vector3(M31, M32, M33); }
    };

    struct Matrix4x3
    {
        float M11 = 0.0F;
        float M12 = 0.0F;
        float M13 = 0.0F;
        float M21 = 0.0F;
        float M22 = 0.0F;
        float M23 = 0.0F;
        float M31 = 0.0F;
        float M32 = 0.0F;
        float M33 = 0.0F;
        float M41 = 0.0F;
        float M42 = 0.0F;
        float M43 = 0.0F;

        constexpr Matrix4x3() noexcept = default;
        constexpr Matrix4x3(Vector3 row0, Vector3 row1, Vector3 row2, Vector3 row3) noexcept
            : M11(row0.X), M12(row0.Y), M13(row0.Z),
              M21(row1.X), M22(row1.Y), M23(row1.Z),
              M31(row2.X), M32(row2.Y), M33(row2.Z),
              M41(row3.X), M42(row3.Y), M43(row3.Z)
        {
        }

        [[nodiscard]] constexpr Vector3 Row0() const noexcept { return Vector3(M11, M12, M13); }
        [[nodiscard]] constexpr Vector3 Row1() const noexcept { return Vector3(M21, M22, M23); }
        [[nodiscard]] constexpr Vector3 Row2() const noexcept { return Vector3(M31, M32, M33); }
        [[nodiscard]] constexpr Vector3 Row3() const noexcept { return Vector3(M41, M42, M43); }

        static const Matrix4x3 Zero;
    };

    struct Matrix4
    {
        float M11 = 0.0F;
        float M12 = 0.0F;
        float M13 = 0.0F;
        float M14 = 0.0F;
        float M21 = 0.0F;
        float M22 = 0.0F;
        float M23 = 0.0F;
        float M24 = 0.0F;
        float M31 = 0.0F;
        float M32 = 0.0F;
        float M33 = 0.0F;
        float M34 = 0.0F;
        float M41 = 0.0F;
        float M42 = 0.0F;
        float M43 = 0.0F;
        float M44 = 0.0F;

        constexpr Matrix4() noexcept = default;
        constexpr Matrix4(Vector4 row0, Vector4 row1, Vector4 row2, Vector4 row3) noexcept
            : M11(row0.X), M12(row0.Y), M13(row0.Z), M14(row0.W),
              M21(row1.X), M22(row1.Y), M23(row1.Z), M24(row1.W),
              M31(row2.X), M32(row2.Y), M33(row2.Z), M34(row2.W),
              M41(row3.X), M42(row3.Y), M43(row3.Z), M44(row3.W)
        {
        }

        // Matrix4.LookAt(eye, target, up): a view matrix, every axis normalised.
        [[nodiscard]] static Matrix4 LookAt(Vector3 eye, Vector3 target, Vector3 up);
        // Matrix4.Transpose(mat).
        [[nodiscard]] static Matrix4 Transpose(const Matrix4& value) noexcept;
        // Matrix4.CreatePerspectiveFieldOfView / CreatePerspectiveOffCenter,
        // with OpenTK's ArgumentOutOfRangeException for a field of view outside
        // (0, pi] and for a depth that is not positive or not increasing.
        [[nodiscard]] static Matrix4 CreatePerspectiveFieldOfView(
            float fovy, float aspect, float depthNear, float depthFar);
        [[nodiscard]] static Matrix4 CreatePerspectiveOffCenter(
            float left, float right, float bottom, float top, float depthNear, float depthFar);
        // Matrix4.CreateOrthographic / CreateOrthographicOffCenter.
        [[nodiscard]] static Matrix4 CreateOrthographic(
            float width, float height, float depthNear, float depthFar) noexcept;
        [[nodiscard]] static Matrix4 CreateOrthographicOffCenter(
            float left, float right, float bottom, float top, float depthNear, float depthFar) noexcept;
        // matrix.ClearTranslation(): Row3's xyz zeroed.
        [[nodiscard]] Matrix4 ClearTranslation() const noexcept;
        // matrix.ExtractScale(): the lengths of the upper three rows' xyz.
        [[nodiscard]] Vector3 ExtractScale() const;

        [[nodiscard]] constexpr Vector4 Row0() const noexcept { return Vector4(M11, M12, M13, M14); }
        [[nodiscard]] constexpr Vector4 Row1() const noexcept { return Vector4(M21, M22, M23, M24); }
        [[nodiscard]] constexpr Vector4 Row2() const noexcept { return Vector4(M31, M32, M33, M34); }
        [[nodiscard]] constexpr Vector4 Row3() const noexcept { return Vector4(M41, M42, M43, M44); }

        // OpenTK's Matrix4 * Matrix4 (Matrix4.Mult): the full row-major product.
        [[nodiscard]] friend constexpr Matrix4 operator*(const Matrix4& left, const Matrix4& right) noexcept
        {
            Matrix4 result{};
            result.M11 = left.M11 * right.M11 + left.M12 * right.M21
                + left.M13 * right.M31 + left.M14 * right.M41;
            result.M12 = left.M11 * right.M12 + left.M12 * right.M22
                + left.M13 * right.M32 + left.M14 * right.M42;
            result.M13 = left.M11 * right.M13 + left.M12 * right.M23
                + left.M13 * right.M33 + left.M14 * right.M43;
            result.M14 = left.M11 * right.M14 + left.M12 * right.M24
                + left.M13 * right.M34 + left.M14 * right.M44;
            result.M21 = left.M21 * right.M11 + left.M22 * right.M21
                + left.M23 * right.M31 + left.M24 * right.M41;
            result.M22 = left.M21 * right.M12 + left.M22 * right.M22
                + left.M23 * right.M32 + left.M24 * right.M42;
            result.M23 = left.M21 * right.M13 + left.M22 * right.M23
                + left.M23 * right.M33 + left.M24 * right.M43;
            result.M24 = left.M21 * right.M14 + left.M22 * right.M24
                + left.M23 * right.M34 + left.M24 * right.M44;
            result.M31 = left.M31 * right.M11 + left.M32 * right.M21
                + left.M33 * right.M31 + left.M34 * right.M41;
            result.M32 = left.M31 * right.M12 + left.M32 * right.M22
                + left.M33 * right.M32 + left.M34 * right.M42;
            result.M33 = left.M31 * right.M13 + left.M32 * right.M23
                + left.M33 * right.M33 + left.M34 * right.M43;
            result.M34 = left.M31 * right.M14 + left.M32 * right.M24
                + left.M33 * right.M34 + left.M34 * right.M44;
            result.M41 = left.M41 * right.M11 + left.M42 * right.M21
                + left.M43 * right.M31 + left.M44 * right.M41;
            result.M42 = left.M41 * right.M12 + left.M42 * right.M22
                + left.M43 * right.M32 + left.M44 * right.M42;
            result.M43 = left.M41 * right.M13 + left.M42 * right.M23
                + left.M43 * right.M33 + left.M44 * right.M43;
            result.M44 = left.M41 * right.M14 + left.M42 * right.M24
                + left.M43 * right.M34 + left.M44 * right.M44;
            return result;
        }

        friend constexpr Matrix4& operator*=(Matrix4& left, const Matrix4& right) noexcept
        {
            left = left * right;
            return left;
        }

        static const Matrix4 Zero;
    };

    // The OpenTK members and operators that the port spells as free
    // functions: Multiply(v, s) for v * s, CreateRotationY(a) for
    // Matrix4.CreateRotationY. Written once, as OpenTK 4.9.4 writes them;
    // every file used to carry its own, and ClearScale in three of them
    // divided each row by its length where OpenTK's Normalized multiplies by
    // 1 / Length. The vector and matrix ones are found by argument-dependent
    // lookup; the ones taking only floats need a using.
    [[nodiscard]] constexpr Vector2 Add(Vector2 left, Vector2 right) noexcept
    {
        return Vector2(left.X + right.X, left.Y + right.Y);
    }

    [[nodiscard]] constexpr Vector2 Subtract(Vector2 left, Vector2 right) noexcept
    {
        return Vector2(left.X - right.X, left.Y - right.Y);
    }

    [[nodiscard]] constexpr Vector2 Multiply(Vector2 value, float scale) noexcept
    {
        return Vector2(value.X * scale, value.Y * scale);
    }

    [[nodiscard]] constexpr Vector3 Add(Vector3 left, Vector3 right) noexcept
    {
        return left + right;
    }

    [[nodiscard]] constexpr Vector3 Subtract(Vector3 left, Vector3 right) noexcept
    {
        return left - right;
    }

    [[nodiscard]] constexpr Vector3 Negate(Vector3 value) noexcept
    {
        return -value;
    }

    // Vector3 * float, float * Vector3.
    [[nodiscard]] constexpr Vector3 Multiply(Vector3 value, float scale) noexcept
    {
        return Vector3(value.X * scale, value.Y * scale, value.Z * scale);
    }

    [[nodiscard]] constexpr Vector3 Multiply(float scale, Vector3 value) noexcept
    {
        return Multiply(value, scale);
    }

    [[nodiscard]] constexpr Vector3 Scale(Vector3 value, float scale) noexcept
    {
        return Multiply(value, scale);
    }

    [[nodiscard]] constexpr Vector3 ScaleVector(Vector3 value, float scale) noexcept
    {
        return Multiply(value, scale);
    }

    [[nodiscard]] constexpr Vector4 ScaleVector(Vector4 value, float scale) noexcept
    {
        return Vector4(value.X * scale, value.Y * scale, value.Z * scale, value.W * scale);
    }

    // Vector3 / float: OpenTK divides each component, it does not multiply
    // by the reciprocal.
    [[nodiscard]] constexpr Vector3 Divide(Vector3 value, float scale) noexcept
    {
        return Vector3(value.X / scale, value.Y / scale, value.Z / scale);
    }

    // Vector3 == Vector3.
    [[nodiscard]] constexpr bool Equal(Vector3 left, Vector3 right) noexcept
    {
        return left.X == right.X && left.Y == right.Y && left.Z == right.Z;
    }

    // value == Vector3.Zero, so -0 is zero.
    [[nodiscard]] constexpr bool IsZero(Vector3 value) noexcept
    {
        return value.X == 0.0F && value.Y == 0.0F && value.Z == 0.0F;
    }

    // Vector3 * Matrix3 (Vector3.TransformRow).
    [[nodiscard]] constexpr Vector3 Multiply(Vector3 value, Matrix3 matrix) noexcept
    {
        return Vector3(
            value.X * matrix.M11 + value.Y * matrix.M21 + value.Z * matrix.M31,
            value.X * matrix.M12 + value.Y * matrix.M22 + value.Z * matrix.M32,
            value.X * matrix.M13 + value.Y * matrix.M23 + value.Z * matrix.M33);
    }

    [[nodiscard]] constexpr Matrix4 Multiply(Matrix4 left, Matrix4 right) noexcept
    {
        return left * right;
    }

    // Matrix4.Mult(matrix, scale): every element.
    [[nodiscard]] constexpr Matrix4 Multiply(Matrix4 value, float scale) noexcept
    {
        value.M11 *= scale; value.M12 *= scale; value.M13 *= scale; value.M14 *= scale;
        value.M21 *= scale; value.M22 *= scale; value.M23 *= scale; value.M24 *= scale;
        value.M31 *= scale; value.M32 *= scale; value.M33 *= scale; value.M34 *= scale;
        value.M41 *= scale; value.M42 *= scale; value.M43 *= scale; value.M44 *= scale;
        return value;
    }

    [[nodiscard]] constexpr bool Equal(const Matrix4& left, const Matrix4& right) noexcept
    {
        return left.M11 == right.M11 && left.M12 == right.M12 && left.M13 == right.M13 && left.M14 == right.M14
            && left.M21 == right.M21 && left.M22 == right.M22 && left.M23 == right.M23 && left.M24 == right.M24
            && left.M31 == right.M31 && left.M32 == right.M32 && left.M33 == right.M33 && left.M34 == right.M34
            && left.M41 == right.M41 && left.M42 == right.M42 && left.M43 == right.M43 && left.M44 == right.M44;
    }

    // Matrix4.Identity.
    [[nodiscard]] constexpr Matrix4 IdentityMatrix() noexcept
    {
        return Matrix4(
            Vector4(1.0F, 0.0F, 0.0F, 0.0F),
            Vector4(0.0F, 1.0F, 0.0F, 0.0F),
            Vector4(0.0F, 0.0F, 1.0F, 0.0F),
            Vector4(0.0F, 0.0F, 0.0F, 1.0F));
    }

    // Matrix4.CreateScale(x, y, z), (Vector3), (float).
    [[nodiscard]] constexpr Matrix4 CreateScale(float x, float y, float z) noexcept
    {
        Matrix4 result = IdentityMatrix();
        result.M11 = x;
        result.M22 = y;
        result.M33 = z;
        return result;
    }

    [[nodiscard]] constexpr Matrix4 CreateScale(Vector3 scale) noexcept
    {
        return CreateScale(scale.X, scale.Y, scale.Z);
    }

    [[nodiscard]] constexpr Matrix4 CreateScale(float scale) noexcept
    {
        return CreateScale(scale, scale, scale);
    }

    // Matrix4.CreateTranslation(x, y, z), (Vector3).
    [[nodiscard]] constexpr Matrix4 CreateTranslation(float x, float y, float z) noexcept
    {
        Matrix4 result = IdentityMatrix();
        result.M41 = x;
        result.M42 = y;
        result.M43 = z;
        return result;
    }

    [[nodiscard]] constexpr Matrix4 CreateTranslation(Vector3 position) noexcept
    {
        return CreateTranslation(position.X, position.Y, position.Z);
    }

    // Matrix4.CreateRotationY(angle).
    [[nodiscard]] inline Matrix4 CreateRotationY(float angle) noexcept
    {
        const float cos = std::cos(angle);
        const float sin = std::sin(angle);
        Matrix4 result = IdentityMatrix();
        result.M11 = cos;
        result.M13 = -sin;
        result.M31 = sin;
        result.M33 = cos;
        return result;
    }

    // matrix.ClearScale(): the upper three rows' xyz, each Normalized().
    [[nodiscard]] inline Matrix4 ClearScale(Matrix4 value) noexcept
    {
        const Vector3 row0 = Normalize(Vector3(value.M11, value.M12, value.M13));
        const Vector3 row1 = Normalize(Vector3(value.M21, value.M22, value.M23));
        const Vector3 row2 = Normalize(Vector3(value.M31, value.M32, value.M33));
        value.M11 = row0.X; value.M12 = row0.Y; value.M13 = row0.Z;
        value.M21 = row1.X; value.M22 = row1.Y; value.M23 = row1.Z;
        value.M31 = row2.X; value.M32 = row2.Y; value.M33 = row2.Z;
        return value;
    }

    // matrix.Row3.Xyz = value.
    constexpr void SetRow3(Matrix4& matrix, Vector3 value) noexcept
    {
        matrix.M41 = value.X;
        matrix.M42 = value.Y;
        matrix.M43 = value.Z;
    }

    // Vector3.Clamp(vec, min, max): each component, NaN passing through.
    [[nodiscard]] constexpr Vector3 Clamp(Vector3 value, Vector3 min, Vector3 max) noexcept
    {
        value.X = value.X < min.X ? min.X : value.X > max.X ? max.X : value.X;
        value.Y = value.Y < min.Y ? min.Y : value.Y > max.Y ? max.Y : value.Y;
        value.Z = value.Z < min.Z ? min.Z : value.Z > max.Z ? max.Z : value.Z;
        return value;
    }

    // Vector3.ComponentMin/ComponentMax: a plain comparison per component,
    // not Math.Min -- so a NaN or a signed zero in b is not special.
    [[nodiscard]] constexpr Vector3 ComponentMin(Vector3 a, Vector3 b) noexcept
    {
        a.X = a.X < b.X ? a.X : b.X;
        a.Y = a.Y < b.Y ? a.Y : b.Y;
        a.Z = a.Z < b.Z ? a.Z : b.Z;
        return a;
    }

    [[nodiscard]] constexpr Vector3 ComponentMax(Vector3 a, Vector3 b) noexcept
    {
        a.X = a.X > b.X ? a.X : b.X;
        a.Y = a.Y > b.Y ? a.Y : b.Y;
        a.Z = a.Z > b.Z ? a.Z : b.Z;
        return a;
    }

    // Vector3.DistanceSquared(vec1, vec2).
    [[nodiscard]] constexpr float DistanceSquared(Vector3 vec1, Vector3 vec2) noexcept
    {
        return ((vec2.X - vec1.X) * (vec2.X - vec1.X))
            + ((vec2.Y - vec1.Y) * (vec2.Y - vec1.Y))
            + ((vec2.Z - vec1.Z) * (vec2.Z - vec1.Z));
    }

    // Matrix4.CreateRotationX(angle), CreateRotationZ(angle).
    [[nodiscard]] inline Matrix4 CreateRotationX(float angle) noexcept
    {
        const float cos = std::cos(angle);
        const float sin = std::sin(angle);
        Matrix4 result = IdentityMatrix();
        result.M22 = cos;
        result.M23 = sin;
        result.M32 = -sin;
        result.M33 = cos;
        return result;
    }

    [[nodiscard]] inline Matrix4 CreateRotationZ(float angle) noexcept
    {
        const float cos = std::cos(angle);
        const float sin = std::sin(angle);
        Matrix4 result = IdentityMatrix();
        result.M11 = cos;
        result.M12 = sin;
        result.M21 = -sin;
        result.M22 = cos;
        return result;
    }

    // Matrix4.CreateFromAxisAngle(axis, angle), in OpenTK's order: the axis
    // normalised, the angle negated, t * x * x rather than x * x * t.
    [[nodiscard]] Matrix4 CreateFromAxisAngle(Vector3 axis, float angle) noexcept;

    // matrix.Determinant, term for term as OpenTK sums it.
    [[nodiscard]] float Determinant(const Matrix4& matrix) noexcept;

    // Matrix4.Invert(mat): throws InvalidOperationException for a singular
    // matrix. OpenTK takes its SSE3 path on x86 and x64 and its scalar
    // fallback elsewhere, and the two round differently; this does the same
    // arithmetic in the same order as whichever of them the C# would run on
    // this processor.
    [[nodiscard]] Matrix4 Invert(const Matrix4& matrix);

    // matrix.Inverted(): the inverse, or the matrix itself when its
    // determinant is zero.
    [[nodiscard]] Matrix4 Inverted(const Matrix4& matrix);

    static_assert(std::is_standard_layout_v<Vector2> && sizeof(Vector2) == 8);
    static_assert(std::is_standard_layout_v<Vector3> && sizeof(Vector3) == 12);
    static_assert(std::is_standard_layout_v<Vector3i> && sizeof(Vector3i) == 12);
    static_assert(std::is_standard_layout_v<Vector4> && sizeof(Vector4) == 16);
    static_assert(std::is_standard_layout_v<Matrix3> && sizeof(Matrix3) == 36);
    static_assert(offsetof(Matrix3, M11) == 0 && offsetof(Matrix3, M21) == 12
        && offsetof(Matrix3, M31) == 24);
    static_assert(std::is_standard_layout_v<Matrix4x3> && sizeof(Matrix4x3) == 48);
    static_assert(offsetof(Matrix4x3, M11) == 0 && offsetof(Matrix4x3, M21) == 12
        && offsetof(Matrix4x3, M31) == 24 && offsetof(Matrix4x3, M41) == 36);
    static_assert(std::is_standard_layout_v<Matrix4> && sizeof(Matrix4) == 64);
    static_assert(offsetof(Matrix4, M11) == 0 && offsetof(Matrix4, M21) == 16
        && offsetof(Matrix4, M31) == 32 && offsetof(Matrix4, M41) == 48);
}
