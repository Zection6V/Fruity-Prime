#include "SWAVWrapper.hpp"

#include "Channel.hpp"
#include "../NC/SWAV.hpp"

#include <algorithm>
#include <bit>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <limits>
#include <memory>
#include <stdexcept>

namespace
{
    [[nodiscard]] constexpr std::int32_t WrapAdd32(
        std::int32_t left,
        std::int32_t right) noexcept
    {
        return std::bit_cast<std::int32_t>(
            static_cast<std::uint32_t>(left) + static_cast<std::uint32_t>(right));
    }

    [[nodiscard]] constexpr std::int32_t ToInt32Unchecked(std::uint32_t value) noexcept
    {
        return std::bit_cast<std::int32_t>(value);
    }

    [[nodiscard]] std::int32_t ToInt32Unchecked(double value) noexcept
    {
        constexpr double MinValue = -2147483648.0;
        constexpr double MaxValueExclusive = 2147483648.0;
        if (!std::isfinite(value) || value < MinValue || value >= MaxValueExclusive)
            return std::numeric_limits<std::int32_t>::min();
        return static_cast<std::int32_t>(value);
    }

    [[nodiscard]] std::shared_ptr<NCSFCommon::NC::SWAV> SourceOrThrow(
        NCSFCommon::NDSSoundRegister* soundRegister)
    {
        if (soundRegister == nullptr)
            throw std::runtime_error("NullReferenceException");
        std::shared_ptr<NCSFCommon::NC::SWAV> source = soundRegister->Source();
        if (!source)
            throw std::runtime_error("NullReferenceException");
        return source;
    }

    [[nodiscard]] std::span<const float> SpanSlice(
        std::span<const float> span,
        std::int32_t index,
        std::int32_t len)
    {
        if (index < 0 || len < 0)
            throw std::out_of_range("Specified argument was out of the range of valid values.");
        const std::size_t offset = static_cast<std::size_t>(index);
        const std::size_t count = static_cast<std::size_t>(len);
        if (offset > span.size() || count > span.size() - offset)
            throw std::out_of_range("Specified argument was out of the range of valid values.");
        return span.subspan(offset, count);
    }

    void CopyTo(std::span<const float> source, std::span<float> destination)
    {
        if (source.size() > destination.size())
            throw std::invalid_argument("Destination is too short.");
        std::copy(source.begin(), source.end(), destination.begin());
    }
}

namespace NCSFPlayer
{
    SWAVWrapper::SWAVWrapper(NCSFCommon::NDSSoundRegister* registerValue)
    {
        const auto sourceForLength = SourceOrThrow(registerValue);
        const std::span<const float> sourceDataForLength = sourceForLength->Data();
        const std::int32_t sourceLength = static_cast<std::int32_t>(sourceDataForLength.size());
        const std::int32_t dataLength = WrapAdd32(sourceLength, 2 * Channel::SincWidth);
        if (dataLength < 0)
            throw std::out_of_range("Specified argument was out of the range of valid values.");
        data.resize(static_cast<std::size_t>(dataLength));

        std::span<float> dataSpan(data.data(), data.size());

        const auto sourceForFirstSample = SourceOrThrow(registerValue);
        const std::span<const float> sourceDataForFirstSample = sourceForFirstSample->Data();
        if (sourceDataForFirstSample.empty())
            throw std::out_of_range("Index was outside the bounds of the array.");
        std::fill_n(dataSpan.begin(), static_cast<std::size_t>(Channel::SincWidth), sourceDataForFirstSample[0]);

        const auto sourceForCopy = SourceOrThrow(registerValue);
        CopyTo(sourceForCopy->Data(), dataSpan.subspan(static_cast<std::size_t>(Channel::SincWidth)));

        if (registerValue->RepeatMode() == 1U)
        {
            const auto sourceForLoop = SourceOrThrow(registerValue);
            CopyTo(
                SpanSlice(sourceForLoop->Data(), ToInt32Unchecked(registerValue->LoopStart()), Channel::SincWidth),
                dataSpan.last(static_cast<std::size_t>(Channel::SincWidth)));
        }
        else
        {
            const std::span<float> tail = dataSpan.last(static_cast<std::size_t>(Channel::SincWidth));
            std::fill(tail.begin(), tail.end(), 0.0F);
        }

        soundRegister = registerValue;
    }

    std::span<const float> SWAVWrapper::Slice(std::int32_t index, std::int32_t len) const
    {
        const std::int32_t samplePosition = ToInt32Unchecked(soundRegister->SamplePosition());
        const std::int32_t offset = WrapAdd32(WrapAdd32(samplePosition, index), Channel::SincWidth);
        return SpanSlice(std::span<const float>(data.data(), data.size()), offset, len);
    }
}
