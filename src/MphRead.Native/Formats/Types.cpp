#include "Types.hpp"

#include <bit>
#include <cassert>
#include <charconv>
#include <cmath>
#include <initializer_list>
#include <limits>
#include <new>
#include <random>
#include <stdexcept>

namespace
{
    constexpr std::uint32_t Prime2 = 2246822519U;
    constexpr std::uint32_t Prime3 = 3266489917U;
    constexpr std::uint32_t Prime4 = 668265263U;
    constexpr std::uint32_t Prime5 = 374761393U;

    template <typename T>
    T& AssignReadonly(T& self, const T& other) noexcept
    {
        if (std::addressof(self) != std::addressof(other))
        {
            self.~T();
            ::new (static_cast<void*>(std::addressof(self))) T(other);
        }
        return self;
    }

    [[nodiscard]] std::uint32_t GlobalHashSeed()
    {
        static const std::uint32_t seed = []
        {
            std::random_device device;
            std::uniform_int_distribution<std::uint32_t> distribution;
            return distribution(device);
        }();
        return seed;
    }

    [[nodiscard]] constexpr std::uint32_t QueueRound(
        std::uint32_t hash, std::uint32_t queuedValue) noexcept
    {
        return std::rotl(hash + queuedValue * Prime3, 17) * Prime4;
    }

    [[nodiscard]] constexpr std::uint32_t MixFinal(std::uint32_t hash) noexcept
    {
        hash ^= hash >> 15;
        hash *= Prime2;
        hash ^= hash >> 13;
        hash *= Prime3;
        hash ^= hash >> 16;
        return hash;
    }

    [[nodiscard]] std::int32_t CombineHashCodes(std::initializer_list<std::int32_t> values)
    {
        std::uint32_t hash = GlobalHashSeed() + Prime5;
        hash += static_cast<std::uint32_t>(values.size() * sizeof(std::uint32_t));
        for (const std::int32_t value : values)
        {
            hash = QueueRound(hash, static_cast<std::uint32_t>(value));
        }
        return std::bit_cast<std::int32_t>(MixFinal(hash));
    }

    [[nodiscard]] std::size_t DotNetWhitespacePrefixLength(std::string_view value)
    {
        if (value.empty())
        {
            return 0;
        }

        const auto byte = [](char ch) { return static_cast<unsigned char>(ch); };
        const unsigned char c0 = byte(value[0]);
        if ((c0 >= 0x09 && c0 <= 0x0D) || c0 == 0x20)
        {
            return 1;
        }
        if (value.size() >= 2 && c0 == 0xC2)
        {
            const unsigned char c1 = byte(value[1]);
            if (c1 == 0x85 || c1 == 0xA0)
            {
                return 2;
            }
        }
        if (value.size() >= 3)
        {
            const unsigned char c1 = byte(value[1]);
            const unsigned char c2 = byte(value[2]);
            if (c0 == 0xE1 && c1 == 0x9A && c2 == 0x80)
            {
                return 3;
            }
            if (c0 == 0xE2 && c1 == 0x80
                && ((c2 >= 0x80 && c2 <= 0x8A)
                    || c2 == 0xA8 || c2 == 0xA9 || c2 == 0xAF))
            {
                return 3;
            }
            if (c0 == 0xE2 && c1 == 0x81 && c2 == 0x9F)
            {
                return 3;
            }
            if (c0 == 0xE3 && c1 == 0x80 && c2 == 0x80)
            {
                return 3;
            }
        }
        return 0;
    }

    [[nodiscard]] std::size_t DotNetWhitespaceSuffixLength(std::string_view value)
    {
        if (value.empty())
        {
            return 0;
        }

        const auto byte = [](char ch) { return static_cast<unsigned char>(ch); };
        const unsigned char last = byte(value.back());
        if ((last >= 0x09 && last <= 0x0D) || last == 0x20)
        {
            return 1;
        }
        if (value.size() >= 2 && byte(value[value.size() - 2]) == 0xC2
            && (last == 0x85 || last == 0xA0))
        {
            return 2;
        }
        if (value.size() >= 3)
        {
            const unsigned char c0 = byte(value[value.size() - 3]);
            const unsigned char c1 = byte(value[value.size() - 2]);
            if (c0 == 0xE1 && c1 == 0x9A && last == 0x80)
            {
                return 3;
            }
            if (c0 == 0xE2 && c1 == 0x80
                && ((last >= 0x80 && last <= 0x8A)
                    || last == 0xA8 || last == 0xA9 || last == 0xAF))
            {
                return 3;
            }
            if (c0 == 0xE2 && c1 == 0x81 && last == 0x9F)
            {
                return 3;
            }
            if (c0 == 0xE3 && c1 == 0x80 && last == 0x80)
            {
                return 3;
            }
        }
        return 0;
    }

