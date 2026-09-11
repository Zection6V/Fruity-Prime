#pragma once

#include "Enums.hpp"

#include <any>
#include <cstddef>
#include <cstdint>
#include <memory>
#include <new>
#include <optional>
#include <string>
#include <string_view>
#include <type_traits>
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

        [[nodiscard]] Vector3 Normalized() const;
        [[nodiscard]] static Vector3 Cross(Vector3 left, Vector3 right) noexcept;
        [[nodiscard]] static float Dot(Vector3 left, Vector3 right) noexcept;
        [[nodiscard]] static float Distance(Vector3 left, Vector3 right);

        static const Vector3 Zero;
    };

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

        [[nodiscard]] constexpr Vector3 Xyz() const noexcept
        {
            return Vector3(X, Y, Z);
        }

        static const Vector4 Zero;
    };

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

        [[nodiscard]] constexpr Vector4 Row0() const noexcept { return Vector4(M11, M12, M13, M14); }
        [[nodiscard]] constexpr Vector4 Row1() const noexcept { return Vector4(M21, M22, M23, M24); }
        [[nodiscard]] constexpr Vector4 Row2() const noexcept { return Vector4(M31, M32, M33, M34); }
        [[nodiscard]] constexpr Vector4 Row3() const noexcept { return Vector4(M41, M42, M43, M44); }

        static const Matrix4 Zero;
    };

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

namespace MphRead
{
    struct LightInfo
    {
        const OpenTK::Mathematics::Vector3 Light1Vector{};
        const OpenTK::Mathematics::Vector3 Light1Color{};
        const OpenTK::Mathematics::Vector3 Light2Vector{};
        const OpenTK::Mathematics::Vector3 Light2Color{};

        static const LightInfo Zero;

        constexpr LightInfo() noexcept = default;
        constexpr LightInfo(
            OpenTK::Mathematics::Vector3 light1Vector,
            OpenTK::Mathematics::Vector3 light1Color,
            OpenTK::Mathematics::Vector3 light2Vector,
            OpenTK::Mathematics::Vector3 light2Color) noexcept
            : Light1Vector(light1Vector),
              Light1Color(light1Color),
              Light2Vector(light2Vector),
              Light2Color(light2Color)
        {
        }

        LightInfo(const LightInfo&) noexcept = default;
        LightInfo& operator=(const LightInfo& other) noexcept;
    };

    enum class RenderItemType : std::int32_t
    {
        Mesh = 0,
        Box = 1,
        Cylinder = 2,
        Sphere = 3,
        Quad = 4,
        Ngon = 5,
        Particle = 6,
        TrailSingle = 7,
        TrailMulti = 8,
        TrailStack = 9
    };

    template <typename T>
    class ManagedArray final
    {
    public:
        ManagedArray() = default;
        explicit ManagedArray(std::size_t length)
            : _values(length)
        {
        }

        [[nodiscard]] std::size_t Length() const noexcept { return _values.size(); }
        [[nodiscard]] T& operator[](std::size_t index) { return _values.at(index); }
        [[nodiscard]] const T& operator[](std::size_t index) const { return _values.at(index); }
        [[nodiscard]] T* Data() noexcept { return _values.data(); }
        [[nodiscard]] const T* Data() const noexcept { return _values.data(); }

        [[nodiscard]] static std::shared_ptr<ManagedArray<T>> Empty()
        {
            static const auto empty = std::make_shared<ManagedArray<T>>();
            return empty;
        }

    private:
        std::vector<T> _values;
    };

    class RenderItem
    {
    public:
        RenderItemType Type = RenderItemType::Mesh;
        std::int32_t PolygonId = 0;
        float Alpha = 0.0F;
        MphRead::PolygonMode PolygonMode = MphRead::PolygonMode::Modulate;
        MphRead::RenderMode RenderMode = MphRead::RenderMode::Normal;
        MphRead::CullingMode CullingMode = MphRead::CullingMode::Neither;
        MphRead::BillboardMode BillboardMode = MphRead::BillboardMode::None;
        bool Wireframe = false;
        bool Lighting = false;
        bool NoLines = false;
        OpenTK::Mathematics::Vector3 Diffuse{};
        OpenTK::Mathematics::Vector3 Ambient{};
        OpenTK::Mathematics::Vector3 Specular{};
        OpenTK::Mathematics::Vector3 Emission{};
        MphRead::LightInfo LightInfo{};
        MphRead::TexgenMode TexgenMode = MphRead::TexgenMode::None;
        RepeatMode XRepeat = RepeatMode::Clamp;
        RepeatMode YRepeat = RepeatMode::Clamp;
        bool HasTexture = false;
        std::int32_t TextureBindingId = 0;
        OpenTK::Mathematics::Matrix4 TexcoordMatrix{};
        OpenTK::Mathematics::Matrix4 Transform{};
        std::int32_t ListId = 0;
        std::int32_t MatrixStackCount = 0;
        const std::shared_ptr<ManagedArray<float>> MatrixStack;
        std::optional<OpenTK::Mathematics::Vector4> OverrideColor{};
        std::optional<OpenTK::Mathematics::Vector4> PaletteOverride{};
        std::shared_ptr<ManagedArray<OpenTK::Mathematics::Vector3>> Points;
        std::int32_t ItemCount = 0;
        float ScaleS = 0.0F;
        float ScaleT = 0.0F;

