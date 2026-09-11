#include "Types.hpp"

#include <bit>
#include <charconv>
#include <cmath>
#include <cstdlib>
#include <limits>
#include <new>
#include <random>
#include <stdexcept>

namespace
{
    constexpr std::uint32_t Prime1 = 2654435761U;
    constexpr std::uint32_t Prime2 = 2246822519U;
    constexpr std::uint32_t Prime3 = 3266489917U;
    constexpr std::uint32_t Prime4 = 668265263U;
    constexpr std::uint32_t Prime5 = 374761393U;

    thread_local std::string ManagedCurrentNegativeSign = "-";

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

    [[nodiscard]] constexpr std::uint32_t HashRound(
        std::uint32_t hash, std::uint32_t input) noexcept
    {
        return std::rotl(hash + input * Prime2, 13) * Prime1;
    }

    [[nodiscard]] constexpr std::uint32_t QueueRound(
        std::uint32_t hash, std::uint32_t queuedValue) noexcept
    {
        return std::rotl(hash + queuedValue * Prime3, 17) * Prime4;
    }

    [[nodiscard]] constexpr std::uint32_t MixState(
        std::uint32_t v1, std::uint32_t v2,
        std::uint32_t v3, std::uint32_t v4) noexcept
    {
        return std::rotl(v1, 1)
            + std::rotl(v2, 7)
            + std::rotl(v3, 12)
            + std::rotl(v4, 18);
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

    [[nodiscard]] std::int32_t CombineHashCodes3(
        std::int32_t value1, std::int32_t value2, std::int32_t value3)
    {
        std::uint32_t hash = GlobalHashSeed() + Prime5;
        hash += 12U;
        hash = QueueRound(hash, static_cast<std::uint32_t>(value1));
        hash = QueueRound(hash, static_cast<std::uint32_t>(value2));
        hash = QueueRound(hash, static_cast<std::uint32_t>(value3));
        return std::bit_cast<std::int32_t>(MixFinal(hash));
    }

    [[nodiscard]] std::int32_t CombineHashCodes4(
        std::int32_t value1, std::int32_t value2,
        std::int32_t value3, std::int32_t value4)
    {
        const std::uint32_t seed = GlobalHashSeed();
        std::uint32_t v1 = seed + Prime1 + Prime2;
        std::uint32_t v2 = seed + Prime2;
        std::uint32_t v3 = seed;
        std::uint32_t v4 = seed - Prime1;

        v1 = HashRound(v1, static_cast<std::uint32_t>(value1));
        v2 = HashRound(v2, static_cast<std::uint32_t>(value2));
        v3 = HashRound(v3, static_cast<std::uint32_t>(value3));
        v4 = HashRound(v4, static_cast<std::uint32_t>(value4));

        std::uint32_t hash = MixState(v1, v2, v3, v4);
        hash += 16U;
        return std::bit_cast<std::int32_t>(MixFinal(hash));
    }

    [[nodiscard]] constexpr bool IsNumberWhitespace(char ch) noexcept
    {
        const auto value = static_cast<unsigned char>(ch);
        return value == 0x20 || (value >= 0x09 && value <= 0x0D);
    }

    [[nodiscard]] constexpr std::int32_t HexValue(char ch) noexcept
    {
        if (ch >= '0' && ch <= '9')
        {
            return ch - '0';
        }
        if (ch >= 'A' && ch <= 'F')
        {
            return ch - 'A' + 10;
        }
        if (ch >= 'a' && ch <= 'f')
        {
            return ch - 'a' + 10;
        }
        return -1;
    }

    [[nodiscard]] std::int32_t ParseHexInt32(std::string_view value)
    {
        std::size_t index = 0;
        while (index < value.size() && IsNumberWhitespace(value[index]))
        {
            index++;
        }
        if (index == value.size())
        {
            throw System::FormatException();
        }

        bool hadDigits = false;
        while (index < value.size() && value[index] == '0')
        {
            hadDigits = true;
            index++;
        }

        std::uint32_t parsed = 0;
        std::size_t significantDigits = 0;
        while (index < value.size())
        {
            const std::int32_t digit = HexValue(value[index]);
            if (digit < 0)
            {
                break;
            }
            hadDigits = true;
            if (significantDigits < 8)
            {
                parsed = (parsed << 4) | static_cast<std::uint32_t>(digit);
            }
            significantDigits++;
            index++;
        }

        if (!hadDigits)
        {
            throw System::FormatException();
        }

        const bool overflow = significantDigits > 8;

        while (index < value.size() && IsNumberWhitespace(value[index]))
        {
            index++;
        }
        while (index < value.size() && value[index] == '\0')
        {
            index++;
        }
        if (index != value.size())
        {
            throw System::FormatException();
        }
        if (overflow)
        {
            throw System::OverflowException();
        }

        return std::bit_cast<std::int32_t>(parsed);
    }

    [[nodiscard]] std::string RemoveLowercaseHexPrefixes(
        std::optional<std::string_view> value)
    {
        if (!value)
        {
            throw System::NullReferenceException();
        }

        std::string result(*value);
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

    [[nodiscard]] std::string FormatInt32CurrentCulture(std::int32_t value)
    {
        char digits[16]{};
        const std::uint32_t magnitude = value < 0
            ? 0U - static_cast<std::uint32_t>(value)
            : static_cast<std::uint32_t>(value);
        const auto [end, error] = std::to_chars(
            digits, digits + sizeof(digits), magnitude);
        if (error != std::errc{})
        {
            throw std::runtime_error("Failed to format Int32.");
        }

        std::string result;
        if (value < 0)
        {
            result += ManagedCurrentNegativeSign;
        }
        result.append(digits, end);
        return result;
    }

#if defined(DEBUG)
    [[noreturn]] void DebugAssertFailed() noexcept
    {
        std::abort();
    }

    void DebugAssert(bool condition) noexcept
    {
        if (!condition)
        {
            DebugAssertFailed();
        }
    }
#endif
}

namespace MphRead::NativeRuntime
{
    void SetManagedCurrentNegativeSign(std::string negativeSign)
    {
        ManagedCurrentNegativeSign = negativeSign;
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

    float Fixed::ToFloat(std::optional<std::string_view> value)
    {
        if (!value)
        {
            throw System::ArgumentNullException("s");
        }
        return ToFloat(ParseHexInt32(*value));
    }

    std::int32_t Fixed::ToInt(float value) noexcept
    {
        return ConvertToInt32Net9(value * 4096.0F);
    }

    std::string Fixed::ToString() const
    {
        return FormatInt32CurrentCulture(Value);
    }

    Vector3Fx::Vector3Fx(
        std::optional<std::string_view> x,
        std::optional<std::string_view> y,
        std::optional<std::string_view> z)
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
#if defined(DEBUG)
            DebugAssert(depth != 0.0F);
#endif
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
        return CombineHashCodes3(Red, Green, Blue);
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
        return CombineHashCodes4(Red, Green, Blue, Alpha);
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
            throw System::ArgumentNullException("array");
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
            throw System::ArgumentNullException("array");
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