    [[nodiscard]] std::string_view TrimDotNetWhitespace(std::string_view value)
    {
        while (const std::size_t count = DotNetWhitespacePrefixLength(value))
        {
            value.remove_prefix(count);
        }
        while (const std::size_t count = DotNetWhitespaceSuffixLength(value))
        {
            value.remove_suffix(count);
        }
        return value;
    }

    [[nodiscard]] std::int32_t ParseHexInt32(std::string_view value)
    {
        value = TrimDotNetWhitespace(value);
        if (value.empty())
        {
            throw std::invalid_argument("Input string was not in a correct format.");
        }

        std::uint64_t parsed = 0;
        const char* const end = value.data() + value.size();
        const auto [ptr, error] = std::from_chars(value.data(), end, parsed, 16);
        if (error == std::errc::invalid_argument || ptr != end)
        {
            throw std::invalid_argument("Input string was not in a correct format.");
        }
        if (error == std::errc::result_out_of_range
            || parsed > std::numeric_limits<std::uint32_t>::max())
        {
            throw std::out_of_range("Value was either too large or too small for an Int32.");
        }
        return std::bit_cast<std::int32_t>(static_cast<std::uint32_t>(parsed));
    }

    [[nodiscard]] std::string RemoveLowercaseHexPrefixes(std::string_view value)
    {
        std::string result(value);
        std::size_t position = 0;
        while ((position = result.find("0x", position)) != std::string::npos)
        {
            result.erase(position, 2);
        }
        return result;
    }

    [[nodiscard]] std::int32_t ConvertToInt32Net9(float value) noexcept
    {
        if (std::isnan(value))
        {
            return 0;
        }

        const double wide = static_cast<double>(value);
        if (wide < static_cast<double>(std::numeric_limits<std::int32_t>::min()))
        {
            return std::numeric_limits<std::int32_t>::min();
        }
        if (wide > static_cast<double>(std::numeric_limits<std::int32_t>::max()))
        {
            return std::numeric_limits<std::int32_t>::max();
        }
        return static_cast<std::int32_t>(std::trunc(wide));
    }

    [[nodiscard]] std::uint8_t RoundByteToEven(float value) noexcept
    {
        const float lower = std::floor(value);
        const float fraction = value - lower;
        float rounded = lower;
        if (fraction > 0.5F)
        {
            rounded = lower + 1.0F;
        }
        else if (fraction == 0.5F)
        {
            const auto integer = static_cast<std::uint32_t>(lower);
            rounded = (integer & 1U) == 0U ? lower : lower + 1.0F;
        }
        return static_cast<std::uint8_t>(rounded);
    }
}

namespace OpenTK::Mathematics
{
    constinit const Vector2 Vector2::Zero{};
    constinit const Vector3 Vector3::Zero{};
    constinit const Vector4 Vector4::Zero{};
    constinit const Matrix4x3 Matrix4x3::Zero{};
    constinit const Matrix4 Matrix4::Zero{};

    Vector3 Vector3::Normalized() const
    {
        const float inverse = 1.0F / std::sqrt((X * X) + (Y * Y) + (Z * Z));
        return Vector3(X * inverse, Y * inverse, Z * inverse);
    }

    Vector3 Vector3::Cross(Vector3 left, Vector3 right) noexcept
    {
        return Vector3(
            (left.Y * right.Z) - (left.Z * right.Y),
            (left.Z * right.X) - (left.X * right.Z),
            (left.X * right.Y) - (left.Y * right.X));
    }

    float Vector3::Dot(Vector3 left, Vector3 right) noexcept
    {
        return (left.X * right.X) + (left.Y * right.Y) + (left.Z * right.Z);
    }

    float Vector3::Distance(Vector3 left, Vector3 right)
    {
        const float x = left.X - right.X;
        const float y = left.Y - right.Y;
        const float z = left.Z - right.Z;
        return std::sqrt((x * x) + (y * y) + (z * z));
    }
}

