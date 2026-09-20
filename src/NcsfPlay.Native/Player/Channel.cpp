#include "Channel.hpp"

#include "Player.hpp"
#include "SWAVWrapper.hpp"
#include "../NC/SWAV.hpp"

#include <array>
#include <bit>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <limits>
#include <numbers>
#include <span>
#include <stdexcept>

#if defined(_MSC_VER) && (defined(_M_IX86) || defined(_M_X64))
#include <intrin.h>
#endif

namespace
{
    [[noreturn]] void ThrowNullReference()
    {
        throw std::runtime_error("NullReferenceException");
    }

    [[noreturn]] void ThrowIndexOutOfRange()
    {
        throw std::out_of_range("IndexOutOfRangeException");
    }

    [[noreturn]] void ThrowOverflow()
    {
        throw std::overflow_error("OverflowException");
    }

    [[nodiscard]] float FloatAdd(float left, float right) noexcept
    {
        volatile float result = left + right;
        return result;
    }

    [[nodiscard]] float FloatSubtract(float left, float right) noexcept
    {
        volatile float result = left - right;
        return result;
    }

    [[nodiscard]] float FloatMultiply(float left, float right) noexcept
    {
        volatile float result = left * right;
        return result;
    }

    [[nodiscard]] float FloatDivide(float left, float right) noexcept
    {
        volatile float result = left / right;
        return result;
    }

    [[nodiscard]] std::int32_t TruncateToInt32(double value) noexcept
    {
        // RyuJIT's unchecked x86/x64 conversion uses the hardware indefinite
        // value for NaN and values whose truncated result is outside Int32.
        if (!std::isfinite(value)
            || value >= 2147483648.0
            || value <= -2147483649.0)
        {
            return std::numeric_limits<std::int32_t>::min();
        }
        return static_cast<std::int32_t>(value);
    }

    [[nodiscard]] std::uint32_t TruncateToUInt32(double value) noexcept
    {
#if defined(__aarch64__) || defined(_M_ARM64)
        if (std::isnan(value) || value <= 0.0)
            return 0U;
        if (!std::isfinite(value) || value >= 4294967295.0)
            return std::numeric_limits<std::uint32_t>::max();
        return static_cast<std::uint32_t>(value);
#else
        if (!std::isfinite(value)
            || value >= 9223372036854775808.0
            || value < -9223372036854775808.0)
        {
            return 0U;
        }

        const std::int64_t converted = static_cast<std::int64_t>(value);
        return static_cast<std::uint32_t>(static_cast<std::uint64_t>(converted));
#endif
    }

    [[nodiscard]] constexpr std::int32_t WrapMul32(
        std::int32_t left,
        std::int32_t right) noexcept
    {
        return std::bit_cast<std::int32_t>(
            static_cast<std::uint32_t>(left) * static_cast<std::uint32_t>(right));
    }

    [[nodiscard]] constexpr std::int32_t WrapSub32(
        std::int32_t left,
        std::int32_t right) noexcept
    {
        return std::bit_cast<std::int32_t>(
            static_cast<std::uint32_t>(left) - static_cast<std::uint32_t>(right));
    }

    [[nodiscard]] std::int32_t IntAbs(std::int32_t value)
    {
        if (value == std::numeric_limits<std::int32_t>::min())
            ThrowOverflow();
        return value < 0 ? -value : value;
    }

    template <typename T, std::size_t N>
    [[nodiscard]] const T& ArrayAt(const std::array<T, N>& values, std::int32_t index)
    {
        if (index < 0 || static_cast<std::size_t>(index) >= N)
            ThrowIndexOutOfRange();
        return values[static_cast<std::size_t>(index)];
    }

    template <typename T>
    [[nodiscard]] const T& SpanAt(std::span<const T> values, std::int32_t index)
    {
        if (index < 0 || static_cast<std::size_t>(index) >= values.size())
            ThrowIndexOutOfRange();
        return values[static_cast<std::size_t>(index)];
    }

