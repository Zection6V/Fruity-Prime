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

    std::optional<Matrix4> MotionCarry::Along(const Matrix4& from, const Matrix4& start, const Matrix4& end,
        float fraction)
    {
        if (!(fraction > 0.0F)) return std::nullopt;
        return AlongSplit(from, start, end, fraction, fraction);
    }

    std::optional<Matrix4> MotionCarry::AlongSplit(const Matrix4& from, const Matrix4& start, const Matrix4& end,
        float move, float turnFraction)
    {
        const Vector3 moved = Vector3(end.M41, end.M42, end.M43) - Vector3(start.M41, start.M42, start.M43);
        if (moved.LengthSquared() > JumpDistance * JumpDistance) return std::nullopt;
        // The motion's turn, D with end = start * D, a fraction of it on `from`.
        Matrix4 turned = Rotation(from);
        const Matrix4 turn = OpenTK::Mathematics::Inverted(Rotation(start)) * Rotation(end);
        if (IsRotation(turn))
        {
            if (const auto part = Power(turn, turnFraction)) turned = turned * *part;
        }
        // Rotation and Inverted work on the upper 3x3; the carry keeps the
        // pose's scale and other columns as they were.
        Matrix4 drawn = from;
        drawn.M11 = turned.M11; drawn.M12 = turned.M12; drawn.M13 = turned.M13;
        drawn.M21 = turned.M21; drawn.M22 = turned.M22; drawn.M23 = turned.M23;
        drawn.M31 = turned.M31; drawn.M32 = turned.M32; drawn.M33 = turned.M33;
        drawn.M41 = from.M41 + moved.X * move;
        drawn.M42 = from.M42 + moved.Y * move;
        drawn.M43 = from.M43 + moved.Z * move;
        const Matrix4 carry = OpenTK::Mathematics::Inverted(from) * drawn;
        if (NearIdentity(carry)) return std::nullopt;
        return carry;
    }

    std::optional<Matrix4> MotionCarry::Between(const Matrix4& previous, const Matrix4& current, float fraction)
    {
        return Along(current, previous, current, fraction);
    }

    std::optional<Matrix4> MotionCarry::BetweenShown(const Matrix4& previous, const Matrix4& current,
        float move, float turnFraction)
    {
        return AlongSplit(current, previous, current, move > 0.0F ? move : 0.0F, turnFraction);
    }

    std::optional<Matrix4> MotionCarry::Toward(const Matrix4& current, const Matrix4& next, float fraction)
    {
        return Along(current, current, next, fraction);
    }

    Matrix4 MotionCarry::Anchor(Vector3 from, Vector3 fromDirection, Vector3 to, Vector3 toDirection)
    {
        // The shortest turn from one direction onto the other, about `from`,
        // then the move onto `to`. Row-vector: p * T(-from) * R * T(to).
        Matrix4 turn = Identity;
        const float a = std::sqrt(fromDirection.LengthSquared()), b = std::sqrt(toDirection.LengthSquared());
        if (a > 0.0F && b > 0.0F)
        {
            const Vector3 u(fromDirection.X / a, fromDirection.Y / a, fromDirection.Z / a);
            const Vector3 v(toDirection.X / b, toDirection.Y / b, toDirection.Z / b);
            const Vector3 axis = Vector3::Cross(u, v);
            const float s = std::sqrt(axis.LengthSquared());
            const float c = std::clamp(Vector3::Dot(u, v), -1.0F, 1.0F);
            if (s > 1e-6F)
            {
                const float x = axis.X / s, y = axis.Y / s, z = axis.Z / s, t = 1.0F - c;
                // Rodrigues in column form, stored transposed for row vectors.
                turn.M11 = t * x * x + c;     turn.M21 = t * x * y - s * z; turn.M31 = t * x * z + s * y;
                turn.M12 = t * x * y + s * z; turn.M22 = t * y * y + c;     turn.M32 = t * y * z - s * x;
                turn.M13 = t * x * z - s * y; turn.M23 = t * y * z + s * x; turn.M33 = t * z * z + c;
            }
        }
        return OpenTK::Mathematics::CreateTranslation(-from) * turn * OpenTK::Mathematics::CreateTranslation(to);
    }

    Matrix4 MotionCarry::Span(Vector3 fromA, Vector3 fromB, Vector3 toA, Vector3 toB)
    {
        const Vector3 before = fromB - fromA, after = toB - toA;
        const float a = std::sqrt(before.LengthSquared()), b = std::sqrt(after.LengthSquared());
        if (a <= 1e-6F || b <= 1e-6F) return Anchor(fromA, before, toA, after);
        // About fromA: turn, then stretch, then onto toA (Anchor's move last).
        const float stretch = b / a;
        const Matrix4 scale = OpenTK::Mathematics::CreateScale(stretch);
        return OpenTK::Mathematics::CreateTranslation(-fromA) * Anchor(Vector3{}, before, Vector3{}, after)
            * scale * OpenTK::Mathematics::CreateTranslation(toA);
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