namespace MphRead
{
    const LightInfo LightInfo::Zero(
        OpenTK::Mathematics::Vector3::Zero,
        OpenTK::Mathematics::Vector3::Zero,
        OpenTK::Mathematics::Vector3::Zero,
        OpenTK::Mathematics::Vector3::Zero);

    LightInfo& LightInfo::operator=(const LightInfo& other) noexcept
    {
        return AssignReadonly(*this, other);
    }

    RenderItem::RenderItem()
        : MatrixStack(std::make_shared<ManagedArray<float>>(16U * 31U)),
          Points(ManagedArray<OpenTK::Mathematics::Vector3>::Empty())
    {
    }

    Fixed& Fixed::operator=(const Fixed& other) noexcept
    {
        return AssignReadonly(*this, other);
    }

    float Fixed::FloatValue() const noexcept
    {
        return ToFloat(Value);
    }

    float Fixed::ToFloat(std::int64_t value) noexcept
    {
        return static_cast<float>(value) / static_cast<float>(1 << 12);
    }

    float Fixed::ToFloat(std::uint32_t value) noexcept
    {
        return static_cast<float>(std::bit_cast<std::int32_t>(value))
            / static_cast<float>(1 << 12);
    }

    float Fixed::ToFloat(std::int32_t value) noexcept
    {
        return static_cast<float>(value) / static_cast<float>(1 << 12);
    }

    float Fixed::ToFloat(std::string_view value)
    {
        return ToFloat(ParseHexInt32(value));
    }

    std::int32_t Fixed::ToInt(float value) noexcept
    {
        return ConvertToInt32Net9(value * 4096.0F);
    }

    std::string Fixed::ToString() const
    {
        return std::to_string(Value);
    }

    Vector3Fx::Vector3Fx(std::string_view x, std::string_view y, std::string_view z)
        : X(ParseHexInt32(RemoveLowercaseHexPrefixes(x))),
          Y(ParseHexInt32(RemoveLowercaseHexPrefixes(y))),
          Z(ParseHexInt32(RemoveLowercaseHexPrefixes(z)))
    {
    }

    Vector3Fx& Vector3Fx::operator=(const Vector3Fx& other) noexcept
    {
        return AssignReadonly(*this, other);
    }

    OpenTK::Mathematics::Vector3 Vector3Fx::ToFloatVector() const noexcept
    {
        return OpenTK::Mathematics::Vector3(
            X.FloatValue(), Y.FloatValue(), Z.FloatValue());
    }

    OpenTK::Mathematics::Vector3i Vector3Fx::ToIntVector() const noexcept
    {
        return OpenTK::Mathematics::Vector3i(X.Value, Y.Value, Z.Value);
    }

    Vector4Fx& Vector4Fx::operator=(const Vector4Fx& other) noexcept
    {
        return AssignReadonly(*this, other);
    }

    OpenTK::Mathematics::Vector4 Vector4Fx::ToFloatVector() const noexcept
    {
        return OpenTK::Mathematics::Vector4(
            X.FloatValue(), Y.FloatValue(), Z.FloatValue(), W.FloatValue());
    }

    Matrix43Fx& Matrix43Fx::operator=(const Matrix43Fx& other) noexcept
    {
        return AssignReadonly(*this, other);
    }

    Matrix44Fx& Matrix44Fx::operator=(const Matrix44Fx& other) noexcept
    {
        return AssignReadonly(*this, other);
    }

    OpenTK::Mathematics::Matrix4 Matrix44Fx::ToFloatMatrix() const noexcept
    {
        return OpenTK::Mathematics::Matrix4(
            One.ToFloatVector(), Two.ToFloatVector(),
            Three.ToFloatVector(), Four.ToFloatVector());
    }

    OpenTK::Mathematics::Matrix3 Matrix::GetTransform3(
        OpenTK::Mathematics::Vector3 vector1,
        OpenTK::Mathematics::Vector3 vector2)
    {
        using OpenTK::Mathematics::Matrix3;
        using OpenTK::Mathematics::Vector3;

        const Vector3 up = Vector3::Cross(vector2, vector1).Normalized();
        const Vector3 direction = Vector3::Cross(vector1, up);

        Matrix3 transform{};
        transform.M11 = up.X;
        transform.M12 = up.Y;
        transform.M13 = up.Z;
        transform.M21 = direction.X;
        transform.M22 = direction.Y;
        transform.M23 = direction.Z;
        transform.M31 = vector1.X;
        transform.M32 = vector1.Y;
        transform.M33 = vector1.Z;
        return transform;
    }