    [[nodiscard]] float Vector128Sum(const std::array<float, 4>& values) noexcept
    {
        // .NET 9 Vector128.Sum intentionally computes Sum(lower) + Sum(upper),
        // and each Vector64.Sum starts from default(float), i.e. +0.0f.
        const float lower = FloatAdd(FloatAdd(0.0F, values[0]), values[1]);
        const float upper = FloatAdd(FloatAdd(0.0F, values[2]), values[3]);
        return FloatAdd(lower, upper);
    }

    [[nodiscard]] bool X86FmaIsSupported() noexcept
    {
#if (defined(__i386__) || defined(__x86_64__)) && (defined(__GNUC__) || defined(__clang__))
        __builtin_cpu_init();
        return __builtin_cpu_supports("avx") && __builtin_cpu_supports("fma");
#elif defined(_MSC_VER) && (defined(_M_IX86) || defined(_M_X64))
        int registers[4]{};
        __cpuid(registers, 1);
        constexpr int OsXSaveBit = 1 << 27;
        constexpr int AvxBit = 1 << 28;
        constexpr int FmaBit = 1 << 12;
        if ((registers[2] & (OsXSaveBit | AvxBit | FmaBit)) != (OsXSaveBit | AvxBit | FmaBit))
            return false;
        return (_xgetbv(0) & 0x6U) == 0x6U;
#else
        return false;
#endif
    }

    [[nodiscard]] bool X86SseIsSupported() noexcept
    {
#if (defined(__i386__) || defined(__x86_64__)) && (defined(__GNUC__) || defined(__clang__))
        __builtin_cpu_init();
        return __builtin_cpu_supports("sse");
#elif defined(_M_X64)
        return true;
#elif defined(_MSC_VER) && defined(_M_IX86)
        int registers[4]{};
        __cpuid(registers, 1);
        return (registers[3] & (1 << 25)) != 0;
#else
        return false;
#endif
    }

    [[nodiscard]] bool MultiplyAddEstimateCanFuse() noexcept
    {
#if defined(__aarch64__) || defined(_M_ARM64)
        return true;
#else
        return X86FmaIsSupported();
#endif
    }

    [[nodiscard]] float MultiplyAddEstimate(float left, float right, float addend) noexcept
    {
        if (MultiplyAddEstimateCanFuse())
            return std::fma(left, right, addend);
        return FloatAdd(FloatMultiply(left, right), addend);
    }

    [[nodiscard]] NCSFPlayer::Player* ConcretePlayer(NCSFCommon::Player* player)
    {
        auto* result = dynamic_cast<NCSFPlayer::Player*>(player);
        if (result == nullptr)
            ThrowNullReference();
        return result;
    }
}

namespace NCSFPlayer
{
    std::array<float, static_cast<std::size_t>(Channel::SincSamples) + 1U> Channel::SincLut{};
    std::array<float, static_cast<std::size_t>(Channel::SincSamples) + 1U> Channel::WindowLut{};
    std::array<float, static_cast<std::size_t>(Channel::SincSamples) + 1U> Channel::FlatTopWindowSincLut{};
    std::array<float, static_cast<std::size_t>(Channel::LanczosSamples) + 1U> Channel::LanczosLut{};

    const std::array<float, 4> Channel::fourPtLagrange_c0multipliers = {0.0F, 1.0F, 0.0F, 0.0F};
    const std::array<float, 4> Channel::fourPtLagrange_c1multipliers =
        {-1.0F / 3.0F, -0.5F, 1.0F, -1.0F / 6.0F};
    const std::array<float, 4> Channel::fourPtLagrange_c2multipliers = {0.5F, -1.0F, 0.5F, 0.0F};
    const std::array<float, 4> Channel::fourPtLagrange_c3multipliers =
        {-1.0F / 6.0F, 0.5F, -0.5F, 1.0F / 6.0F};

    float Channel::Sinc(float x)
    {
        if (x == 0.0F)
            return 1.0F;

        const float numeratorArgument = FloatMultiply(x, std::numbers::pi_v<float>);
        const float denominator = FloatMultiply(x, std::numbers::pi_v<float>);
        return FloatDivide(std::sin(numeratorArgument), denominator);
    }

