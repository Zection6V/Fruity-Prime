#include "MotionCarry.hpp"

#include "../../Formats/Types.hpp"

#include <algorithm>
#include <cmath>
#include <cstring>

namespace MphRead::Mods::Render
{
    namespace
    {
        using OpenTK::Mathematics::Matrix4;
        using OpenTK::Mathematics::Vector3;

        constexpr float Pi = 3.14159265358979323846F;

        const Matrix4 Identity(OpenTK::Mathematics::Vector4(1, 0, 0, 0), OpenTK::Mathematics::Vector4(0, 1, 0, 0),
            OpenTK::Mathematics::Vector4(0, 0, 1, 0), OpenTK::Mathematics::Vector4(0, 0, 0, 1));

        Matrix4 Rotation(const Matrix4& value) noexcept
        {
            Matrix4 result = value.ClearTranslation();
            result.M14 = result.M24 = result.M34 = 0.0F;
            result.M44 = 1.0F;
            return result;
        }

        // How far a 3x3 is from a rotation: rows unit length and orthogonal.
        bool IsRotation(const Matrix4& m) noexcept
        {
            const Vector3 a(m.M11, m.M12, m.M13), b(m.M21, m.M22, m.M23), c(m.M31, m.M32, m.M33);
            constexpr float tolerance = 1e-3F;
            return std::fabs(a.LengthSquared() - 1) < tolerance && std::fabs(b.LengthSquared() - 1) < tolerance
                && std::fabs(c.LengthSquared() - 1) < tolerance && std::fabs(Vector3::Dot(a, b)) < tolerance
                && std::fabs(Vector3::Dot(a, c)) < tolerance && std::fabs(Vector3::Dot(b, c)) < tolerance;
        }

        // A rotation raised to a power, through its axis and angle. Row-vector
        // matrices are the transpose of the column ones the formulas are
        // written for, so the axis is read off the transpose's skew part and
        // the result built transposed.
        std::optional<Matrix4> Power(const Matrix4& rotation, float exponent) noexcept
        {
            const float trace = rotation.M11 + rotation.M22 + rotation.M33;
            const float cosine = std::clamp((trace - 1.0F) / 2.0F, -1.0F, 1.0F);
            const float angle = std::acos(cosine);
            if (angle < 1e-6F) return Identity;
            if (angle > MotionCarry::MaxTurnDegrees * Pi / 180.0F) return std::nullopt;
            // Column form c = transpose(rotation): axis = (c32 - c23, c13 - c31, c21 - c12).
            Vector3 axis(rotation.M23 - rotation.M32, rotation.M31 - rotation.M13, rotation.M12 - rotation.M21);
            const float length = std::sqrt(axis.LengthSquared());
            if (length < 1e-9F) return std::nullopt;
            axis = Vector3(axis.X / length, axis.Y / length, axis.Z / length);
            const float turn = angle * exponent;
            const float c = std::cos(turn), s = std::sin(turn), t = 1.0F - c;
            const float x = axis.X, y = axis.Y, z = axis.Z;
            Matrix4 result = Identity;
            // Rodrigues in column form, stored transposed.
            result.M11 = t * x * x + c;     result.M21 = t * x * y - s * z; result.M31 = t * x * z + s * y;
            result.M12 = t * x * y + s * z; result.M22 = t * y * y + c;     result.M32 = t * y * z - s * x;
            result.M13 = t * x * z - s * y; result.M23 = t * y * z + s * x; result.M33 = t * z * z + c;
            return result;
        }

        bool NearIdentity(const Matrix4& m) noexcept
        {
            const float* a = &m.M11;
            const float* b = &Identity.M11;
            for (int i = 0; i < 16; ++i)
            {
                if (std::fabs(a[i] - b[i]) > 1e-5F) return false;
            }
            return true;
        }
    }

    std::optional<Matrix4> MotionCarry::Between(const Matrix4& previous, const Matrix4& current, float fraction)
    {
        if (!(fraction > 0.0F)) return std::nullopt;
        const Vector3 from(previous.M41, previous.M42, previous.M43);
        const Vector3 to(current.M41, current.M42, current.M43);
        const Vector3 moved = to - from;
        if (moved.LengthSquared() > JumpDistance * JumpDistance) return std::nullopt;
        // The step's turn, D with current = previous * D, carried a fraction on.
        Matrix4 turned = Rotation(current);
        const Matrix4 before = Rotation(previous);
        const Matrix4 turn = OpenTK::Mathematics::Inverted(before) * turned;
        if (IsRotation(turn))
        {
            if (const auto part = Power(turn, fraction)) turned = turned * *part;
        }
        Matrix4 shown = turned;
        shown.M41 = to.X + moved.X * fraction;
        shown.M42 = to.Y + moved.Y * fraction;
        shown.M43 = to.Z + moved.Z * fraction;
        shown.M44 = current.M44;
        // Rotation and Inverted work on the upper 3x3; the carry keeps the
        // pose's scale and other columns as the step had them.
        Matrix4 drawn = current;
        drawn.M11 = shown.M11; drawn.M12 = shown.M12; drawn.M13 = shown.M13;
        drawn.M21 = shown.M21; drawn.M22 = shown.M22; drawn.M23 = shown.M23;
        drawn.M31 = shown.M31; drawn.M32 = shown.M32; drawn.M33 = shown.M33;
        drawn.M41 = shown.M41; drawn.M42 = shown.M42; drawn.M43 = shown.M43;
        const Matrix4 carry = OpenTK::Mathematics::Inverted(current) * drawn;
        if (NearIdentity(carry)) return std::nullopt;
        return carry;
    }

    std::optional<Matrix4> MotionCarry::By(Vector3 shift)
    {
        if (shift.LengthSquared() <= 0.0F || shift.LengthSquared() > JumpDistance * JumpDistance) return std::nullopt;
        return OpenTK::Mathematics::CreateTranslation(shift);
    }

    Vector3 MotionCarry::Point(Vector3 point, const Matrix4& carry) noexcept
    {
        return Matrix::Vec3MultMtx4(point, carry);
    }

    void MotionCarry::Apply(RenderItem& item, const Matrix4& carry)
    {
        const bool billboard = item.BillboardMode != BillboardMode::None;
        const auto move = [&](Matrix4& matrix)
        {
            if (billboard)
            {
                const Vector3 at = Point(Vector3(matrix.M41, matrix.M42, matrix.M43), carry);
                matrix.M41 = at.X; matrix.M42 = at.Y; matrix.M43 = at.Z;
            }
            else
            {
                matrix = matrix * carry;
            }
        };
        if (item.MatrixStackCount > 0 && item.MatrixStack != nullptr)
        {
            ManagedArray<float>& stack = *item.MatrixStack;
            const std::size_t count = std::min<std::size_t>(static_cast<std::size_t>(item.MatrixStackCount),
                stack.Length() / 16U);
            for (std::size_t i = 0; i < count; ++i)
            {
                Matrix4 matrix{};
                float* values = &matrix.M11;
                for (std::size_t j = 0; j < 16U; ++j) values[j] = stack[i * 16U + j];
                move(matrix);
                for (std::size_t j = 0; j < 16U; ++j) stack[i * 16U + j] = values[j];
            }
        }
        else
        {
            move(item.Transform);
        }
    }
}