    OpenTK::Mathematics::Matrix4 Matrix::GetTransform4(
        OpenTK::Mathematics::Vector3 vector1,
        OpenTK::Mathematics::Vector3 vector2,
        OpenTK::Mathematics::Vector3 position)
    {
        using OpenTK::Mathematics::Matrix4;
        using OpenTK::Mathematics::Vector3;

        const Vector3 up = Vector3::Cross(vector2, vector1).Normalized();
        const Vector3 direction = Vector3::Cross(vector1, up);

        Matrix4 transform{};
        transform.M11 = up.X;
        transform.M12 = up.Y;
        transform.M13 = up.Z;
        transform.M21 = direction.X;
        transform.M22 = direction.Y;
        transform.M23 = direction.Z;
        transform.M31 = vector1.X;
        transform.M32 = vector1.Y;
        transform.M33 = vector1.Z;
        transform.M41 = position.X;
        transform.M42 = position.Y;
        transform.M43 = position.Z;
        transform.M44 = 1.0F;
        return transform;
    }

    OpenTK::Mathematics::Matrix4 Matrix::GetTransformSRT(
        OpenTK::Mathematics::Vector3 scale,
        OpenTK::Mathematics::Vector3 angle,
        OpenTK::Mathematics::Vector3 position)
    {
        const float sinAx = std::sin(angle.X);
        const float sinAy = std::sin(angle.Y);
        const float sinAz = std::sin(angle.Z);
        const float cosAx = std::cos(angle.X);
        const float cosAy = std::cos(angle.Y);
        const float cosAz = std::cos(angle.Z);

        const float v18 = cosAx * cosAz;
        const float v19 = cosAx * sinAz;
        const float v20 = cosAx * cosAy;
        const float v22 = sinAx * sinAy;
        const float v17 = v19 * sinAy;

        OpenTK::Mathematics::Matrix4 transform{};
        transform.M11 = scale.X * cosAy * cosAz;
        transform.M12 = scale.X * cosAy * sinAz;
        transform.M13 = scale.X * -sinAy;
        transform.M21 = scale.Y * ((v22 * cosAz) - v19);
        transform.M22 = scale.Y * ((v22 * sinAz) + v18);
        transform.M23 = scale.Y * sinAx * cosAy;
        transform.M31 = scale.Z * (v18 * sinAy + sinAx * sinAz);
        transform.M32 = scale.Z * (v17 + (v19 * sinAy) - (sinAx * cosAz));
        transform.M33 = scale.Z * v20;
        transform.M41 = position.X;
        transform.M42 = position.Y;
        transform.M43 = position.Z;
        transform.M14 = 0.0F;
        transform.M24 = 0.0F;
        transform.M34 = 0.0F;
        transform.M44 = 1.0F;
        return transform;
    }

    OpenTK::Mathematics::Vector3 Matrix::Vec3MultMtx4(
        OpenTK::Mathematics::Vector3 vec,
        OpenTK::Mathematics::Matrix4 mat) noexcept
    {
        return OpenTK::Mathematics::Vector3(
            (vec.X * mat.M11) + (vec.Y * mat.M21) + (vec.Z * mat.M31) + mat.M41,
            (vec.X * mat.M12) + (vec.Y * mat.M22) + (vec.Z * mat.M32) + mat.M42,
            (vec.X * mat.M13) + (vec.Y * mat.M23) + (vec.Z * mat.M33) + mat.M43);
    }

    OpenTK::Mathematics::Vector3 Matrix::Vec3MultMtx3(
        OpenTK::Mathematics::Vector3 vec,
        OpenTK::Mathematics::Matrix4 mat) noexcept
    {
        return OpenTK::Mathematics::Vector3(
            (vec.X * mat.M11) + (vec.Y * mat.M21) + (vec.Z * mat.M31),
            (vec.X * mat.M12) + (vec.Y * mat.M22) + (vec.Z * mat.M32),
            (vec.X * mat.M13) + (vec.Y * mat.M23) + (vec.Z * mat.M33));
    }