    Channel::Channel()
    {
        static const bool initialized = []
        {
            const float dx = FloatDivide(1.0F, static_cast<float>(SincResolution));
            float x = 0.0F;

            for (std::int32_t i = 0; i <= SincSamples; ++i, x = FloatAdd(x, dx))
            {
                const float y = FloatDivide(x, static_cast<float>(SincWidth));

                SincLut[static_cast<std::size_t>(i)] =
                    std::fabs(x) < static_cast<float>(SincWidth) ? Sinc(x) : 0.0F;

                const float windowCos1 = std::cos(FloatMultiply(std::numbers::pi_v<float>, y));
                const float windowCos2 = std::cos(
                    FloatMultiply(FloatMultiply(2.0F, std::numbers::pi_v<float>), y));
                WindowLut[static_cast<std::size_t>(i)] = FloatAdd(
                    FloatAdd(0.40897F, FloatMultiply(0.5F, windowCos1)),
                    FloatMultiply(0.09103F, windowCos2));

                const float flatTopSinc =
                    std::fabs(x) < static_cast<float>(SincWidth) ? Sinc(x) : 0.0F;
                const float piY = FloatMultiply(std::numbers::pi_v<float>, y);
                const float flatWindow = FloatAdd(
                    FloatAdd(
                        FloatAdd(
                            FloatAdd(
                                0.21557895F,
                                FloatMultiply(0.41663158F, std::cos(piY))),
                            FloatMultiply(
                                0.27723158F,
                                std::cos(FloatMultiply(piY, 2.0F)))),
                        FloatMultiply(
                            0.083578947F,
                            std::cos(FloatMultiply(piY, 3.0F)))),
                    FloatMultiply(
                        0.006947368F,
                        std::cos(FloatMultiply(piY, 4.0F))));
                FlatTopWindowSincLut[static_cast<std::size_t>(i)] =
                    FloatMultiply(flatTopSinc, flatWindow);

                if (i <= LanczosSamples)
                {
                    LanczosLut[static_cast<std::size_t>(i)] =
                        x < static_cast<float>(Alpha)
                            ? FloatMultiply(Sinc(x), Sinc(FloatDivide(x, static_cast<float>(Alpha))))
                            : 0.0F;
                }
            }

            return true;
        }();

        (void)initialized;
    }

    float Channel::LanczosInterpolate(double ratio)
    {
        if (!swavWrapper)
            ThrowNullReference();

        const std::span<const float> data = swavWrapper->Slice(-Alpha + 1, 2 * Alpha);
        float sum = 0.0F;
        for (std::int32_t i = -Alpha + 1; i <= Alpha; ++i)
        {
            const double distance = std::floor(
                std::fabs(ratio - static_cast<double>(i)) * static_cast<double>(SincResolution));
            const std::int32_t lutIndex = TruncateToInt32(distance);
            const float product = FloatMultiply(
                SpanAt(data, i + Alpha - 1),
                ArrayAt(LanczosLut, lutIndex));
            sum = FloatAdd(sum, product);
        }
        return sum;
    }

    float Channel::SimpleSincInterpolate(double ratio)
    {
        if (!swavWrapper)
            ThrowNullReference();

        const std::span<const float> data = swavWrapper->Slice(-SincWidth + 1, 2 * SincWidth);
        float sum = 0.0F;
        for (std::int32_t i = 0; i < SincWidth * 2; ++i)
        {
            const std::int32_t sampleIndex = i - SincWidth + 1;
            const double distance = std::floor(
                std::fabs(ratio - static_cast<double>(sampleIndex)) * static_cast<double>(SincResolution));
            const std::int32_t lutIndex = TruncateToInt32(distance);
            const float product = FloatMultiply(
                SpanAt(data, i),
                ArrayAt(FlatTopWindowSincLut, lutIndex));
            sum = FloatAdd(sum, product);
        }
        return sum;
    }

