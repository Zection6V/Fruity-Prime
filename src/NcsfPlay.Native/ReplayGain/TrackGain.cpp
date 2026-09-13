#include "TrackGain.hpp"
#include "ReplayGain.hpp"

#include <algorithm>
#include <bit>
#include <cmath>
#include <cstddef>
#include <cstring>
#include <limits>
#include <stdexcept>

#if defined(__i386__) || defined(__x86_64__) || defined(_M_IX86) || defined(_M_X64)
#include <immintrin.h>
#if defined(_MSC_VER)
#include <intrin.h>
#endif
#endif

namespace NCSFCommon::ReplayGain
{
    namespace
    {
#if defined(__i386__) || defined(__x86_64__) || defined(_M_IX86) || defined(_M_X64)
        [[nodiscard]] bool AvxIsSupported() noexcept
        {
#if defined(__GNUC__) || defined(__clang__)
            return __builtin_cpu_supports("avx") != 0;
#elif defined(_MSC_VER)
            int registers[4]{};
            __cpuid(registers, 1);
            constexpr int OsxsaveBit = 1 << 27;
            constexpr int AvxBit = 1 << 28;
            if ((registers[2] & (OsxsaveBit | AvxBit)) != (OsxsaveBit | AvxBit))
                return false;
            return (_xgetbv(0) & 0x6) == 0x6;
#else
            return false;
#endif
        }

        [[nodiscard]] bool Sse2IsSupported() noexcept
        {
#if defined(__x86_64__) || defined(_M_X64)
            return true;
#elif defined(__GNUC__) || defined(__clang__)
            return __builtin_cpu_supports("sse2") != 0;
#elif defined(_MSC_VER)
            int registers[4]{};
            __cpuid(registers, 1);
            return (registers[3] & (1 << 26)) != 0;
#else
            return false;
#endif
        }

#if defined(__GNUC__) || defined(__clang__)
        __attribute__((target("avx")))
#endif
        void ConvertFourInt32ToDoubleAvx(const std::int32_t* input, double* output) noexcept
        {
            const __m128i values = _mm_loadu_si128(reinterpret_cast<const __m128i*>(input));
            _mm256_storeu_pd(output, _mm256_cvtepi32_pd(values));
        }

#if defined(__GNUC__) || defined(__clang__)
        __attribute__((target("avx")))
#endif
        double Sum16Avx(const double* values) noexcept
        {
            const __m256d vector0 = _mm256_loadu_pd(values);
            const __m256d vector1 = _mm256_loadu_pd(values + 4);
            const __m256d vector2 = _mm256_loadu_pd(values + 8);
            const __m256d vector3 = _mm256_loadu_pd(values + 12);

            __m256d sum = _mm256_mul_pd(vector0, vector0);
            sum = _mm256_add_pd(sum, _mm256_mul_pd(vector1, vector1));
            sum = _mm256_add_pd(sum, _mm256_mul_pd(vector2, vector2));
            sum = _mm256_add_pd(sum, _mm256_mul_pd(vector3, vector3));

            alignas(32) double lanes[4];
            _mm256_store_pd(lanes, sum);
            return (lanes[0] + lanes[1]) + (lanes[2] + lanes[3]);
        }

#if defined(__GNUC__) || defined(__clang__)
        __attribute__((target("sse2")))
#endif
        double Sum16Sse2(const double* values) noexcept
        {
            __m128d sum = _mm_mul_pd(_mm_loadu_pd(values), _mm_loadu_pd(values));
            for (std::int32_t i = 1; i < 8; ++i)
            {
                const __m128d value = _mm_loadu_pd(values + static_cast<std::ptrdiff_t>(i) * 2);
                sum = _mm_add_pd(sum, _mm_mul_pd(value, value));
            }

            alignas(16) double lanes[2];
            _mm_store_pd(lanes, sum);
            return lanes[0] + lanes[1];
        }
#else
        [[nodiscard]] constexpr bool AvxIsSupported() noexcept
        {
            return false;
        }

        [[nodiscard]] constexpr bool Sse2IsSupported() noexcept
        {
            return false;
        }
#endif

        void IncrementUnchecked(std::int32_t& value) noexcept
        {
            std::uint32_t bits = std::bit_cast<std::uint32_t>(value);
            ++bits;
            value = std::bit_cast<std::int32_t>(bits);
        }
    }