        RenderItem();
        RenderItem(const RenderItem&) = delete;
        RenderItem& operator=(const RenderItem&) = delete;
        RenderItem(RenderItem&&) = delete;
        RenderItem& operator=(RenderItem&&) = delete;
    };

    struct Fixed
    {
        const std::int32_t Value = 0;

        constexpr Fixed() noexcept = default;
        constexpr explicit Fixed(std::int32_t value) noexcept
            : Value(value)
        {
        }

        Fixed(const Fixed&) noexcept = default;
        Fixed& operator=(const Fixed& other) noexcept;

        [[nodiscard]] float FloatValue() const noexcept;
        [[nodiscard]] static float ToFloat(std::int64_t value) noexcept;
        [[nodiscard]] static float ToFloat(std::uint32_t value) noexcept;
        [[nodiscard]] static float ToFloat(std::int32_t value) noexcept;
        [[nodiscard]] static float ToFloat(std::string_view value);
        [[nodiscard]] static std::int32_t ToInt(float value) noexcept;
        [[nodiscard]] std::string ToString() const;
    };

    struct Vector3Fx
    {
        const Fixed X{};
        const Fixed Y{};
        const Fixed Z{};

        constexpr Vector3Fx() noexcept = default;
        constexpr Vector3Fx(std::int32_t x, std::int32_t y, std::int32_t z) noexcept
            : X(x), Y(y), Z(z)
        {
        }
        Vector3Fx(std::string_view x, std::string_view y, std::string_view z);

        Vector3Fx(const Vector3Fx&) noexcept = default;
        Vector3Fx& operator=(const Vector3Fx& other) noexcept;

        [[nodiscard]] OpenTK::Mathematics::Vector3 ToFloatVector() const noexcept;
        [[nodiscard]] OpenTK::Mathematics::Vector3i ToIntVector() const noexcept;
    };

    struct Vector4Fx
    {
        const Fixed X{};
        const Fixed Y{};
        const Fixed Z{};
        const Fixed W{};

        constexpr Vector4Fx() noexcept = default;
        Vector4Fx(const Vector4Fx&) noexcept = default;
        Vector4Fx& operator=(const Vector4Fx& other) noexcept;

        [[nodiscard]] OpenTK::Mathematics::Vector4 ToFloatVector() const noexcept;
    };

    struct Matrix43Fx
    {
        const Vector3Fx One{};
        const Vector3Fx Two{};
        const Vector3Fx Three{};
        const Vector3Fx Four{};

        constexpr Matrix43Fx() noexcept = default;
        Matrix43Fx(const Matrix43Fx&) noexcept = default;
        Matrix43Fx& operator=(const Matrix43Fx& other) noexcept;
    };

    struct Matrix44Fx
    {
        const Vector4Fx One{};
        const Vector4Fx Two{};
        const Vector4Fx Three{};
        const Vector4Fx Four{};

        constexpr Matrix44Fx() noexcept = default;
        Matrix44Fx(const Matrix44Fx&) noexcept = default;
        Matrix44Fx& operator=(const Matrix44Fx& other) noexcept;

        [[nodiscard]] OpenTK::Mathematics::Matrix4 ToFloatMatrix() const noexcept;
    };

    class Matrix final
    {
    public:
        Matrix() = delete;