    OpenTK::Mathematics::Vector3 Matrix::Vec4MultMtx4x3(
        OpenTK::Mathematics::Vector4 vec,
        OpenTK::Mathematics::Matrix4x3 mat) noexcept
    {
        return OpenTK::Mathematics::Vector3(
            (vec.W * mat.M41) + (vec.Z * mat.M31) + (vec.X * mat.M11) + (vec.Y * mat.M21),
            (vec.W * mat.M42) + (vec.Z * mat.M32) + (vec.X * mat.M12) + (vec.Y * mat.M22),
            (vec.W * mat.M43) + (vec.Z * mat.M33) + (vec.X * mat.M13) + (vec.Y * mat.M23));
    }

    OpenTK::Mathematics::Matrix4x3 Matrix::Concat43(
        OpenTK::Mathematics::Matrix4x3 first,
        OpenTK::Mathematics::Matrix4x3 second) noexcept
    {
        OpenTK::Mathematics::Matrix4x3 output = OpenTK::Mathematics::Matrix4x3::Zero;
        output.M11 = first.M13 * second.M31 + first.M11 * second.M11 + first.M12 * second.M21;
        output.M12 = first.M13 * second.M32 + first.M11 * second.M12 + first.M12 * second.M22;
        output.M13 = first.M13 * second.M33 + first.M11 * second.M13 + first.M12 * second.M23;
        output.M21 = first.M23 * second.M31 + first.M21 * second.M11 + first.M22 * second.M21;
        output.M22 = first.M23 * second.M32 + first.M21 * second.M12 + first.M22 * second.M22;
        output.M23 = first.M23 * second.M33 + first.M21 * second.M13 + first.M22 * second.M23;
        output.M31 = first.M33 * second.M31 + first.M31 * second.M11 + first.M32 * second.M21;
        output.M32 = first.M33 * second.M32 + first.M31 * second.M12 + first.M32 * second.M22;
        output.M33 = first.M33 * second.M33 + first.M31 * second.M13 + first.M32 * second.M23;
        output.M41 = second.M41 + first.M43 * second.M31 + first.M41 * second.M11 + first.M42 * second.M21;
        output.M42 = second.M42 + first.M43 * second.M32 + first.M41 * second.M12 + first.M42 * second.M22;
        output.M43 = second.M43 + first.M43 * second.M33 + first.M41 * second.M13 + first.M42 * second.M23;
        return output;
    }

    OpenTK::Mathematics::Matrix4 Matrix::Multiply44(
        OpenTK::Mathematics::Matrix4 first,
        OpenTK::Mathematics::Matrix4 second) noexcept
    {
        OpenTK::Mathematics::Matrix4 output = OpenTK::Mathematics::Matrix4::Zero;
        output.M11 = first.M13 * second.M31 + first.M11 * second.M11 + first.M12 * second.M21;
        output.M12 = first.M13 * second.M32 + first.M11 * second.M12 + first.M12 * second.M22;
        output.M21 = first.M23 * second.M31 + first.M21 * second.M11 + first.M22 * second.M21;
        output.M22 = first.M23 * second.M32 + first.M21 * second.M12 + first.M22 * second.M22;
        output.M31 = first.M33 * second.M31 + first.M31 * second.M11 + first.M32 * second.M21;
        output.M32 = first.M33 * second.M32 + first.M31 * second.M12 + first.M32 * second.M22;
        return output;
    }

    OpenTK::Mathematics::Matrix3 Matrix::RotateAlign(
        OpenTK::Mathematics::Vector3 from,
        OpenTK::Mathematics::Vector3 to) noexcept
    {
        using OpenTK::Mathematics::Matrix3;
        using OpenTK::Mathematics::Vector3;

        const Vector3 axis = Vector3::Cross(from, to);
        const float cos = Vector3::Dot(from, to);
        const float k = 1.0F / (1.0F + cos);
        return Matrix3(
            (axis.X * axis.X * k) + cos,
            (axis.Z * axis.X * k) - axis.Z,
            (axis.Z * axis.X * k) + axis.Y,
            (axis.X * axis.Y * k) + axis.Z,
            (axis.Y * axis.Y * k) + cos,
            (axis.Z * axis.Y * k) - axis.X,
            (axis.X * axis.Z * k) - axis.Y,
            (axis.Y * axis.Z * k) + axis.X,
            (axis.Z * axis.Z * k) + cos);
    }

