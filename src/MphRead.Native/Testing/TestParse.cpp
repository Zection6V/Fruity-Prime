#include "TestParse.hpp"
#include "../Formats/Types.hpp"
#include "../NativeRuntime/System/Globalization.hpp"
#include "../NativeRuntime/System/Managed.hpp"
#include "../NativeRuntime/OpenTK/Mathematics.hpp"

#include <bit>
#include <cmath>
#include <cstdint>
#include <memory>
#include <optional>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

using ::MphRead::NativeRuntime::IsNumberWhiteSpace;
using ::MphRead::NativeRuntime::RequireReference;
using ::OpenTK::Mathematics::Length;
using ::OpenTK::Mathematics::MathHelper::RadiansToDegrees;

namespace
{
    using OpenTK::Mathematics::Matrix3;
    using OpenTK::Mathematics::Matrix4;
    using OpenTK::Mathematics::Matrix4x3;
    using OpenTK::Mathematics::Vector3;
    using OpenTK::Mathematics::Vector4;
    using StringArray = MphRead::Testing::TestParse::StringArray;

    struct Quaternion
    {
        float X = 0.0F;
        float Y = 0.0F;
        float Z = 0.0F;
        float W = 0.0F;
    };

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
        while (index < value.size() && IsNumberWhiteSpace(value[index]))
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

        while (index < value.size() && IsNumberWhiteSpace(value[index]))
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

    [[nodiscard]] std::shared_ptr<StringArray> Split(
        const std::optional<std::string>& value, char separator)
    {
        const std::string& source = RequireReference(value);
        auto result = std::make_shared<StringArray>();

        std::size_t start = 0;
        while (true)
        {
            const std::size_t position = source.find(separator, start);
            if (position == std::string::npos)
            {
                result->emplace_back(source.substr(start));
                break;
            }
            result->emplace_back(source.substr(start, position - start));
            start = position + 1;
        }
        return result;
    }

    void ValidateValues(
        const std::shared_ptr<StringArray>& values,
        std::size_t expectedLength,
        std::size_t expectedItemLength)
    {
        if (!values)
        {
            throw System::NullReferenceException();
        }

        if (values->size() != expectedLength)
        {
            throw System::ArgumentException("values");
        }

        for (const std::optional<std::string>& value : *values)
        {
            if (!value)
            {
                throw System::NullReferenceException();
            }
            if (value->size() != expectedItemLength)
            {
                throw System::ArgumentException("values");
            }
        }
    }

    [[nodiscard]] float ParseFixed(const std::optional<std::string>& value)
    {
        if (!value)
        {
            throw System::ArgumentNullException("s");
        }
        return static_cast<float>(ParseHexInt32(*value)) / 4096.0F;
    }

    [[nodiscard]] Matrix4x3 MakeMatrix4x3(
        float m11, float m12, float m13,
        float m21, float m22, float m23,
        float m31, float m32, float m33,
        float m41, float m42, float m43) noexcept
    {
        return Matrix4x3(
            Vector3(m11, m12, m13),
            Vector3(m21, m22, m23),
            Vector3(m31, m32, m33),
            Vector3(m41, m42, m43));
    }

    [[nodiscard]] Matrix4 MakeMatrix4(
        float m11, float m12, float m13, float m14,
        float m21, float m22, float m23, float m24,
        float m31, float m32, float m33, float m34,
        float m41, float m42, float m43, float m44) noexcept
    {
        return Matrix4(
            Vector4(m11, m12, m13, m14),
            Vector4(m21, m22, m23, m24),
            Vector4(m31, m32, m33, m34),
            Vector4(m41, m42, m43, m44));
    }

    [[nodiscard]] Matrix4 MakeMatrix4(Matrix3 value) noexcept
    {
        return Matrix4(
            Vector4(value.M11, value.M12, value.M13, 0.0F),
            Vector4(value.M21, value.M22, value.M23, 0.0F),
            Vector4(value.M31, value.M32, value.M33, 0.0F),
            Vector4(0.0F, 0.0F, 0.0F, 1.0F));
    }