        [[nodiscard]] static OpenTK::Mathematics::Matrix3 GetTransform3(
            OpenTK::Mathematics::Vector3 vector1,
            OpenTK::Mathematics::Vector3 vector2);
        [[nodiscard]] static OpenTK::Mathematics::Matrix4 GetTransform4(
            OpenTK::Mathematics::Vector3 vector1,
            OpenTK::Mathematics::Vector3 vector2,
            OpenTK::Mathematics::Vector3 position);
        [[nodiscard]] static OpenTK::Mathematics::Matrix4 GetTransformSRT(
            OpenTK::Mathematics::Vector3 scale,
            OpenTK::Mathematics::Vector3 angle,
            OpenTK::Mathematics::Vector3 position);
        [[nodiscard]] static OpenTK::Mathematics::Vector3 Vec3MultMtx4(
            OpenTK::Mathematics::Vector3 vec,
            OpenTK::Mathematics::Matrix4 mat) noexcept;
        [[nodiscard]] static OpenTK::Mathematics::Vector3 Vec3MultMtx3(
            OpenTK::Mathematics::Vector3 vec,
            OpenTK::Mathematics::Matrix4 mat) noexcept;
        [[nodiscard]] static OpenTK::Mathematics::Vector3 Vec4MultMtx4x3(
            OpenTK::Mathematics::Vector4 vec,
            OpenTK::Mathematics::Matrix4x3 mat) noexcept;
        [[nodiscard]] static OpenTK::Mathematics::Matrix4x3 Concat43(
            OpenTK::Mathematics::Matrix4x3 first,
            OpenTK::Mathematics::Matrix4x3 second) noexcept;
        [[nodiscard]] static OpenTK::Mathematics::Matrix4 Multiply44(
            OpenTK::Mathematics::Matrix4 first,
            OpenTK::Mathematics::Matrix4 second) noexcept;
        [[nodiscard]] static OpenTK::Mathematics::Matrix3 RotateAlign(
            OpenTK::Mathematics::Vector3 from,
            OpenTK::Mathematics::Vector3 to) noexcept;
        [[nodiscard]] static float ProjectPosition(
            OpenTK::Mathematics::Vector3 pos,
            OpenTK::Mathematics::Matrix4 viewMatrix,
            OpenTK::Mathematics::Matrix4 projectionMtx,
            OpenTK::Mathematics::Vector2& dest) noexcept;
        static void GetProjectedValues(
            OpenTK::Mathematics::Vector3 pos,
            OpenTK::Mathematics::Vector3 camPos,
            OpenTK::Mathematics::Matrix4 viewMatrix,
            OpenTK::Mathematics::Matrix4 projectionMtx,
            float& dist,
            float& depth,
            float& scaleInv,
            OpenTK::Mathematics::Vector3& target,
            OpenTK::Mathematics::Vector2& screenPos);
    };

    struct ColorRgb
    {
        const std::uint8_t Red = 0;
        const std::uint8_t Green = 0;
        const std::uint8_t Blue = 0;

        constexpr ColorRgb() noexcept = default;
        constexpr ColorRgb(std::uint8_t red, std::uint8_t green, std::uint8_t blue) noexcept
            : Red(red), Green(green), Blue(blue)
        {
        }

        ColorRgb(const ColorRgb&) noexcept = default;
        ColorRgb& operator=(const ColorRgb& other) noexcept;

        [[nodiscard]] OpenTK::Mathematics::Vector3 AsVector3() const noexcept;
        [[nodiscard]] OpenTK::Mathematics::Vector4 AsVector4(float alpha = 1.0F) const noexcept;
        [[nodiscard]] bool Equals(const std::any& obj) const;
        [[nodiscard]] std::int32_t GetHashCode() const;
    };

    [[nodiscard]] OpenTK::Mathematics::Vector3 operator/(ColorRgb left, float right) noexcept;
    [[nodiscard]] constexpr bool operator==(ColorRgb lhs, ColorRgb rhs) noexcept
    {
        return lhs.Red == rhs.Red && lhs.Green == rhs.Green && lhs.Blue == rhs.Blue;
    }
    [[nodiscard]] constexpr bool operator!=(ColorRgb lhs, ColorRgb rhs) noexcept
    {
        return lhs.Red != rhs.Red || lhs.Green != rhs.Green || lhs.Blue != rhs.Blue;
    }

    struct ColorRgba
    {
        const std::uint8_t Red = 0;
        const std::uint8_t Green = 0;
        const std::uint8_t Blue = 0;
        const std::uint8_t Alpha = 0;

        constexpr ColorRgba() noexcept = default;
        constexpr ColorRgba(
            std::uint8_t red, std::uint8_t green,
            std::uint8_t blue, std::uint8_t alpha) noexcept
            : Red(red), Green(green), Blue(blue), Alpha(alpha)
        {
        }
        explicit ColorRgba(std::uint32_t value, std::uint8_t alpha = 255) noexcept;