    float Matrix::ProjectPosition(
        OpenTK::Mathematics::Vector3 pos,
        OpenTK::Mathematics::Matrix4 viewMatrix,
        OpenTK::Mathematics::Matrix4 projectionMtx,
        OpenTK::Mathematics::Vector2& dest) noexcept
    {
        pos = Vec3MultMtx4(pos, viewMatrix);
        const float w = projectionMtx.M44
            + pos.X * projectionMtx.M14
            + pos.Y * projectionMtx.M24
            + pos.Z * projectionMtx.M34;
        if (w <= 0.0F)
        {
            dest = OpenTK::Mathematics::Vector2::Zero;
            return w;
        }

        const float x = (projectionMtx.M41
            + pos.X * projectionMtx.M11
            + pos.Y * projectionMtx.M21
            + pos.Z * projectionMtx.M31) / w;
        const float y = (projectionMtx.M42
            + pos.X * projectionMtx.M12
            + pos.Y * projectionMtx.M22
            + pos.Z * projectionMtx.M32) / w;
        dest.X = (x + 1.0F) / 2.0F;
        dest.Y = (1.0F - y) / 2.0F;
        return w;
    }

    void Matrix::GetProjectedValues(
        OpenTK::Mathematics::Vector3 pos,
        OpenTK::Mathematics::Vector3 camPos,
        OpenTK::Mathematics::Matrix4 viewMatrix,
        OpenTK::Mathematics::Matrix4 projectionMtx,
        float& dist,
        float& depth,
        float& scaleInv,
        OpenTK::Mathematics::Vector3& target,
        OpenTK::Mathematics::Vector2& screenPos)
    {
        using OpenTK::Mathematics::Vector2;
        using OpenTK::Mathematics::Vector3;

        const Vector3 between = (camPos - pos).Normalized();
        target = pos + between;
        dist = Vector3::Distance(camPos, target);
        pos = Vec3MultMtx4(pos, viewMatrix);
        depth = projectionMtx.M44
            + pos.X * projectionMtx.M14
            + pos.Y * projectionMtx.M24
            + pos.Z * projectionMtx.M34;
        if (depth > 0.0F)
        {
            scaleInv = 1.0F / depth;
        }
        else
        {
            assert(depth != 0.0F);
            scaleInv = 0.0F;
            screenPos = Vector2::Zero;
            return;
        }

        const float x = (projectionMtx.M41
            + pos.X * projectionMtx.M11
            + pos.Y * projectionMtx.M21
            + pos.Z * projectionMtx.M31) / depth;
        const float y = (projectionMtx.M42
            + pos.X * projectionMtx.M12
            + pos.Y * projectionMtx.M22
            + pos.Z * projectionMtx.M32) / depth;
        screenPos = Vector2(x, y);
    }

    ColorRgb& ColorRgb::operator=(const ColorRgb& other) noexcept
    {
        return AssignReadonly(*this, other);
    }

    OpenTK::Mathematics::Vector3 ColorRgb::AsVector3() const noexcept
    {
        return OpenTK::Mathematics::Vector3(
            Red / 255.0F, Green / 255.0F, Blue / 255.0F);
    }

    OpenTK::Mathematics::Vector4 ColorRgb::AsVector4(float alpha) const noexcept
    {
        return OpenTK::Mathematics::Vector4(
            Red / 255.0F, Green / 255.0F, Blue / 255.0F, alpha);
    }

    OpenTK::Mathematics::Vector3 operator/(ColorRgb left, float right) noexcept
    {
        return OpenTK::Mathematics::Vector3(
            left.Red / right, left.Green / right, left.Blue / right);
    }

    bool ColorRgb::Equals(const std::any& obj) const
    {
        const ColorRgb* const other = std::any_cast<ColorRgb>(&obj);
        return other != nullptr
            && Red == other->Red
            && Green == other->Green
            && Blue == other->Blue;
    }

    std::int32_t ColorRgb::GetHashCode() const
    {
        return CombineHashCodes({Red, Green, Blue});
    }

    ColorRgba::ColorRgba(std::uint32_t value, std::uint8_t alpha) noexcept
        : Red(RoundByteToEven(
            static_cast<float>((value >> 0) & 0x1FU) / 31.0F * 255.0F)),
          Green(RoundByteToEven(
            static_cast<float>((value >> 5) & 0x1FU) / 31.0F * 255.0F)),
          Blue(RoundByteToEven(
            static_cast<float>((value >> 10) & 0x1FU) / 31.0F * 255.0F)),
          Alpha(alpha)
    {
    }