    [[nodiscard]] Quaternion ExtractRotation(Matrix4 value)
    {
        Vector3 row0(value.M11, value.M12, value.M13);
        Vector3 row1(value.M21, value.M22, value.M23);
        Vector3 row2(value.M31, value.M32, value.M33);

        row0 = row0.Normalized();
        row1 = row1.Normalized();
        row2 = row2.Normalized();

        Quaternion q{};
        const double trace = 0.25 * (
            static_cast<double>(row0.X)
            + static_cast<double>(row1.Y)
            + static_cast<double>(row2.Z)
            + 1.0);

        if (trace > 0.0)
        {
            double sq = std::sqrt(trace);

            q.W = static_cast<float>(sq);
            sq = 1.0 / (4.0 * sq);
            q.X = static_cast<float>(
                (static_cast<double>(row1.Z) - static_cast<double>(row2.Y)) * sq);
            q.Y = static_cast<float>(
                (static_cast<double>(row2.X) - static_cast<double>(row0.Z)) * sq);
            q.Z = static_cast<float>(
                (static_cast<double>(row0.Y) - static_cast<double>(row1.X)) * sq);
        }
        else if (row0.X > row1.Y && row0.X > row2.Z)
        {
            double sq = 2.0 * std::sqrt(
                1.0 + static_cast<double>(row0.X)
                - static_cast<double>(row1.Y)
                - static_cast<double>(row2.Z));

            q.X = static_cast<float>(0.25 * sq);
            sq = 1.0 / sq;
            q.W = static_cast<float>(
                (static_cast<double>(row2.Y) - static_cast<double>(row1.Z)) * sq);
            q.Y = static_cast<float>(
                (static_cast<double>(row1.X) + static_cast<double>(row0.Y)) * sq);
            q.Z = static_cast<float>(
                (static_cast<double>(row2.X) + static_cast<double>(row0.Z)) * sq);
        }
        else if (row1.Y > row2.Z)
        {
            double sq = 2.0 * std::sqrt(
                1.0 + static_cast<double>(row1.Y)
                - static_cast<double>(row0.X)
                - static_cast<double>(row2.Z));

            q.Y = static_cast<float>(0.25 * sq);
            sq = 1.0 / sq;
            q.W = static_cast<float>(
                (static_cast<double>(row2.X) - static_cast<double>(row0.Z)) * sq);
            q.X = static_cast<float>(
                (static_cast<double>(row1.X) + static_cast<double>(row0.Y)) * sq);
            q.Z = static_cast<float>(
                (static_cast<double>(row2.Y) + static_cast<double>(row1.Z)) * sq);
        }
        else
        {
            double sq = 2.0 * std::sqrt(
                1.0 + static_cast<double>(row2.Z)
                - static_cast<double>(row0.X)
                - static_cast<double>(row1.Y));

            q.Z = static_cast<float>(0.25 * sq);
            sq = 1.0 / sq;
            q.W = static_cast<float>(
                (static_cast<double>(row1.X) - static_cast<double>(row0.Y)) * sq);
            q.X = static_cast<float>(
                (static_cast<double>(row2.X) + static_cast<double>(row0.Z)) * sq);
            q.Y = static_cast<float>(
                (static_cast<double>(row2.Y) + static_cast<double>(row1.Z)) * sq);
        }

        const float length = std::sqrt(
            (q.W * q.W)
            + ((q.X * q.X) + (q.Y * q.Y) + (q.Z * q.Z)));
        const float scale = 1.0F / length;
        q.X *= scale;
        q.Y *= scale;
        q.Z *= scale;
        q.W *= scale;
        return q;
    }

    [[nodiscard]] Vector3 ToEulerAngles(Quaternion q)
    {
        constexpr float singularityThreshold = 0.4999995F;
        constexpr float piOver2 = 3.1415927F / 2.0F;

        const float sqw = q.W * q.W;
        const float sqx = q.X * q.X;
        const float sqy = q.Y * q.Y;
        const float sqz = q.Z * q.Z;
        const float unit = sqx + sqy + sqz + sqw;
        const float singularityTest = (q.X * q.Z) + (q.W * q.Y);

        Vector3 eulerAngles{};
        if (singularityTest > singularityThreshold * unit)
        {
            eulerAngles.Z = 2.0F * std::atan2(q.X, q.W);
            eulerAngles.Y = piOver2;
            eulerAngles.X = 0.0F;
        }
        else if (singularityTest < -singularityThreshold * unit)
        {
            eulerAngles.Z = -2.0F * std::atan2(q.X, q.W);
            eulerAngles.Y = -piOver2;
            eulerAngles.X = 0.0F;
        }
        else
        {
            eulerAngles.Z = std::atan2(
                2.0F * ((q.W * q.Z) - (q.X * q.Y)),
                sqw + sqx - sqy - sqz);
            eulerAngles.Y = std::asin(2.0F * singularityTest / unit);
            eulerAngles.X = std::atan2(
                2.0F * ((q.W * q.X) - (q.Y * q.Z)),
                sqw - sqx - sqy + sqz);
        }
        return eulerAngles;
    }