    TrackGain::TrackGain(std::int32_t sampleRate, std::int32_t sampleSize)
        : lInPreBuf(static_cast<std::size_t>(ReplayGain::MaxOrder * 2)),
          lInPrePos(ReplayGain::MaxOrder),
          lStepBuf(static_cast<std::size_t>(ReplayGain::MaxSamplesPerWindow + ReplayGain::MaxOrder)),
          lStepPos(ReplayGain::MaxOrder),
          lOutBuf(static_cast<std::size_t>(ReplayGain::MaxSamplesPerWindow + ReplayGain::MaxOrder)),
          lOutPos(ReplayGain::MaxOrder),
          rInPreBuf(static_cast<std::size_t>(ReplayGain::MaxOrder * 2)),
          rInPrePos(ReplayGain::MaxOrder),
          rStepBuf(static_cast<std::size_t>(ReplayGain::MaxSamplesPerWindow + ReplayGain::MaxOrder)),
          rStepPos(ReplayGain::MaxOrder),
          rOutBuf(static_cast<std::size_t>(ReplayGain::MaxSamplesPerWindow + ReplayGain::MaxOrder)),
          rOutPos(ReplayGain::MaxOrder)
    {
        if (!ReplayGain::IsSupportedFormat(sampleRate, sampleSize))
            throw std::runtime_error("Unsupported format. Supported sample sizes are 16, 24.");

        const auto iterator = std::find_if(
            ReplayGain::FreqInfos->begin(),
            ReplayGain::FreqInfos->end(),
            [sampleRate](const std::shared_ptr<FrequencyInfo>& info)
            {
                return static_cast<std::int64_t>(info->SampleRate()) == static_cast<std::int64_t>(sampleRate);
            });
        const auto index = static_cast<std::size_t>(std::distance(ReplayGain::FreqInfos->begin(), iterator));
        freqInfo = ReplayGain::FreqInfos->at(index);

        this->sampleSize = sampleSize;
        sampleWindow = static_cast<std::int32_t>(
            std::ceil(static_cast<double>(sampleRate) * ReplayGain::RmsWindowTime));
    }

    void TrackGain::AnalyzeSamples(
        std::span<const std::int32_t> leftSamples,
        std::span<const std::int32_t> rightSamples)
    {
        if (leftSamples.size() != rightSamples.size())
            throw std::invalid_argument("leftSamples must be as big as rightSamples");

        const std::int32_t numSamples = static_cast<std::int32_t>(leftSamples.size());

        std::vector<double> leftDouble(static_cast<std::size_t>(numSamples));
        std::vector<double> rightDouble(static_cast<std::size_t>(numSamples));

        if (sampleSize == 16)
        {
            if (AvxIsSupported())
            {
                const std::int32_t len = numSamples / 4;
                for (std::int32_t i = 0; i < len; ++i)
                {
#if defined(__i386__) || defined(__x86_64__) || defined(_M_IX86) || defined(_M_X64)
                    const std::ptrdiff_t offset = static_cast<std::ptrdiff_t>(i) * 4;
                    ConvertFourInt32ToDoubleAvx(leftSamples.data() + offset, leftDouble.data() + offset);
                    ConvertFourInt32ToDoubleAvx(rightSamples.data() + offset, rightDouble.data() + offset);
#endif
                }
                std::int32_t offset = len * 4;
                if (offset != numSamples)
                {
                    for (; offset < numSamples; ++offset)
                    {
                        const std::size_t index = static_cast<std::size_t>(offset);
                        leftDouble[index] = leftSamples[index];
                        rightDouble[index] = rightSamples[index];
                    }
                }
            }
            else
            {
                for (std::int32_t i = 0; i < numSamples; ++i)
                {
                    const std::size_t index = static_cast<std::size_t>(i);
                    leftDouble[index] = leftSamples[index];
                    rightDouble[index] = rightSamples[index];
                }
            }
        }
        else if (sampleSize == 24)
        {
            for (std::int32_t i = 0; i < numSamples; ++i)
            {
                const std::size_t index = static_cast<std::size_t>(i);
                leftDouble[index] = static_cast<double>(leftSamples[index]) * ReplayGain::Factor24Bit;
                rightDouble[index] = static_cast<double>(rightSamples[index]) * ReplayGain::Factor24Bit;
            }
        }
        else
        {
            throw std::logic_error("Invalid operation.");
        }

        for (std::int32_t i = 0; i < numSamples; ++i)
        {
            const std::size_t index = static_cast<std::size_t>(i);
            double tmpPeak = std::abs(leftDouble[index]);
            if (tmpPeak > GainData.PeakSample())
                GainData.PeakSample(tmpPeak);

            tmpPeak = std::abs(rightDouble[index]);
            if (tmpPeak > GainData.PeakSample())
                GainData.PeakSample(tmpPeak);
        }

        AnalyzeSamples(leftDouble, rightDouble);
    }