    float Channel::OldSincInterpolate(double ratio)
    {
        if (!swavWrapper)
            ThrowNullReference();

        const std::span<const float> data = swavWrapper->Slice(-SincWidth + 1, 2 * SincWidth);
        std::array<float, static_cast<std::size_t>(SincWidth * 2)> kernel{};
        float kernelSum = 0.0F;
        const std::int32_t shift = TruncateToInt32(
            std::floor(ratio * static_cast<double>(SincResolution)));
        const double sampleIncrease = Register().SampleIncrease();
        const std::int32_t step = sampleIncrease > 1.0
            ? TruncateToInt32(static_cast<double>(SincResolution) / sampleIncrease)
            : SincResolution;
        const std::int32_t shiftAdj = WrapMul32(shift, step) / SincResolution;
        const std::int32_t windowStep = SincResolution;

        for (std::int32_t i = SincWidth; i >= -(SincWidth - 1); --i)
        {
            const std::int32_t pos = WrapMul32(i, step);
            const std::int32_t windowPos = i * windowStep;
            const float kernelValue = FloatMultiply(
                ArrayAt(SincLut, IntAbs(WrapSub32(shiftAdj, pos))),
                ArrayAt(WindowLut, IntAbs(WrapSub32(shift, windowPos))));
            kernel[static_cast<std::size_t>(i + SincWidth - 1)] = kernelValue;
            kernelSum = FloatAdd(kernelSum, kernelValue);
        }

        float sum = 0.0F;
        for (std::int32_t i = 0; i < SincWidth * 2; ++i)
        {
            sum = FloatAdd(
                sum,
                FloatMultiply(SpanAt(data, i), kernel[static_cast<std::size_t>(i)]));
        }
        return FloatDivide(sum, kernelSum);
    }

    float Channel::SixPointLagrangeInterpolate(double ratio)
    {
        if (!swavWrapper)
            ThrowNullReference();

        const std::span<const float> data = swavWrapper->Slice(-2, 6);
        ratio -= 0.5;

        const float even1 = FloatAdd(SpanAt(data, 0), SpanAt(data, 5));
        const float odd1 = FloatSubtract(SpanAt(data, 0), SpanAt(data, 5));
        const float even2 = FloatAdd(SpanAt(data, 1), SpanAt(data, 4));
        const float odd2 = FloatSubtract(SpanAt(data, 1), SpanAt(data, 4));
        const float even3 = FloatAdd(SpanAt(data, 2), SpanAt(data, 3));
        const float odd3 = FloatSubtract(SpanAt(data, 2), SpanAt(data, 3));

        const float c0 = FloatAdd(
            FloatSubtract(
                FloatMultiply(0.01171875F, even1),
                FloatMultiply(0.09765625F, even2)),
            FloatMultiply(0.5859375F, even3));

        const float c1 = FloatSubtract(
            FloatSubtract(
                FloatMultiply(FloatDivide(25.0F, 384.0F), odd2),
                FloatMultiply(1.171875F, odd3)),
            FloatMultiply(0.0046875F, odd1));

        const float c2 = FloatSubtract(
            FloatSubtract(
                FloatMultiply(0.40625F, even2),
                FloatMultiply(FloatDivide(17.0F, 48.0F), even3)),
            FloatMultiply(FloatDivide(5.0F, 96.0F), even1));

        const float c3 = FloatAdd(
            FloatSubtract(
                FloatDivide(odd1, 48.0F),
                FloatMultiply(FloatDivide(13.0F, 48.0F), odd2)),
            FloatMultiply(FloatDivide(17.0F, 24.0F), odd3));

        const float c4 = FloatAdd(
            FloatSubtract(
                FloatDivide(even1, 48.0F),
                FloatMultiply(0.0625F, even2)),
            FloatDivide(even3, 24.0F));

        const float c5 = FloatSubtract(
            FloatSubtract(
                FloatDivide(odd2, 24.0F),
                FloatDivide(odd3, 12.0F)),
            FloatDivide(odd1, 120.0F));

        const float ratioFloat = static_cast<float>(ratio);
        return std::fma(
            std::fma(
                std::fma(
                    std::fma(
                        std::fma(c5, ratioFloat, c4),
                        ratioFloat,
                        c3),
                    ratioFloat,
                    c2),
                ratioFloat,
                c1),
            ratioFloat,
            c0);
    }