    [[nodiscard]] Vector3 ExtractTranslation(Matrix4 value) noexcept
    {
        return Vector3(value.M41, value.M42, value.M43);
    }

    void SetRow1(Matrix3& matrix, Vector3 value) noexcept
    {
        matrix.M21 = value.X;
        matrix.M22 = value.Y;
        matrix.M23 = value.Z;
    }

    void SetRow2(Matrix3& matrix, Vector3 value) noexcept
    {
        matrix.M31 = value.X;
        matrix.M32 = value.Y;
        matrix.M33 = value.Z;
    }
}

namespace MphRead::Testing
{
    void TestParse::TestMatrix()
    {
        Vector3 field58(0.0F, 0.0F, 1.0F);
        Vector3 field64(0.0F, 1.0F, 0.0F);
        Vector3 field70(-1.0F, 0.0F, 0.0F);
        const Matrix3 mat1 = TestVectors(field58, field64, field70);
        Nop();

        field58 = Vector3(1.0F, 0.0F, 0.0F);
        field64 = Vector3(0.0F, 1.0F, 0.0F);
        field70 = Vector3(0.0F, 0.0F, 1.0F);
        const Matrix3 mat2 = TestVectors(field58, field64, field70);

        const Quaternion quat1 = ExtractRotation(MakeMatrix4(mat1));
        Vector3 rot1 = ToEulerAngles(quat1);
        rot1 = Vector3(
            RadiansToDegrees(rot1.X),
            RadiansToDegrees(rot1.Y),
            RadiansToDegrees(rot1.Z));

        const Quaternion quat2 = ExtractRotation(MakeMatrix4(mat2));
        Vector3 rot2 = ToEulerAngles(quat2);
        rot2 = Vector3(
            RadiansToDegrees(rot2.X),
            RadiansToDegrees(rot2.Y),
            RadiansToDegrees(rot2.Z));

        static_cast<void>(rot1);
        static_cast<void>(rot2);
        Nop();
    }

    void TestParse::TestMatrices()
    {
        const Matrix4x3 mtx1 = ParseMatrix48(
            "03 F0 FF FF 00 00 00 00 9C 00 00 00 F9 FF FF FF FB 0F 00 00 3E FF FF FF 64 FF FF FF 3E FF FF FF 08 F0 FF FF 22 00 00 00 86 40 00 00 F1 AD FD FF");
        const Matrix4x3 mtx2 = ParseMatrix48(
            "FD 0F 00 00 D3 FF FF FF 97 00 00 00 00 00 00 00 53 0F 00 00 9B 04 00 00 62 FF FF FF 66 FB FF FF 50 0F 00 00 F4 E8 FF FF DA 0B FF FF BF F8 01 00");
        const Matrix4x3 currentTextureMatrix = ParseMatrix48(
            "FF EF FF FF 00 00 00 00 FE FF FF FF 00 00 00 00 86 0F 00 00 DF 03 00 00 01 00 00 00 DF 03 00 00 7A F0 FF FF 00 00 00 00 7F F4 FF FF CA D2 FF FF");
        const Matrix4x3 mult = Matrix::Concat43(mtx1, mtx2);

        const Matrix4 trans(
            Vector4(mtx1.Row0(), 0.0F),
            Vector4(mtx1.Row1(), 0.0F),
            Vector4(mtx1.Row2(), 0.0F),
            Vector4(mtx1.Row3(), 1.0F));

        const Vector3 pos = ExtractTranslation(trans);
        Vector3 rot = ToEulerAngles(ExtractRotation(trans));
        rot = Vector3(
            RadiansToDegrees(rot.X),
            RadiansToDegrees(rot.Y),
            RadiansToDegrees(rot.Z));
        const Vector3 scale = trans.ExtractScale();

        static_cast<void>(currentTextureMatrix);
        static_cast<void>(mult);
        static_cast<void>(pos);
        static_cast<void>(rot);
        static_cast<void>(scale);
    }