    double TrackGain::Sqr(double d) noexcept
    {
        return d * d;
    }

    void TrackGain::FilterYule(
        std::span<const double> input,
        std::span<double> output,
        std::int32_t& inPos,
        std::int32_t& outPos,
        std::int64_t nSamples,
        std::span<const double> aKernel,
        std::span<const double> bKernel)
    {
        while (nSamples-- != 0)
        {
            const std::size_t inIndex = static_cast<std::size_t>(inPos);
            const std::size_t outIndex = static_cast<std::size_t>(outPos);
            output[outIndex] = 1e-10 +
                input[inIndex] * bKernel[0] - output[outIndex - 1] * aKernel[1] +
                input[inIndex - 1] * bKernel[1] - output[outIndex - 2] * aKernel[2] +
                input[inIndex - 2] * bKernel[2] - output[outIndex - 3] * aKernel[3] +
                input[inIndex - 3] * bKernel[3] - output[outIndex - 4] * aKernel[4] +
                input[inIndex - 4] * bKernel[4] - output[outIndex - 5] * aKernel[5] +
                input[inIndex - 5] * bKernel[5] - output[outIndex - 6] * aKernel[6] +
                input[inIndex - 6] * bKernel[6] - output[outIndex - 7] * aKernel[7] +
                input[inIndex - 7] * bKernel[7] - output[outIndex - 8] * aKernel[8] +
                input[inIndex - 8] * bKernel[8] - output[outIndex - 9] * aKernel[9] +
                input[inIndex - 9] * bKernel[9] - output[outIndex - 10] * aKernel[10] +
                input[inIndex - 10] * bKernel[10];
            ++outPos;
            ++inPos;
        }
    }

    void TrackGain::FilterButter(
        std::span<const double> input,
        std::span<double> output,
        std::int32_t& inPos,
        std::int32_t& outPos,
        std::int64_t nSamples,
        std::span<const double> aKernel,
        std::span<const double> bKernel)
    {
        while (nSamples-- != 0)
        {
            const std::size_t inIndex = static_cast<std::size_t>(inPos);
            const std::size_t outIndex = static_cast<std::size_t>(outPos);
            output[outIndex] =
                input[inIndex] * bKernel[0] - output[outIndex - 1] * aKernel[1] +
                input[inIndex - 1] * bKernel[1] - output[outIndex - 2] * aKernel[2] +
                input[inIndex - 2] * bKernel[2];
            ++outPos;
            ++inPos;
        }
    }