    float Channel::FourPointInterpolate(double ratio)
    {
        if (!swavWrapper)
            ThrowNullReference();

        const std::span<const float> data = swavWrapper->Slice(-1, 4);
        const float ratioFloat = static_cast<float>(ratio);

        if (X86FmaIsSupported())
        {
            std::array<float, 4> c0{};
            std::array<float, 4> c1{};
            std::array<float, 4> c2{};
            std::array<float, 4> c3{};
            std::array<float, 4> result{};

            for (std::int32_t i = 0; i < 4; ++i)
            {
                const std::size_t index = static_cast<std::size_t>(i);
                const float value = SpanAt(data, i);
                c0[index] = FloatMultiply(value, fourPtLagrange_c0multipliers[index]);
                c1[index] = FloatMultiply(value, fourPtLagrange_c1multipliers[index]);
                c2[index] = FloatMultiply(value, fourPtLagrange_c2multipliers[index]);
                c3[index] = FloatMultiply(value, fourPtLagrange_c3multipliers[index]);

                result[index] = std::fma(
                    std::fma(
                        std::fma(c3[index], ratioFloat, c2[index]),
                        ratioFloat,
                        c1[index]),
                    ratioFloat,
                    c0[index]);
            }

            return Vector128Sum(result);
        }

        if (X86SseIsSupported())
        {
            std::array<float, 4> c0{};
            std::array<float, 4> c1{};
            std::array<float, 4> c2{};
            std::array<float, 4> c3{};
            std::array<float, 4> result{};

            const float ratioSquaredForC3 = FloatMultiply(ratioFloat, ratioFloat);
            const float ratioCubed = FloatMultiply(ratioFloat, ratioSquaredForC3);
            const float ratioSquaredForC2 = FloatMultiply(ratioFloat, ratioFloat);

            for (std::int32_t i = 0; i < 4; ++i)
            {
                const std::size_t index = static_cast<std::size_t>(i);
                const float value = SpanAt(data, i);
                c0[index] = FloatMultiply(value, fourPtLagrange_c0multipliers[index]);
                c1[index] = FloatMultiply(value, fourPtLagrange_c1multipliers[index]);
                c2[index] = FloatMultiply(value, fourPtLagrange_c2multipliers[index]);
                c3[index] = FloatMultiply(value, fourPtLagrange_c3multipliers[index]);

                const float c3Tmp = FloatMultiply(c3[index], ratioCubed);
                const float c2Tmp = FloatMultiply(c2[index], ratioSquaredForC2);
                const float c1Tmp = FloatMultiply(c1[index], ratioFloat);
                result[index] = FloatAdd(FloatAdd(FloatAdd(c3Tmp, c2Tmp), c1Tmp), c0[index]);
            }

            return Vector128Sum(result);
        }

        const float c0 = SpanAt(data, 1);
        const float c1 = FloatSubtract(
            FloatSubtract(
                FloatSubtract(
                    SpanAt(data, 2),
                    FloatDivide(SpanAt(data, 0), 3.0F)),
                FloatMultiply(0.5F, SpanAt(data, 1))),
            FloatDivide(SpanAt(data, 3), 6.0F));
        const float c2 = FloatSubtract(
            FloatMultiply(0.5F, FloatAdd(SpanAt(data, 0), SpanAt(data, 2))),
            SpanAt(data, 1));
        const float c3 = FloatAdd(
            FloatDivide(FloatSubtract(SpanAt(data, 3), SpanAt(data, 0)), 6.0F),
            FloatMultiply(0.5F, FloatSubtract(SpanAt(data, 1), SpanAt(data, 2))));

        return std::fma(
            std::fma(
                std::fma(c3, ratioFloat, c2),
                ratioFloat,
                c1),
            ratioFloat,
            c0);
    }