    ColorRgba& ColorRgba::operator=(const ColorRgba& other) noexcept
    {
        return AssignReadonly(*this, other);
    }

    ColorRgba ColorRgba::WithAlpha(std::uint8_t alpha) const noexcept
    {
        return ColorRgba(Red, Green, Blue, alpha);
    }

    std::uint32_t ColorRgba::ToUint() const noexcept
    {
        return static_cast<std::uint32_t>(Red)
            | (static_cast<std::uint32_t>(Green) << 8)
            | (static_cast<std::uint32_t>(Blue) << 16)
            | (static_cast<std::uint32_t>(Alpha) << 24);
    }

    bool ColorRgba::Equals(const std::any& obj) const
    {
        const ColorRgba* const other = std::any_cast<ColorRgba>(&obj);
        return other != nullptr
            && Red == other->Red
            && Green == other->Green
            && Blue == other->Blue
            && Alpha == other->Alpha;
    }

    std::int32_t ColorRgba::GetHashCode() const
    {
        return CombineHashCodes({Red, Green, Blue, Alpha});
    }

    OpenTK::Mathematics::Vector2 TypeExtensions::WithX(
        OpenTK::Mathematics::Vector2 vector, float x) noexcept
    {
        return {x, vector.Y};
    }

    OpenTK::Mathematics::Vector2 TypeExtensions::WithY(
        OpenTK::Mathematics::Vector2 vector, float y) noexcept
    {
        return {vector.X, y};
    }

    OpenTK::Mathematics::Vector2 TypeExtensions::AddX(
        OpenTK::Mathematics::Vector2 vector, float x) noexcept
    {
        return {vector.X + x, vector.Y};
    }

    OpenTK::Mathematics::Vector2 TypeExtensions::AddY(
        OpenTK::Mathematics::Vector2 vector, float y) noexcept
    {
        return {vector.X, vector.Y + y};
    }

    OpenTK::Mathematics::Vector3 TypeExtensions::WithX(
        OpenTK::Mathematics::Vector3 vector, float x) noexcept
    {
        return {x, vector.Y, vector.Z};
    }

    OpenTK::Mathematics::Vector3 TypeExtensions::WithY(
        OpenTK::Mathematics::Vector3 vector, float y) noexcept
    {
        return {vector.X, y, vector.Z};
    }

    OpenTK::Mathematics::Vector3 TypeExtensions::WithZ(
        OpenTK::Mathematics::Vector3 vector, float z) noexcept
    {
        return {vector.X, vector.Y, z};
    }

    OpenTK::Mathematics::Vector3 TypeExtensions::AddX(
        OpenTK::Mathematics::Vector3 vector, float x) noexcept
    {
        return {vector.X + x, vector.Y, vector.Z};
    }

    OpenTK::Mathematics::Vector3 TypeExtensions::AddY(
        OpenTK::Mathematics::Vector3 vector, float y) noexcept
    {
        return {vector.X, vector.Y + y, vector.Z};
    }

    OpenTK::Mathematics::Vector3 TypeExtensions::AddZ(
        OpenTK::Mathematics::Vector3 vector, float z) noexcept
    {
        return {vector.X, vector.Y, vector.Z + z};
    }

    OpenTK::Mathematics::Vector3i TypeExtensions::ToFixedVector(
        OpenTK::Mathematics::Vector3 vector) noexcept
    {
        return {
            Fixed::ToInt(vector.X),
            Fixed::ToInt(vector.Y),
            Fixed::ToInt(vector.Z)
        };
    }

    Vector3Fx TypeExtensions::ToVector3Fx(
        OpenTK::Mathematics::Vector3 vector) noexcept
    {
        return Vector3Fx(
            Fixed::ToInt(vector.X),
            Fixed::ToInt(vector.Y),
            Fixed::ToInt(vector.Z));
    }

    OpenTK::Mathematics::Vector4 TypeExtensions::WithX(
        OpenTK::Mathematics::Vector4 vector, float x) noexcept
    {
        return {x, vector.Y, vector.Z, vector.W};
    }

    OpenTK::Mathematics::Vector4 TypeExtensions::WithY(
        OpenTK::Mathematics::Vector4 vector, float y) noexcept
    {
        return {vector.X, y, vector.Z, vector.W};
    }