        ColorRgba(const ColorRgba&) noexcept = default;
        ColorRgba& operator=(const ColorRgba& other) noexcept;

        [[nodiscard]] ColorRgba WithAlpha(std::uint8_t alpha) const noexcept;
        [[nodiscard]] std::uint32_t ToUint() const noexcept;
        [[nodiscard]] bool Equals(const std::any& obj) const;
        [[nodiscard]] std::int32_t GetHashCode() const;
    };

    [[nodiscard]] constexpr bool operator==(ColorRgba lhs, ColorRgba rhs) noexcept
    {
        return lhs.Red == rhs.Red && lhs.Green == rhs.Green
            && lhs.Blue == rhs.Blue && lhs.Alpha == rhs.Alpha;
    }
    [[nodiscard]] constexpr bool operator!=(ColorRgba lhs, ColorRgba rhs) noexcept
    {
        return lhs.Red != rhs.Red || lhs.Green != rhs.Green
            || lhs.Blue != rhs.Blue || lhs.Alpha != rhs.Alpha;
    }

    class TypeExtensions final
    {
    public:
        TypeExtensions() = delete;

        [[nodiscard]] static OpenTK::Mathematics::Vector2 WithX(
            OpenTK::Mathematics::Vector2 vector, float x) noexcept;
        [[nodiscard]] static OpenTK::Mathematics::Vector2 WithY(
            OpenTK::Mathematics::Vector2 vector, float y) noexcept;
        [[nodiscard]] static OpenTK::Mathematics::Vector2 AddX(
            OpenTK::Mathematics::Vector2 vector, float x) noexcept;
        [[nodiscard]] static OpenTK::Mathematics::Vector2 AddY(
            OpenTK::Mathematics::Vector2 vector, float y) noexcept;

        [[nodiscard]] static OpenTK::Mathematics::Vector3 WithX(
            OpenTK::Mathematics::Vector3 vector, float x) noexcept;
        [[nodiscard]] static OpenTK::Mathematics::Vector3 WithY(
            OpenTK::Mathematics::Vector3 vector, float y) noexcept;
        [[nodiscard]] static OpenTK::Mathematics::Vector3 WithZ(
            OpenTK::Mathematics::Vector3 vector, float z) noexcept;
        [[nodiscard]] static OpenTK::Mathematics::Vector3 AddX(
            OpenTK::Mathematics::Vector3 vector, float x) noexcept;
        [[nodiscard]] static OpenTK::Mathematics::Vector3 AddY(
            OpenTK::Mathematics::Vector3 vector, float y) noexcept;
        [[nodiscard]] static OpenTK::Mathematics::Vector3 AddZ(
            OpenTK::Mathematics::Vector3 vector, float z) noexcept;
        [[nodiscard]] static OpenTK::Mathematics::Vector3i ToFixedVector(
            OpenTK::Mathematics::Vector3 vector) noexcept;
        [[nodiscard]] static Vector3Fx ToVector3Fx(
            OpenTK::Mathematics::Vector3 vector) noexcept;

        [[nodiscard]] static OpenTK::Mathematics::Vector4 WithX(
            OpenTK::Mathematics::Vector4 vector, float x) noexcept;
        [[nodiscard]] static OpenTK::Mathematics::Vector4 WithY(
            OpenTK::Mathematics::Vector4 vector, float y) noexcept;
        [[nodiscard]] static OpenTK::Mathematics::Vector4 WithZ(
            OpenTK::Mathematics::Vector4 vector, float z) noexcept;
        [[nodiscard]] static OpenTK::Mathematics::Vector4 WithW(
            OpenTK::Mathematics::Vector4 vector, float w) noexcept;
        [[nodiscard]] static OpenTK::Mathematics::Vector4 AddX(
            OpenTK::Mathematics::Vector4 vector, float x) noexcept;
        [[nodiscard]] static OpenTK::Mathematics::Vector4 AddY(
            OpenTK::Mathematics::Vector4 vector, float y) noexcept;
        [[nodiscard]] static OpenTK::Mathematics::Vector4 AddZ(
            OpenTK::Mathematics::Vector4 vector, float z) noexcept;
        [[nodiscard]] static OpenTK::Mathematics::Vector4 AddW(
            OpenTK::Mathematics::Vector4 vector, float w) noexcept;

