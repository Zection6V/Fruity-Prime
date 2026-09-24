#include "Mathematics.hpp"

#include "../System/Globalization.hpp"

#include <array>
#include <cmath>
#include <limits>
#include <string>

namespace OpenTK::Mathematics
{
    constinit const Vector2 Vector2::Zero{};
    constinit const Vector3 Vector3::Zero{};
    constinit const Vector3 Vector3::UnitX{1.0F, 0.0F, 0.0F};
    constinit const Vector3 Vector3::UnitY{0.0F, 1.0F, 0.0F};
    constinit const Vector3 Vector3::UnitZ{0.0F, 0.0F, 1.0F};
    constinit const Vector4 Vector4::Zero{};
    constinit const Matrix4x3 Matrix4x3::Zero{};
    constinit const Matrix4 Matrix4::Zero{};

    Vector3 Vector3::Normalized() const
    {
        const float inverse = 1.0F / std::sqrt((X * X) + (Y * Y) + (Z * Z));
        return Vector3(X * inverse, Y * inverse, Z * inverse);
    }

    std::string Vector3::ToString() const
    {
        // MathHelper.GetListSeparator: a comma, unless the culture already
        // spells a decimal point that way.
        const std::string decimalSeparator = MphRead::NativeRuntime::CurrentDecimalSeparator();
        const char listSeparator
            = (!decimalSeparator.empty() && decimalSeparator.front() == ',') ? ';' : ',';
        return "(" + MphRead::NativeRuntime::SingleToString(X) + listSeparator + " "
            + MphRead::NativeRuntime::SingleToString(Y) + listSeparator + " "
            + MphRead::NativeRuntime::SingleToString(Z) + ")";
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

    Matrix4 CreateFromAxisAngle(Vector3 axis, float angle) noexcept
    {
        axis = Normalize(axis);
        const float axisX = axis.X;
        const float axisY = axis.Y;
        const float axisZ = axis.Z;
        const float cos = std::cos(-angle);
        const float sin = std::sin(-angle);
        const float t = 1.0F - cos;
        const float tXX = t * axisX * axisX;
        const float tXY = t * axisX * axisY;
        const float tXZ = t * axisX * axisZ;
        const float tYY = t * axisY * axisY;
        const float tYZ = t * axisY * axisZ;
        const float tZZ = t * axisZ * axisZ;
        const float sinX = sin * axisX;
        const float sinY = sin * axisY;
        const float sinZ = sin * axisZ;
        Matrix4 result{};
        result.M11 = tXX + cos;
        result.M12 = tXY - sinZ;
        result.M13 = tXZ + sinY;
        result.M21 = tXY + sinZ;
        result.M22 = tYY + cos;
        result.M23 = tYZ - sinX;
        result.M31 = tXZ - sinY;
        result.M32 = tYZ + sinX;
        result.M33 = tZZ + cos;
        result.M44 = 1.0F;
        return result;
    }

    float Determinant(const Matrix4& matrix) noexcept
    {
        const float m11 = matrix.M11, m12 = matrix.M12, m13 = matrix.M13, m14 = matrix.M14;
        const float m21 = matrix.M21, m22 = matrix.M22, m23 = matrix.M23, m24 = matrix.M24;
        const float m31 = matrix.M31, m32 = matrix.M32, m33 = matrix.M33, m34 = matrix.M34;
        const float m41 = matrix.M41, m42 = matrix.M42, m43 = matrix.M43, m44 = matrix.M44;
        return
            (m11 * m22 * m33 * m44) - (m11 * m22 * m34 * m43) + (m11 * m23 * m34 * m42) - (m11 * m23 * m32 * m44)
            + (m11 * m24 * m32 * m43) - (m11 * m24 * m33 * m42) - (m12 * m23 * m34 * m41) + (m12 * m23 * m31 * m44)
            - (m12 * m24 * m31 * m43) + (m12 * m24 * m33 * m41) - (m12 * m21 * m33 * m44) + (m12 * m21 * m34 * m43)
            + (m13 * m24 * m31 * m42) - (m13 * m24 * m32 * m41) + (m13 * m21 * m32 * m44) - (m13 * m21 * m34 * m42)
            + (m13 * m22 * m34 * m41) - (m13 * m22 * m31 * m44) - (m14 * m21 * m32 * m43) + (m14 * m21 * m33 * m42)
            - (m14 * m22 * m33 * m41) + (m14 * m22 * m31 * m43) - (m14 * m23 * m31 * m42) + (m14 * m23 * m32 * m41);
    }

    namespace
    {
        [[noreturn]] void ThrowSingular()
        {
            throw System::InvalidOperationException("Matrix is singular and cannot be inverted.");
        }

#if defined(__x86_64__) || defined(_M_X64) || defined(__i386__) || defined(_M_IX86)
        // Matrix4.InvertSse3, one 128-bit lane at a time. Every SSE
        // instruction there is a lane-wise single-precision operation (or a
        // shuffle), so doing each lane in scalar float, in the same order,
        // rounds exactly as the SIMD does.
        using Lanes = std::array<float, 4>;

        // _mm_shuffle_ps(a, b, imm): two lanes of a, then two of b.
        [[nodiscard]] Lanes Shuffle(const Lanes& a, const Lanes& b, unsigned imm) noexcept
        {
            return { a[imm & 3U], a[(imm >> 2) & 3U], b[(imm >> 4) & 3U], b[(imm >> 6) & 3U] };
        }

        // _mm_shuffle_epi32(v, imm): four lanes of v.
        [[nodiscard]] Lanes Swizzle(const Lanes& v, unsigned imm) noexcept
        {
            return { v[imm & 3U], v[(imm >> 2) & 3U], v[(imm >> 4) & 3U], v[(imm >> 6) & 3U] };
        }

        [[nodiscard]] Lanes Mul(const Lanes& a, const Lanes& b) noexcept
        {
            return { a[0] * b[0], a[1] * b[1], a[2] * b[2], a[3] * b[3] };
        }

        [[nodiscard]] Lanes Sub(const Lanes& a, const Lanes& b) noexcept
        {
            return { a[0] - b[0], a[1] - b[1], a[2] - b[2], a[3] - b[3] };
        }

        [[nodiscard]] Lanes AddLanes(const Lanes& a, const Lanes& b) noexcept
        {
            return { a[0] + b[0], a[1] + b[1], a[2] + b[2], a[3] + b[3] };
        }

        [[nodiscard]] Lanes Div(const Lanes& a, const Lanes& b) noexcept
        {
            return { a[0] / b[0], a[1] / b[1], a[2] / b[2], a[3] / b[3] };
        }

        // _mm_hadd_ps(a, b).
        [[nodiscard]] Lanes HorizontalAdd(const Lanes& a, const Lanes& b) noexcept
        {
            return { a[0] + a[1], a[2] + a[3], b[0] + b[1], b[2] + b[3] };
        }

        [[nodiscard]] Matrix4 InvertSse3(const Matrix4& mat)
        {
            const Lanes row0{ mat.M11, mat.M12, mat.M13, mat.M14 };
            const Lanes row1{ mat.M21, mat.M22, mat.M23, mat.M24 };
            const Lanes row2{ mat.M31, mat.M32, mat.M33, mat.M34 };
            const Lanes row3{ mat.M41, mat.M42, mat.M43, mat.M44 };

            // Sse.MoveLowToHigh(a, b) is (a0, a1, b0, b1);
            // Sse.MoveHighToLow(a, b) is (b2, b3, a2, a3).
            const Lanes A{ row0[0], row0[1], row1[0], row1[1] };
            const Lanes B{ row0[2], row0[3], row1[2], row1[3] };
            const Lanes C{ row2[0], row2[1], row3[0], row3[1] };
            const Lanes D{ row2[2], row2[3], row3[2], row3[3] };

            const Lanes detSub = Sub(
                Mul(Shuffle(row0, row2, 0b1000'1000U), Shuffle(row1, row3, 0b1101'1101U)),
                Mul(Shuffle(row0, row2, 0b1101'1101U), Shuffle(row1, row3, 0b1000'1000U)));

            const Lanes detA = Swizzle(detSub, 0b0000'0000U);
            const Lanes detB = Swizzle(detSub, 0b0101'0101U);
            const Lanes detC = Swizzle(detSub, 0b1010'1010U);
            const Lanes detD = Swizzle(detSub, 0b1111'1111U);

            const Lanes D_C = Sub(
                Mul(Swizzle(D, 0b0000'1111U), C),
                Mul(Swizzle(D, 0b1010'0101U), Swizzle(C, 0b0100'1110U)));
            const Lanes A_B = Sub(
                Mul(Swizzle(A, 0b0000'1111U), B),
                Mul(Swizzle(A, 0b1010'0101U), Swizzle(B, 0b0100'1110U)));

            Lanes X_ = Sub(
                Mul(detD, A),
                AddLanes(
                    Mul(B, Swizzle(D_C, 0b1100'1100U)),
                    Mul(Swizzle(B, 0b1011'0001U), Swizzle(D_C, 0b0110'0110U))));
            Lanes W_ = Sub(
                Mul(detA, D),
                AddLanes(
                    Mul(C, Swizzle(A_B, 0b1100'1100U)),
                    Mul(Swizzle(C, 0b1011'0001U), Swizzle(A_B, 0b0110'0110U))));

            Lanes detM = Mul(detA, detD);

            Lanes Y_ = Sub(
                Mul(detB, C),
                Sub(
                    Mul(D, Swizzle(A_B, 0b0011'0011U)),
                    Mul(Swizzle(D, 0b1011'0001U), Swizzle(A_B, 0b0110'0110U))));
            Lanes Z_ = Sub(
                Mul(detC, B),
                Sub(
                    Mul(A, Swizzle(D_C, 0b0011'0011U)),
                    Mul(Swizzle(A, 0b1011'0001U), Swizzle(D_C, 0b0110'0110U))));

            detM = AddLanes(detM, Mul(detB, detC));

            Lanes tr = Mul(A_B, Swizzle(D_C, 0b1101'1000U));
            tr = HorizontalAdd(tr, tr);
            tr = HorizontalAdd(tr, tr);

            detM = Sub(detM, tr);

            if (std::fabs(detM[0]) < std::numeric_limits<float>::denorm_min())
            {
                ThrowSingular();
            }

            const Lanes rDetM = Div(Lanes{ 1.0F, -1.0F, -1.0F, 1.0F }, detM);
            X_ = Mul(X_, rDetM);
            Y_ = Mul(Y_, rDetM);
            Z_ = Mul(Z_, rDetM);
            W_ = Mul(W_, rDetM);

            const Lanes r0 = Shuffle(X_, Y_, 0b0111'0111U);
            const Lanes r1 = Shuffle(X_, Y_, 0b0010'0010U);
            const Lanes r2 = Shuffle(Z_, W_, 0b0111'0111U);
            const Lanes r3 = Shuffle(Z_, W_, 0b0010'0010U);
            return Matrix4(
                Vector4(r0[0], r0[1], r0[2], r0[3]),
                Vector4(r1[0], r1[1], r1[2], r1[3]),
                Vector4(r2[0], r2[1], r2[2], r2[3]),
                Vector4(r3[0], r3[1], r3[2], r3[3]));
        }
#else
        // Matrix4.InvertFallback (System.Numerics' Matrix4x4.Invert).
        [[nodiscard]] Matrix4 InvertFallback(const Matrix4& mat)
        {
            const float a = mat.M11, b = mat.M21, c = mat.M31, d = mat.M41;
            const float e = mat.M12, f = mat.M22, g = mat.M32, h = mat.M42;
            const float i = mat.M13, j = mat.M23, k = mat.M33, l = mat.M43;
            const float m = mat.M14, n = mat.M24, o = mat.M34, p = mat.M44;

            const float kp_lo = k * p - l * o;
            const float jp_ln = j * p - l * n;
            const float jo_kn = j * o - k * n;
            const float ip_lm = i * p - l * m;
            const float io_km = i * o - k * m;
            const float in_jm = i * n - j * m;

            const float a11 = +(f * kp_lo - g * jp_ln + h * jo_kn);
            const float a12 = -(e * kp_lo - g * ip_lm + h * io_km);
            const float a13 = +(e * jp_ln - f * ip_lm + h * in_jm);
            const float a14 = -(e * jo_kn - f * io_km + g * in_jm);

            const float det = a * a11 + b * a12 + c * a13 + d * a14;
            if (std::fabs(det) < std::numeric_limits<float>::denorm_min())
            {
                ThrowSingular();
            }
            const float invDet = 1.0F / det;

            const float gp_ho = g * p - h * o;
            const float fp_hn = f * p - h * n;
            const float fo_gn = f * o - g * n;
            const float ep_hm = e * p - h * m;
            const float eo_gm = e * o - g * m;
            const float en_fm = e * n - f * m;

            const float gl_hk = g * l - h * k;
            const float fl_hj = f * l - h * j;
            const float fk_gj = f * k - g * j;
            const float el_hi = e * l - h * i;
            const float ek_gi = e * k - g * i;
            const float ej_fi = e * j - f * i;

            return Matrix4(
                Vector4(a11 * invDet, a12 * invDet, a13 * invDet, a14 * invDet),
                Vector4(
                    -(b * kp_lo - c * jp_ln + d * jo_kn) * invDet,
                    +(a * kp_lo - c * ip_lm + d * io_km) * invDet,
                    -(a * jp_ln - b * ip_lm + d * in_jm) * invDet,
                    +(a * jo_kn - b * io_km + c * in_jm) * invDet),
                Vector4(
                    +(b * gp_ho - c * fp_hn + d * fo_gn) * invDet,
                    -(a * gp_ho - c * ep_hm + d * eo_gm) * invDet,
                    +(a * fp_hn - b * ep_hm + d * en_fm) * invDet,
                    -(a * fo_gn - b * eo_gm + c * en_fm) * invDet),
                Vector4(
                    -(b * gl_hk - c * fl_hj + d * fk_gj) * invDet,
                    +(a * gl_hk - c * el_hi + d * ek_gi) * invDet,
                    -(a * fl_hj - b * el_hi + d * ej_fi) * invDet,
                    +(a * fk_gj - b * ek_gi + c * ej_fi) * invDet));
        }
#endif
    }

    Matrix4 Invert(const Matrix4& matrix)
    {
#if defined(__x86_64__) || defined(_M_X64) || defined(__i386__) || defined(_M_IX86)
        return InvertSse3(matrix);
#else
        return InvertFallback(matrix);
#endif
    }

    Matrix4 Inverted(const Matrix4& matrix)
    {
        if (Determinant(matrix) != 0.0F)
        {
            return Invert(matrix);
        }
        return matrix;
    }

}