    OpenTK::Mathematics::Vector4 TypeExtensions::WithZ(
        OpenTK::Mathematics::Vector4 vector, float z) noexcept
    {
        return {vector.X, vector.Y, z, vector.W};
    }

    OpenTK::Mathematics::Vector4 TypeExtensions::WithW(
        OpenTK::Mathematics::Vector4 vector, float w) noexcept
    {
        return {vector.X, vector.Y, vector.Z, w};
    }

    OpenTK::Mathematics::Vector4 TypeExtensions::AddX(
        OpenTK::Mathematics::Vector4 vector, float x) noexcept
    {
        return {vector.X + x, vector.Y, vector.Z, vector.W};
    }

    OpenTK::Mathematics::Vector4 TypeExtensions::AddY(
        OpenTK::Mathematics::Vector4 vector, float y) noexcept
    {
        return {vector.X, vector.Y + y, vector.Z, vector.W};
    }

    OpenTK::Mathematics::Vector4 TypeExtensions::AddZ(
        OpenTK::Mathematics::Vector4 vector, float z) noexcept
    {
        return {vector.X, vector.Y, vector.Z + z, vector.W};
    }

    OpenTK::Mathematics::Vector4 TypeExtensions::AddW(
        OpenTK::Mathematics::Vector4 vector, float w) noexcept
    {
        return {vector.X, vector.Y, vector.Z, vector.W + w};
    }

    OpenTK::Mathematics::Matrix3 TypeExtensions::AsMatrix3(
        OpenTK::Mathematics::Matrix4x3 matrix) noexcept
    {
        return OpenTK::Mathematics::Matrix3(
            matrix.Row0(), matrix.Row1(), matrix.Row2());
    }

    OpenTK::Mathematics::Matrix4 TypeExtensions::AsMatrix4(
        OpenTK::Mathematics::Matrix4x3 matrix) noexcept
    {
        return OpenTK::Mathematics::Matrix4(
            OpenTK::Mathematics::Vector4(matrix.Row0()),
            OpenTK::Mathematics::Vector4(matrix.Row1()),
            OpenTK::Mathematics::Vector4(matrix.Row2()),
            OpenTK::Mathematics::Vector4(matrix.Row3()));
    }

    OpenTK::Mathematics::Matrix4 TypeExtensions::Keep3x3(
        OpenTK::Mathematics::Matrix4x3 matrix) noexcept
    {
        return OpenTK::Mathematics::Matrix4(
            OpenTK::Mathematics::Vector4(matrix.Row0(), 0.0F),
            OpenTK::Mathematics::Vector4(matrix.Row1(), 0.0F),
            OpenTK::Mathematics::Vector4(matrix.Row2(), 0.0F),
            OpenTK::Mathematics::Vector4::Zero);
    }

    OpenTK::Mathematics::Matrix4 TypeExtensions::Keep3x3(
        OpenTK::Mathematics::Matrix4 matrix) noexcept
    {
        return OpenTK::Mathematics::Matrix4(
            OpenTK::Mathematics::Vector4(matrix.Row0().Xyz(), 0.0F),
            OpenTK::Mathematics::Vector4(matrix.Row1().Xyz(), 0.0F),
            OpenTK::Mathematics::Vector4(matrix.Row2().Xyz(), 0.0F),
            OpenTK::Mathematics::Vector4::Zero);
    }

    std::u16string MarshalExtensions::MarshalString(
        const std::shared_ptr<ManagedArray<std::uint8_t>>& array)
    {
        if (!array)
        {
            throw std::invalid_argument("array");
        }

        std::u16string result;
        result.reserve(array->Length());
        for (std::size_t i = 0; i < array->Length(); i++)
        {
            const char16_t value = static_cast<char16_t>((*array)[i]);
            if (value == u'\0')
            {
                break;
            }
            result.push_back(value);
        }
        return result;
    }

    std::u16string MarshalExtensions::MarshalString(
        const std::shared_ptr<ManagedArray<char16_t>>& array)
    {
        if (!array)
        {
            throw std::invalid_argument("array");
        }

        std::u16string result;
        result.reserve(array->Length());
        for (std::size_t i = 0; i < array->Length(); i++)
        {
            const char16_t value = (*array)[i];
            if (value == u'\0')
            {
                break;
            }
            result.push_back(value);
        }
        return result;
    }
}