    float Channel::LinearInterpolate(double ratio)
    {
        if (!swavWrapper)
            ThrowNullReference();

        const std::span<const float> data = swavWrapper->Slice(0, 2);
        const float amount = static_cast<float>(ratio);
        const float right = FloatMultiply(SpanAt(data, 1), amount);
        return MultiplyAddEstimate(
            SpanAt(data, 0),
            FloatSubtract(1.0F, amount),
            right);
    }

    float Channel::Interpolate()
    {
        double ratio = Register().SamplePosition();
        ratio -= static_cast<double>(TruncateToInt32(ratio));

        NCSFPlayer::Player* player = ConcretePlayer(NCSFCommon::Channel::Player());
        switch (player->Interpolation())
        {
        case Interpolation::Lanczos:
            return LanczosInterpolate(ratio);
        case Interpolation::SimpleSinc:
            return SimpleSincInterpolate(ratio);
        case Interpolation::Sinc:
            return OldSincInterpolate(ratio);
        case Interpolation::SixPointLagrange:
            return SixPointLagrangeInterpolate(ratio);
        case Interpolation::FourPointLagrange:
            return FourPointInterpolate(ratio);
        case Interpolation::Linear:
            return LinearInterpolate(ratio);
        default:
            return 0.0F;
        }
    }

    float Channel::GenerateSample()
    {
        if (Register().SamplePosition() < 0.0)
            return 0.0F;

        if (Register().Format() != 3U)
        {
            NCSFPlayer::Player* player = ConcretePlayer(NCSFCommon::Channel::Player());
            if (player->Interpolation() == Interpolation::None)
            {
                const auto& source = Register().Source();
                if (!source)
                    ThrowNullReference();
                return SpanAt(source->Data(), TruncateToInt32(Register().SamplePosition()));
            }
            return Interpolate();
        }

        if (Id() < 8U)
            return 0.0F;

        if (Id() < 14U)
        {
            const auto& duty = ArrayAt(WaveDutyTable, static_cast<std::int32_t>(Register().WaveDuty()));
            const std::int32_t position = TruncateToInt32(Register().SamplePosition());
            const std::size_t dutyIndex = static_cast<std::size_t>(static_cast<std::uint32_t>(position) & 0x7U);
            return duty[dutyIndex];
        }

        const std::uint32_t currentPosition = TruncateToUInt32(Register().SamplePosition());
        if (Register().PSGLastCount() != currentPosition)
        {
            const std::uint32_t max = currentPosition;
            for (std::uint32_t i = Register().PSGLastCount(); i < max; ++i)
            {
                if ((Register().PSGX() & 1U) != 0U)
                {
                    Register().PSGX(static_cast<std::uint16_t>((Register().PSGX() >> 1U) ^ 0x6000U));
                    Register().PSGLast(-1.0F);
                }
                else
                {
                    Register().PSGX(static_cast<std::uint16_t>(Register().PSGX() >> 1U));
                    Register().PSGLast(1.0F);
                }
            }

            Register().PSGLastCount(currentPosition);
        }

        return Register().PSGLast();
    }

    void Channel::IncrementSample()
    {
        const double samplePosition = Register().SamplePosition() + Register().SampleIncrease();

        if (Register().Format() != 3U)
        {
            NCSFPlayer::Player* player = ConcretePlayer(NCSFCommon::Channel::Player());
            if (player->Interpolation() != Interpolation::None
                && Register().SamplePosition() < 0.0
                && samplePosition >= 0.0)
            {
                swavWrapper = std::make_shared<SWAVWrapper>(&Register());
            }
        }

        Register().SamplePosition(samplePosition);

        if (Register().Format() != 3U
            && Register().SamplePosition() >= static_cast<double>(Register().TotalLength()))
        {
            if (Register().RepeatMode() == 1U)
            {
                while (Register().SamplePosition() >= static_cast<double>(Register().TotalLength()))
                {
                    Register().SamplePosition(
                        Register().SamplePosition() - static_cast<double>(Register().Length()));
                }
            }
            else
            {
                Kill();
                swavWrapper.reset();
            }
        }
    }
}