    Matrix3 TestParse::TestVectors(
        Vector3 field58, Vector3 field64, Vector3 field70)
    {
        Matrix3 field4F4(
            Vector3(field58.X, 0.0F, field58.Z),
            Vector3(field64.X, field64.Y, field64.Z),
            Vector3(field70.X, 0.0F, field70.Y));

        SetRow2(field4F4, Vector3::Cross(field4F4.Row0(), field4F4.Row1()));
        SetRow1(field4F4, Vector3::Cross(field4F4.Row2(), field4F4.Row0()));

        const Vector3 row0 = field4F4.Row0().Normalized();
        const Vector3 row1 = field4F4.Row1().Normalized();
        const Vector3 row2 = field4F4.Row2().Normalized();

        field4F4.M11 = row0.X;
        field4F4.M12 = row0.Y;
        field4F4.M13 = row0.Z;
        SetRow1(field4F4, row1);
        SetRow2(field4F4, row2);

        return field4F4;
    }

    Vector3 TestParse::ParseVector3(const std::optional<std::string>& values)
    {
        const std::shared_ptr<StringArray> split = Split(values, ' ');
        if (split->size() != 12)
        {
            throw System::ArgumentException("values");
        }
        for (const std::optional<std::string>& value : *split)
        {
            if (!value)
            {
                throw System::NullReferenceException();
            }
            if (value->size() != 2)
            {
                throw System::ArgumentException("values");
            }
        }

        return Vector3(
            ParseHexInt32(
                split->at(3).value() + split->at(2).value()
                + split->at(1).value() + split->at(0).value()) / 4096.0F,
            ParseHexInt32(
                split->at(7).value() + split->at(6).value()
                + split->at(5).value() + split->at(4).value()) / 4096.0F,
            ParseHexInt32(
                split->at(11).value() + split->at(10).value()
                + split->at(9).value() + split->at(8).value()) / 4096.0F);
    }

    Matrix4x3 TestParse::ParseMatrix12(
        const std::shared_ptr<StringArray>& values)
    {
        ValidateValues(values, 12, 8);

        return MakeMatrix4x3(
            ParseFixed(values->at(0)),
            ParseFixed(values->at(1)),
            ParseFixed(values->at(2)),
            ParseFixed(values->at(3)),
            ParseFixed(values->at(4)),
            ParseFixed(values->at(5)),
            ParseFixed(values->at(6)),
            ParseFixed(values->at(7)),
            ParseFixed(values->at(8)),
            ParseFixed(values->at(9)),
            ParseFixed(values->at(10)),
            ParseFixed(values->at(11)));
    }

    Matrix4 TestParse::ParseMatrix16(
        const std::shared_ptr<StringArray>& values)
    {
        ValidateValues(values, 16, 8);

        return MakeMatrix4(
            ParseFixed(values->at(0)),
            ParseFixed(values->at(1)),
            ParseFixed(values->at(2)),
            ParseFixed(values->at(3)),
            ParseFixed(values->at(4)),
            ParseFixed(values->at(5)),
            ParseFixed(values->at(6)),
            ParseFixed(values->at(7)),
            ParseFixed(values->at(8)),
            ParseFixed(values->at(9)),
            ParseFixed(values->at(10)),
            ParseFixed(values->at(11)),
            ParseFixed(values->at(12)),
            ParseFixed(values->at(13)),
            ParseFixed(values->at(14)),
            ParseFixed(values->at(15)));
    }

    Matrix4 TestParse::ParseMatrix16(
        const std::optional<std::string>& value)
    {
        return ParseMatrix16(Split(value, ' '));
    }

    Matrix4x3 TestParse::ParseMatrix48(
        const std::optional<std::string>& value)
    {
        const std::shared_ptr<StringArray> values = Split(value, ' ');
        ValidateValues(values, 48, 2);

        auto matrixValues = std::make_shared<StringArray>();
        matrixValues->reserve(12);
        for (std::size_t index = 0; index < 48; index += 4)
        {
            matrixValues->emplace_back(
                values->at(index + 3).value()
                + values->at(index + 2).value()
                + values->at(index + 1).value()
                + values->at(index).value());
        }
        return ParseMatrix12(matrixValues);
    }

    Matrix4 TestParse::ParseMatrix64(
        const std::optional<std::string>& value)
    {
        const std::shared_ptr<StringArray> values = Split(value, ' ');
        ValidateValues(values, 64, 2);

        auto matrixValues = std::make_shared<StringArray>();
        matrixValues->reserve(16);
        for (std::size_t index = 0; index < 64; index += 4)
        {
            matrixValues->emplace_back(
                values->at(index + 3).value()
                + values->at(index + 2).value()
                + values->at(index + 1).value()
                + values->at(index).value());
        }
        return ParseMatrix16(matrixValues);
    }

    void TestParse::Nop() noexcept
    {
    }
}