        [[nodiscard]] static OpenTK::Mathematics::Matrix3 AsMatrix3(
            OpenTK::Mathematics::Matrix4x3 matrix) noexcept;
        [[nodiscard]] static OpenTK::Mathematics::Matrix4 AsMatrix4(
            OpenTK::Mathematics::Matrix4x3 matrix) noexcept;
        [[nodiscard]] static OpenTK::Mathematics::Matrix4 Keep3x3(
            OpenTK::Mathematics::Matrix4x3 matrix) noexcept;
        [[nodiscard]] static OpenTK::Mathematics::Matrix4 Keep3x3(
            OpenTK::Mathematics::Matrix4 matrix) noexcept;

        template <typename T>
        requires std::is_enum_v<T>
        [[nodiscard]] static constexpr bool TestFlag(T value, T flags) noexcept
        {
            using Underlying = std::underlying_type_t<T>;
            using Unsigned = std::make_unsigned_t<Underlying>;
            const Unsigned valueBits = static_cast<Unsigned>(static_cast<Underlying>(value));
            const Unsigned flagBits = static_cast<Unsigned>(static_cast<Underlying>(flags));
            return (valueBits | flagBits) == valueBits;
        }

        template <typename T>
        requires std::is_enum_v<T>
        [[nodiscard]] static constexpr bool TestAny(T value, T flags) noexcept
        {
            using Underlying = std::underlying_type_t<T>;
            using Unsigned = std::make_unsigned_t<Underlying>;
            const Unsigned valueBits = static_cast<Unsigned>(static_cast<Underlying>(value));
            const Unsigned flagBits = static_cast<Unsigned>(static_cast<Underlying>(flags));
            return (valueBits & flagBits) != 0;
        }
    };

    class MarshalExtensions final
    {
    public:
        MarshalExtensions() = delete;

        [[nodiscard]] static std::u16string MarshalString(
            const std::shared_ptr<ManagedArray<std::uint8_t>>& array);
        [[nodiscard]] static std::u16string MarshalString(
            const std::shared_ptr<ManagedArray<char16_t>>& array);
    };

    static_assert(std::is_standard_layout_v<LightInfo>);
    static_assert(sizeof(LightInfo) == 48);
    static_assert(offsetof(LightInfo, Light1Vector) == 0);
    static_assert(offsetof(LightInfo, Light1Color) == 12);
    static_assert(offsetof(LightInfo, Light2Vector) == 24);
    static_assert(offsetof(LightInfo, Light2Color) == 36);

    static_assert(std::is_standard_layout_v<Fixed> && sizeof(Fixed) == 4);
    static_assert(offsetof(Fixed, Value) == 0);

    static_assert(std::is_standard_layout_v<Vector3Fx> && sizeof(Vector3Fx) == 12);
    static_assert(offsetof(Vector3Fx, X) == 0);
    static_assert(offsetof(Vector3Fx, Y) == 4);
    static_assert(offsetof(Vector3Fx, Z) == 8);

    static_assert(std::is_standard_layout_v<Vector4Fx> && sizeof(Vector4Fx) == 16);
    static_assert(offsetof(Vector4Fx, X) == 0);
    static_assert(offsetof(Vector4Fx, Y) == 4);
    static_assert(offsetof(Vector4Fx, Z) == 8);
    static_assert(offsetof(Vector4Fx, W) == 12);

    static_assert(std::is_standard_layout_v<Matrix43Fx> && sizeof(Matrix43Fx) == 48);
    static_assert(offsetof(Matrix43Fx, One) == 0);
    static_assert(offsetof(Matrix43Fx, Two) == 12);
    static_assert(offsetof(Matrix43Fx, Three) == 24);
    static_assert(offsetof(Matrix43Fx, Four) == 36);

    static_assert(std::is_standard_layout_v<Matrix44Fx> && sizeof(Matrix44Fx) == 64);
    static_assert(offsetof(Matrix44Fx, One) == 0);
    static_assert(offsetof(Matrix44Fx, Two) == 16);
    static_assert(offsetof(Matrix44Fx, Three) == 32);
    static_assert(offsetof(Matrix44Fx, Four) == 48);

    static_assert(std::is_standard_layout_v<ColorRgb> && sizeof(ColorRgb) == 3);
    static_assert(offsetof(ColorRgb, Red) == 0);
    static_assert(offsetof(ColorRgb, Green) == 1);
    static_assert(offsetof(ColorRgb, Blue) == 2);

    static_assert(std::is_standard_layout_v<ColorRgba> && sizeof(ColorRgba) == 4);
    static_assert(offsetof(ColorRgba, Red) == 0);
    static_assert(offsetof(ColorRgba, Green) == 1);
    static_assert(offsetof(ColorRgba, Blue) == 2);
    static_assert(offsetof(ColorRgba, Alpha) == 3);
}