    void TrackGain::AnalyzeSamples(
        std::span<const double> leftSamples,
        std::span<const double> rightSamples)
    {
        const std::int32_t numSamples = static_cast<std::int32_t>(leftSamples.size());

        std::int64_t batchSamples = numSamples;
        std::int64_t curSamplePos = 0;

        if (numSamples < ReplayGain::MaxOrder)
        {
            std::copy(
                leftSamples.begin(),
                leftSamples.end(),
                lInPreBuf.begin() + ReplayGain::MaxOrder);
            std::copy(
                rightSamples.begin(),
                rightSamples.end(),
                rInPreBuf.begin() + ReplayGain::MaxOrder);
        }
        else
        {
            std::copy_n(leftSamples.begin(), ReplayGain::MaxOrder, lInPreBuf.begin() + ReplayGain::MaxOrder);
            std::copy_n(rightSamples.begin(), ReplayGain::MaxOrder, rInPreBuf.begin() + ReplayGain::MaxOrder);
        }

        while (batchSamples > 0)
        {
            std::int64_t curSamples = batchSamples > sampleWindow - totSamp
                ? sampleWindow - totSamp
                : batchSamples;
            std::int32_t curLeftPos;
            std::int32_t curRightPos;
            std::span<const double> curLeft;
            std::span<const double> curRight;
            if (curSamplePos < ReplayGain::MaxOrder)
            {
                curLeftPos = lInPrePos + static_cast<std::int32_t>(curSamplePos);
                curRightPos = rInPrePos + static_cast<std::int32_t>(curSamplePos);
                curLeft = lInPreBuf;
                curRight = rInPreBuf;
                if (curSamples > ReplayGain::MaxOrder - curSamplePos)
                    curSamples = ReplayGain::MaxOrder - curSamplePos;
            }
            else
            {
                curLeftPos = curRightPos = static_cast<std::int32_t>(curSamplePos);
                curLeft = leftSamples;
                curRight = rightSamples;
            }

            std::int32_t outPos = lStepPos + static_cast<std::int32_t>(totSamp);
            FilterYule(
                curLeft,
                lStepBuf,
                curLeftPos,
                outPos,
                curSamples,
                *freqInfo->AYule(),
                *freqInfo->BYule());
            outPos = rStepPos + static_cast<std::int32_t>(totSamp);
            FilterYule(
                curRight,
                rStepBuf,
                curRightPos,
                outPos,
                curSamples,
                *freqInfo->AYule(),
                *freqInfo->BYule());

            std::int32_t inPos = lStepPos + static_cast<std::int32_t>(totSamp);
            outPos = lOutPos + static_cast<std::int32_t>(totSamp);
            FilterButter(
                lStepBuf,
                lOutBuf,
                inPos,
                outPos,
                curSamples,
                *freqInfo->AButter(),
                *freqInfo->BButter());
            inPos = rStepPos + static_cast<std::int32_t>(totSamp);
            outPos = rOutPos + static_cast<std::int32_t>(totSamp);
            FilterButter(
                rStepBuf,
                rOutBuf,
                inPos,
                outPos,
                curSamples,
                *freqInfo->AButter(),
                *freqInfo->BButter());

            curLeft = lOutBuf;
            curLeftPos = lOutPos + static_cast<std::int32_t>(totSamp);
            curRight = rOutBuf;
            curRightPos = rOutPos + static_cast<std::int32_t>(totSamp);

            for (std::int64_t i = curSamples % 16; i-- != 0;)
            {
                lSum += Sqr(curLeft[static_cast<std::size_t>(curLeftPos++)]);
                rSum += Sqr(curRight[static_cast<std::size_t>(curRightPos++)]);
            }

            if (AvxIsSupported())
            {
#if defined(__i386__) || defined(__x86_64__) || defined(_M_IX86) || defined(_M_X64)
                const double* curLeftVec = curLeft.data() + curLeftPos;
                const double* curRightVec = curRight.data() + curRightPos;
                curLeftPos = curRightPos = 0;
                for (std::int64_t i = curSamples / 16; i-- != 0;)
                {
                    lSum += Sum16Avx(curLeftVec + static_cast<std::ptrdiff_t>(curLeftPos) * 4);
                    curLeftPos += 4;
                    rSum += Sum16Avx(curRightVec + static_cast<std::ptrdiff_t>(curRightPos) * 4);
                    curRightPos += 4;
                }
#endif
            }
            else if (Sse2IsSupported())
            {
#if defined(__i386__) || defined(__x86_64__) || defined(_M_IX86) || defined(_M_X64)
                for (std::int64_t i = curSamples / 16; i-- != 0;)
                {
                    lSum += Sum16Sse2(curLeft.data() + curLeftPos);
                    curLeftPos += 4;
                    rSum += Sum16Sse2(curRight.data() + curRightPos);
                    curRightPos += 4;
                }
#endif
            }
            else
            {
                for (std::int64_t i = curSamples / 16; i-- != 0;)
                {
                    const std::size_t leftIndex = static_cast<std::size_t>(curLeftPos);
                    lSum +=
                        Sqr(curLeft[leftIndex]) + Sqr(curLeft[leftIndex + 1]) +
                        Sqr(curLeft[leftIndex + 2]) + Sqr(curLeft[leftIndex + 3]) +
                        Sqr(curLeft[leftIndex + 4]) + Sqr(curLeft[leftIndex + 5]) +
                        Sqr(curLeft[leftIndex + 6]) + Sqr(curLeft[leftIndex + 7]) +
                        Sqr(curLeft[leftIndex + 8]) + Sqr(curLeft[leftIndex + 9]) +
                        Sqr(curLeft[leftIndex + 10]) + Sqr(curLeft[leftIndex + 11]) +
                        Sqr(curLeft[leftIndex + 12]) + Sqr(curLeft[leftIndex + 13]) +
                        Sqr(curLeft[leftIndex + 14]) + Sqr(curLeft[leftIndex + 15]);
                    curLeftPos += 16;

                    const std::size_t rightIndex = static_cast<std::size_t>(curRightPos);
                    rSum +=
                        Sqr(curRight[rightIndex]) + Sqr(curRight[rightIndex + 1]) +
                        Sqr(curRight[rightIndex + 2]) + Sqr(curRight[rightIndex + 3]) +
                        Sqr(curRight[rightIndex + 4]) + Sqr(curRight[rightIndex + 5]) +
                        Sqr(curRight[rightIndex + 6]) + Sqr(curRight[rightIndex + 7]) +
                        Sqr(curRight[rightIndex + 8]) + Sqr(curRight[rightIndex + 9]) +
                        Sqr(curRight[rightIndex + 10]) + Sqr(curRight[rightIndex + 11]) +
                        Sqr(curRight[rightIndex + 12]) + Sqr(curRight[rightIndex + 13]) +
                        Sqr(curRight[rightIndex + 14]) + Sqr(curRight[rightIndex + 15]);
                    curRightPos += 16;
                }
            }

            batchSamples -= curSamples;
            curSamplePos += curSamples;
            totSamp += curSamples;
            if (totSamp == sampleWindow)
            {
                const double val = ReplayGain::StepsPerDb * 10 *
                    std::log10((lSum + rSum) / static_cast<double>(totSamp) * 0.5 + 1e-37);
                const double clamped = std::clamp(
                    val,
                    0.0,
                    static_cast<double>(GainData.Accum().size() - 1));
                const std::int32_t ival = static_cast<std::int32_t>(clamped);
                IncrementUnchecked(GainData.Accum()[static_cast<std::size_t>(ival)]);
                lSum = rSum = 0.0;

                if (totSamp > std::numeric_limits<std::int32_t>::max())
                    throw std::overflow_error("Too many samples! Change to long and recompile!");

                const std::size_t sampleOffset = static_cast<std::size_t>(totSamp);
                const std::size_t historyBytes = static_cast<std::size_t>(ReplayGain::MaxOrder) * sizeof(double);
                std::memmove(lOutBuf.data(), lOutBuf.data() + sampleOffset, historyBytes);
                std::memmove(rOutBuf.data(), rOutBuf.data() + sampleOffset, historyBytes);
                std::memmove(lStepBuf.data(), lStepBuf.data() + sampleOffset, historyBytes);
                std::memmove(rStepBuf.data(), rStepBuf.data() + sampleOffset, historyBytes);

                totSamp = 0;
            }
            if (totSamp > sampleWindow)
                throw std::runtime_error("Gain analysis error!");
        }

        if (numSamples < ReplayGain::MaxOrder)
        {
            const std::size_t count = static_cast<std::size_t>(ReplayGain::MaxOrder - numSamples);
            const std::size_t sourceOffset = static_cast<std::size_t>(numSamples);
            std::memmove(lInPreBuf.data(), lInPreBuf.data() + sourceOffset, count * sizeof(double));
            std::memmove(rInPreBuf.data(), rInPreBuf.data() + sourceOffset, count * sizeof(double));

            std::copy(
                leftSamples.begin(),
                leftSamples.end(),
                lInPreBuf.begin() + (ReplayGain::MaxOrder - numSamples));
            std::copy(
                rightSamples.begin(),
                rightSamples.end(),
                rInPreBuf.begin() + (ReplayGain::MaxOrder - numSamples));
        }
        else
        {
            std::copy_n(
                leftSamples.end() - ReplayGain::MaxOrder,
                ReplayGain::MaxOrder,
                lInPreBuf.begin());
            std::copy_n(
                rightSamples.end() - ReplayGain::MaxOrder,
                ReplayGain::MaxOrder,
                rInPreBuf.begin());
        }
    }

    double TrackGain::GetGain()
    {
        return ReplayGain::AnalyzeResult(GainData.Accum());
    }

    double TrackGain::GetPeak() const noexcept
    {
        return GainData.PeakSample() / ReplayGain::MaxSampleValue;
    }
}
